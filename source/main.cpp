#include "Receiver.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <csignal>
#include <cstdlib>

// Global flag to handle graceful shutdown
std::atomic<bool> keepRunning(true);

// Signal handler to handle Ctrl+C
void signalHandler(int signal) {
    std::cout << "Received stop signal" << std::endl;
    if (signal == SIGINT) {
        keepRunning = false;
    }
}

int main(int argc, char* argv[]) {
    // Default configuration
    int intervalMs = 1000;       // Period in milliseconds
    size_t numEvents = 10;       // Number of events to retrieve

    // Parse command-line arguments
    if (argc > 1) {
        intervalMs = std::atoi(argv[1]);
    }
    if (argc > 2) {
        numEvents = std::atoi(argv[2]);
    }

    std::cout << "Starting Receiver with interval " << intervalMs 
              << " ms and retrieving " << numEvents << " events per iteration." << std::endl;

    // Set up signal handling for graceful shutdown
    std::signal(SIGINT, signalHandler);

    // Create and start the Receiver
    Receiver& receiver = Receiver::getInstance();
    receiver.init(
        "",                      // Set the host name
        "BUF001",                // Set the buffer name to "BUF001"
        "Event Receiver",        // Set the client name
        EVENTID_ALL,             // Set the event ID to request all events
        true,                    // Set to get all events
        1000                     // Set the maximum buffer size
    );                   
    receiver.start();

    // Periodically retrieve and print events
    while (receiver.isListeningForEvents()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));

        auto events = receiver.getNLatest(numEvents);
        if (events.empty()) {
            std::cout << "[INFO] No new events in the buffer." << std::endl;
        } else {
            for (const auto& event : events) {
                std::cout << "[EVENT] Received event with size: " << event.size() << " bytes" << std::endl;
            }
        }
    }


    std::cout << "Stoping Receiver" << std::endl;
    receiver.stop();
    std::cout << "Exiting Program" << std::endl;

    return 0;
}
