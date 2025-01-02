#include "MidasReceiver.h"
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

    std::cout << "Starting Receiver with interval " << intervalMs 
              << " ms and retrieving " << numEvents << " events per iteration." << std::endl;

    // Create and start the Receiver
    MidasReceiver& midasReceiver = MidasReceiver::getInstance();
    midasReceiver.init(
        "",                   // Host name (default is empty, resolved via cm_get_environment)
        "",                   // Expt name (default is empty, resolved via cm_get_environment)
        "BUF001",             // Buffer name to use
        "Event Receiver",     // Client name to register
        EVENTID_ALL,          // Event ID to request (default: all events)
        true,                 // Whether to get all events (default: true)
        1000,                 // Maximum buffer size
        300                   // Timeout for cm_yield (default: 300 ms)
    );

    // Initialize the timestamp tracking
    auto lastTimestamp = std::chrono::system_clock::now();

    midasReceiver.start();

    // Periodically retrieve and print events
    while (midasReceiver.isListeningForEvents()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));

        // Retrieve the latest events after the last timestamp
        auto timedEvents = midasReceiver.getLatestEvents(numEvents, lastTimestamp);
        
        if (timedEvents.empty()) {
            std::cout << "[INFO] No new events in the buffer." << std::endl;
        } else {
            std::vector<TMEvent> events;
            std::vector<std::chrono::system_clock::time_point> timestamps;

            // Unpack events and timestamps
            for (const auto& timedEvent : timedEvents) {
                events.push_back(timedEvent.event); // Unpack TMEvent
                timestamps.push_back(timedEvent.timestamp); // Unpack timestamp
            }

            // Print the events and their details
            for (size_t i = 0; i < events.size(); ++i) {
                const auto& event = events[i];
                const auto& timestamp = timestamps[i];

                // Print event details
                std::cout << "[EVENT] Received event with size: " << event.data_size << " bytes" << std::endl;
                std::cout << "  Event ID: " << event.event_id << std::endl;
                std::cout << "  Trigger Mask: " << event.trigger_mask << std::endl;
                std::cout << "  Serial Number: " << event.serial_number << std::endl;
                std::cout << "  Time Stamp: " << std::chrono::system_clock::to_time_t(timestamp) << std::endl;
                std::cout << "  Data Size: " << event.data_size << " bytes" << std::endl;
                std::cout << "  Event Header Size: " << event.event_header_size << " bytes" << std::endl;
                std::cout << "  Bank Header Flags: " << event.bank_header_flags << std::endl;

                if (!event.banks.empty()) {
                    std::cout << "  Found " << event.banks.size() << " banks in the event:" << std::endl;
                    for (const auto& bank : event.banks) {
                        std::cout << "    Bank Name: " << bank.name << std::endl;
                        std::cout << "    Bank Size: " << bank.data_size << " bytes" << std::endl;
                    }
                }

                // Optionally, print event data (if small enough)
                std::cout << "  Event Data (first 32 bytes): ";
                for (size_t j = 0; j < std::min(event.data.size(), size_t(32)); ++j) {
                    std::cout << std::hex << (event.data[j] & 0xFF) << " ";
                }
                std::cout << std::dec << std::endl;
            }

            // Update the timestamp to the most recent event timestamp (for the next iteration)
            lastTimestamp = timestamps.back();
        }
    }

    std::cout << "Stopping Receiver" << std::endl;
    midasReceiver.stop();
    std::cout << "Exiting Program" << std::endl;

    return 0;
}
