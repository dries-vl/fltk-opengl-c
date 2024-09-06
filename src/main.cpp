#include <FL/Fl.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

// OpenGL drawing function
void draw_scene() {
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1.0, 0.0, 0.0);
    glBegin(GL_QUADS);
    glVertex2f(-0.5, -0.5);
    glVertex2f(0.5, -0.5);
    glVertex2f(0.5, 0.5);
    glVertex2f(-0.5, 0.5);
    glEnd();
}

// FLTK GL Window class
class MyGLWindow : public Fl_Gl_Window {
public:
    MyGLWindow(int x, int y, int w, int h, const char* l = 0) : Fl_Gl_Window(x, y, w, h, l) {}

    void draw() {
        if (!valid()) {
            valid(1);
            glViewport(0, 0, w(), h());
        }
        draw_scene();
    }
};

int perform_http_post(const char *url, const char *post_data) {
    CURL *curl;
    CURLcode res;

    // Initialize libcurl
    curl = curl_easy_init();
    if (curl) {
        // Set the URL
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Set the POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

        // Perform the request, res will get the return code
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return 1; // Return error code if request fails
        }

        // Cleanup after the request
        curl_easy_cleanup(curl);
        return 0; // Success
    } else {
        fprintf(stderr, "Failed to initialize libcurl\n");
        return 1; // Return error if initialization fails
    }
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>  // for sleep function

// Write callback function to capture the response data
size_t write_callback(void *ptr, size_t size, size_t nmemb, char *data) {
    strcat(data, (char *)ptr);
    return size * nmemb;
}

// Perform a POST request and get the status and response URLs
int submit_request(const char *url, const char *api_key, const char *json_data, char *status_url, char *response_url) {
    CURL *curl;
    CURLcode res;
    char response[4096] = {0};  // Buffer to store the server's response

    curl = curl_easy_init();
    if (curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, api_key);
        headers = curl_slist_append(headers, "User-Agent: fal-client/0.2.2 (C)");

        // Set up the POST request
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

        // Perform the POST request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "POST request failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return 1;
        }

        // Parse the response to extract the status_url and response_url
        sscanf(response, "{\"status_url\":\"%[^\"]\",\"response_url\":\"%[^\"]\"}", status_url, response_url);

        curl_easy_cleanup(curl);
        return 0;  // Success
    } else {
        fprintf(stderr, "Failed to initialize curl\n");
        return 1;
    }
}

// Poll the status until the task is complete
int poll_status(const char *status_url, const char *api_key) {
    CURL *curl;
    CURLcode res;
    char response[4096] = {0};  // Buffer to store the server's response

    curl = curl_easy_init();
    if (curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, api_key);
        headers = curl_slist_append(headers, "User-Agent: fal-client/0.2.2 (C)");

        // Set up the GET request for polling
        curl_easy_setopt(curl, CURLOPT_URL, status_url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

        // Polling loop
        while (1) {
            memset(response, 0, sizeof(response));  // Clear response buffer
            res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                fprintf(stderr, "Polling failed: %s\n", curl_easy_strerror(res));
                curl_easy_cleanup(curl);
                return 1;
            }

            // Check the status from the response
            if (strstr(response, "\"status\":\"COMPLETED\"")) {
                break;  // Task is completed
            } else if (strstr(response, "\"status\":\"IN_QUEUE\"")) {
                int queue_position;
                sscanf(response, "{\"queue_position\":%d}", &queue_position);
                printf("Request is in queue at position %d\n", queue_position);
            } else if (strstr(response, "\"status\":\"IN_PROGRESS\"")) {
                printf("Request is in progress, waiting...\n");
            }

            sleep(1);  // Wait for 1 second before polling again
        }

        curl_easy_cleanup(curl);
        return 0;  // Success
    } else {
        fprintf(stderr, "Failed to initialize curl for polling\n");
        return 1;
    }
}

// Retrieve the final result
int retrieve_result(const char *response_url, const char *api_key) {
    CURL *curl;
    CURLcode res;
    char response[4096] = {0};  // Buffer to store the server's response

    curl = curl_easy_init();
    if (curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, api_key);
        headers = curl_slist_append(headers, "User-Agent: fal-client/0.2.2 (C)");

        // Set up the GET request to retrieve the result
        curl_easy_setopt(curl, CURLOPT_URL, response_url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

        // Perform the GET request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "Failed to retrieve the result: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return 1;
        }

        // Print the final result
        printf("Final result: %s\n", response);

        curl_easy_cleanup(curl);
        return 0;  // Success
    } else {
        fprintf(stderr, "Failed to initialize curl for result retrieval\n");
        return 1;
    }
}

// Main program logic
void perform_request() {
    const char *submit_url = "https://queue.fal.run/fal-ai/flux/dev";
    const char *api_key = "Authorization: Key 5ff46e36-d30e-4609-ab5a-c4c6fb0d1297:79e9099d823568f450be54e8f4a12c9e";
    const char *payload = "{\"prompt\": \"A 2000s photograph of a Roman bust next to a swimming pool in a Southern French villa\"}";

    char status_url[512] = {0};
    char response_url[512] = {0};

    // Step 1: Submit the request
    if (submit_request(submit_url, api_key, payload, status_url, response_url) != 0) {
        return;
    }

    // Step 2: Poll for status until the request is completed
    if (poll_status(status_url, api_key) != 0) {
        return;
    }

    // Step 3: Retrieve the result
    retrieve_result(response_url, api_key);
}

int main(int argc, char **argv) {
    Fl_Window *window = new Fl_Window(300, 300, "FLTK OpenGL Example");
    MyGLWindow *gl_window = new MyGLWindow(10, 10, 280, 280);
    window->end();
    window->show(argc, argv);
    perform_request();
    Fl::run();

}