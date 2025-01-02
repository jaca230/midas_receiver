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

    // Get the singleton instance of MidasEventReceiver for "BUF001"
    std::string midasEventBufferName1 = "BUF001"; // First buffer name
    MidasEventReceiver* midasEventReceiver1 = MidasEventReceiver::getInstance(midasEventBufferName1);

    // Get the singleton instance of MidasEventReceiver for "BUF"
    std::string midasEventBufferName2 = "BUF"; // Second buffer name
    MidasEventReceiver* midasEventReceiver2 = MidasEventReceiver::getInstance(midasEventBufferName2);

    // Initialize the receivers with necessary parameters
    midasEventReceiver1->init(
        "",                   // Host name (default is empty, resolved via cm_get_environment)
        "DAQ",
        "Event Receiver BUF001",  // Client name to register
        EVENTID_ALL,          // Event ID to request (default: all events)
        true,                 // Whether to get all events (default: true)
        1000,                 // Maximum buffer size
        300                   // Timeout for cm_yield (default: 300 ms)
    );
    midasEventReceiver2->init(
        "",                   // Host name (default is empty, resolved via cm_get_environment)
        "DAQ",
        "Event Receiver BUF",   // Client name to register
        EVENTID_ALL,          // Event ID to request (default: all events)
        true,                 // Whether to get all events (default: true)
        1000,                 // Maximum buffer size
        300                   // Timeout for cm_yield (default: 300 ms)
    );

    // Initialize the timestamp tracking
    auto lastTimestamp1 = std::chrono::system_clock::now();
    auto lastTimestamp2 = std::chrono::system_clock::now();

    // Start receiving events for both buffers
    std::cout << "Starting to listen for events..." << std::endl;
    midasEventReceiver1->start();
    midasEventReceiver2->start();

    // Periodically retrieve and print events
    while (midasEventReceiver1->isListeningForData() || midasEventReceiver2->isListeningForData()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));

        // Retrieve the latest events from both receivers after their respective timestamps
        if (midasEventReceiver1->isListeningForData()) {
            auto events1 = midasEventReceiver1->getLatestEvents(numEvents, lastTimestamp1);
            if (!events1.empty()) {
                std::vector<TMEvent> unpackedEvents1;
                std::vector<std::chrono::system_clock::time_point> timestamps1;
                for (const auto& timedEvent : events1) {
                    unpackedEvents1.push_back(timedEvent->event);
                    timestamps1.push_back(timedEvent->timestamp);
                }

                // Print the details of the retrieved events for BUF001
                for (size_t i = 0; i < unpackedEvents1.size(); ++i) {
                    const auto& event = unpackedEvents1[i];
                    const auto& timestamp = timestamps1[i];
                    std::cout << "[EVENT] Buffer: " << midasEventBufferName1 << std::endl;
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

                    std::cout << "  Event Data (first 32 bytes): ";
                    for (size_t j = 0; j < std::min(event.data.size(), size_t(32)); ++j) {
                        std::cout << std::hex << (event.data[j] & 0xFF) << " ";
                    }
                    std::cout << std::dec << std::endl;
                }

                // Update the timestamp for BUF001
                lastTimestamp1 = timestamps1.back();
            }
        }

        if (midasEventReceiver2->isListeningForData()) {
            auto events2 = midasEventReceiver2->getLatestEvents(numEvents, lastTimestamp2);
            if (!events2.empty()) {
                std::vector<TMEvent> unpackedEvents2;
                std::vector<std::chrono::system_clock::time_point> timestamps2;
                for (const auto& timedEvent : events2) {
                    unpackedEvents2.push_back(timedEvent->event);
                    timestamps2.push_back(timedEvent->timestamp);
                }

                // Print the details of the retrieved events for BUF
                for (size_t i = 0; i < unpackedEvents2.size(); ++i) {
                    const auto& event = unpackedEvents2[i];
                    const auto& timestamp = timestamps2[i];
                    std::cout << "[EVENT] Buffer: " << midasEventBufferName2 << std::endl;
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

                    std::cout << "  Event Data (first 32 bytes): ";
                    for (size_t j = 0; j < std::min(event.data.size(), size_t(32)); ++j) {
                        std::cout << std::hex << (event.data[j] & 0xFF) << " ";
                    }
                    std::cout << std::dec << std::endl;
                }

                // Update the timestamp for BUF
                lastTimestamp2 = timestamps2.back();
            }
        }
    }

    // Stop the receivers once done
    std::cout << "Stopping Receiver" << std::endl;
    midasEventReceiver1->stop();
    midasEventReceiver2->stop();
    std::cout << "Exiting Program" << std::endl;

    return 0;
}
