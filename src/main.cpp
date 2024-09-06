#include <FL/Fl.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <cpr/cpr.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

// Function to perform the HTTP POST request, poll status, and retrieve the result
void perform_request() {
    const std::string submit_url = "https://queue.fal.run/fal-ai/flux/dev";
    const std::string api_key = "Key 5ff46e36-d30e-4609-ab5a-c4c6fb0d1297:79e9099d823568f450be54e8f4a12c9e";

    // Headers for the request
    cpr::Header headers = {
        {"Authorization", api_key},
        {"User-Agent", "fal-client/0.2.2 (cpp)"},
        {"Content-Type", "application/json"}
    };

    // Step 1: Submit the request
    nlohmann::json payload = {
        {"prompt", "A 2000s photograph of a Roman bust next to a swimming pool in a Southern French villa"}
    };

    cpr::Response submit_response = cpr::Post(cpr::Url{submit_url}, headers, cpr::Body{payload.dump()});

    if (submit_response.status_code != 200) {
        std::cerr << "Failed to submit request: " << submit_response.status_code << "\n";
        return;
    }

    // Parse the submit response to get the status and response URLs
    auto submit_data = nlohmann::json::parse(submit_response.text);
    std::string status_url = submit_data["status_url"];
    std::string response_url = submit_data["response_url"];

    // Step 2: Poll for status until the request is completed
    while (true) {
        cpr::Response status_response = cpr::Get(cpr::Url{status_url}, headers);
        auto status_data = nlohmann::json::parse(status_response.text);

        std::string status = status_data["status"];
        if (status == "COMPLETED") {
            break;
        } else if (status == "IN_PROGRESS") {
            std::cout << "Request in progress, waiting...\n";
        } else if (status == "IN_QUEUE") {
            int queue_position = status_data["queue_position"];
            std::cout << "Request is in queue at position " << queue_position << "\n";
        }

        // Sleep for a short interval before polling again
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Step 3: Retrieve the final result
    cpr::Response result_response = cpr::Get(cpr::Url{response_url}, headers);
    auto result_data = nlohmann::json::parse(result_response.text);

    // Print the result
    std::cout << "Final result: " << result_data.dump(4) << "\n";
}

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

int main(int argc, char **argv) {
    Fl_Window *window = new Fl_Window(300, 300, "FLTK OpenGL Example");
    MyGLWindow *gl_window = new MyGLWindow(10, 10, 280, 280);
    window->end();
    window->show(argc, argv);
    perform_request();
    Fl::run();
}