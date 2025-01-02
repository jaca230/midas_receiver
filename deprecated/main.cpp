#include "MidasEventReceiver.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <csignal>
#include <cstdlib>

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

    std::cout << "Starting Event Receiver with interval " << intervalMs 
              << " ms and retrieving " << numEvents << " events per iteration." << std::endl;

    // Get the singleton instance of MidasEventReceiver
    std::string midasEventBufferName = "BUF001"; // Example buffer name
    MidasEventReceiver* midasEventReceiver = MidasEventReceiver::getInstance(midasEventBufferName);

    // Initialize the receiver with necessary parameters
    midasEventReceiver->init(
        "",                   // Host name (default is empty, resolved via cm_get_environment)
        "Event Receiver",     // Client name to register
        EVENTID_ALL,          // Event ID to request (default: all events)
        true,                 // Whether to get all events (default: true)
        1000,                 // Maximum buffer size
        300                   // Timeout for cm_yield (default: 300 ms)
    );

    // Initialize the timestamp tracking
    auto lastTimestamp = std::chrono::system_clock::now();

    // Start receiving events
    std::cout << "Starting to listen for events..." << std::endl;
    midasEventReceiver->start();

    // Periodically retrieve and print events
    while (midasEventReceiver->isListeningForData()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));

        // Retrieve the latest events after the last timestamp
        auto events = midasEventReceiver->getLatestData(numEvents, lastTimestamp);

        if (events.empty()) {
            std::cout << "[INFO] No new events in the buffer." << std::endl;
        } else {
            // Print the details of the retrieved events
            for (size_t i = 0; i < events.size(); ++i) {
                const auto& event = events[i];

                // Print event details
                std::cout << "[EVENT] Received event with size: " << event->getDataSize() << " bytes" << std::endl;
                std::cout << "  Event ID: " << event->getEventID() << std::endl;
                std::cout << "  Serial Number: " << event->getSerialNumber() << std::endl;
                std::cout << "  Time Stamp: " << std::chrono::system_clock::to_time_t(event->getTimestamp()) << std::endl;
                std::cout << "  Data Size: " << event->getDataSize() << " bytes" << std::endl;
                
                // Optionally, print event data (if small enough)
                std::cout << "  Event Data (first 32 bytes): ";
                for (size_t j = 0; j < std::min(event->getData().size(), size_t(32)); ++j) {
                    std::cout << static_cast<int>(event->getData()[j]) << " ";
                }
                std::cout << std::endl;
            }

            // Update the timestamp for the next retrieval
            lastTimestamp = std::chrono::system_clock::now();
        }
    }

    // Stop the receiver once done
    std::cout << "Stopping Receiver" << std::endl;
    midasEventReceiver->stop();
    std::cout << "Exiting Program" << std::endl;

    return 0;
}
