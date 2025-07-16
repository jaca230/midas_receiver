#include "MidasReceiver.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <ctime>

// Helper to format time_point as human-readable string
std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    char buffer[26];
    ctime_r(&t, buffer);
    buffer[24] = '\0'; // Remove newline at end
    return std::string(buffer);
}

int main(int argc, char* argv[]) {
    int intervalMs = 1000;
    size_t numEvents = 1;

    if (argc > 1) {
        intervalMs = std::atoi(argv[1]);
    }
    if (argc > 2) {
        numEvents = std::atoi(argv[2]);
    }

    std::cout << "Starting MidasReceiver with interval " << intervalMs
              << " ms and retrieving " << numEvents << " events per iteration." << std::endl;

    MidasReceiver& midasReceiver = MidasReceiver::getInstance();

    MidasReceiverConfig config;
    config.host = "";
    config.experiment = "";
    config.bufferName = "SYSTEM";
    config.clientName = "Event Receiver";
    config.eventID = EVENTID_ALL;
    config.getAllEvents = true;
    config.maxBufferSize = 1000;
    config.cmYieldTimeout = 300;
    config.transitionRegistrations = {
        {TR_START,      100},
        {TR_STOP,       900},
        {TR_PAUSE,      100},
        {TR_RESUME,     100},
        {TR_STARTABORT, 500}
    };

    midasReceiver.init(config);

    auto lastEventTimestamp = std::chrono::system_clock::now();
    auto lastMessageTimestamp = std::chrono::system_clock::now();
    auto lastTransitionTimestamp = std::chrono::system_clock::now();

    midasReceiver.start();

    while (midasReceiver.isListeningForEvents()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));

        // Retrieve and print events (shared_ptr<TimedEvent>)
        auto timedEvents = midasReceiver.getLatestEvents(numEvents, lastEventTimestamp);
        if (!timedEvents.empty()) {
            std::cout << "\n=== Midas Events (count=" << timedEvents.size() << ") ===" << std::endl;
            for (auto& timedEventPtr : timedEvents) {
                auto& timedEvent = *timedEventPtr;
                TMEvent& event = *timedEvent.event;
                auto& ts = timedEvent.timestamp;

                event.FindAllBanks();

                std::cout << "[EVENT] Timestamp: " << formatTimestamp(ts) << std::endl;
                std::cout << "  Event ID: " << event.event_id << std::endl;
                std::cout << "  Trigger Mask: " << event.trigger_mask << std::endl;
                std::cout << "  Serial Number: " << event.serial_number << std::endl;
                std::cout << "  Data Size: " << event.data_size << " bytes" << std::endl;
                std::cout << "  Event Header Size: " << event.event_header_size << " bytes" << std::endl;
                std::cout << "  Bank Header Flags: " << event.bank_header_flags << std::endl;

                if (!event.banks.empty()) {
                    std::cout << "  Banks (" << event.banks.size() << "):" << std::endl;
                    for (const auto& bank : event.banks) {
                        std::cout << "    Name: " << bank.name << ", Size: " << bank.data_size << " bytes" << std::endl;
                    }
                }

                std::cout << "  Data (first 32 bytes): ";
                for (size_t i = 0; i < std::min(event.data.size(), size_t(32)); ++i) {
                    printf("%02X ", static_cast<unsigned char>(event.data[i]));
                }
                std::cout << std::endl << std::endl;
            }
            lastEventTimestamp = timedEvents.back()->timestamp;
        } else {
            std::cout << "[INFO] No new events." << std::endl;
        }

        // Messages remain unchanged (still by value)
        auto messages = midasReceiver.getLatestMessages(numEvents, lastMessageTimestamp);
        if (!messages.empty()) {
            std::cout << "\n=== Midas Messages (count=" << messages.size() << ") ===" << std::endl;
            for (const auto& msg : messages) {
                std::cout << "Timestamp: " << formatTimestamp(msg.timestamp) << std::endl;
                std::cout << "Message data pointer: " << msg.message << std::endl;
                // Add decoding here if message structure is known
            }
            std::cout << std::endl;
            lastMessageTimestamp = messages.back().timestamp;
        }

        // Transitions remain unchanged (still by value)
        auto transitions = midasReceiver.getLatestTransitions(numEvents, lastTransitionTimestamp);
        if (!transitions.empty()) {
            std::cout << "\n=== Midas Transitions (count=" << transitions.size() << ") ===" << std::endl;
            for (const auto& transition : transitions) {
                std::cout << "Timestamp: " << formatTimestamp(transition.timestamp)
                          << ", Run Number: " << transition.run_number
                          << ", Error: " << transition.error << std::endl;
            }
            std::cout << std::endl;
            lastTransitionTimestamp = transitions.back().timestamp;
        }
    }

    std::cout << "Stopping MidasReceiver..." << std::endl;
    midasReceiver.stop();
    std::cout << "Program exiting." << std::endl;

    return 0;
}
