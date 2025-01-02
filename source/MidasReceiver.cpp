#include "MidasReceiver.h"
#include <cstring>
#include <iostream>

#define MAX_EVENT_SIZE (10 * 1024 * 1024)

// Initialize the static member variables
MidasReceiver* MidasReceiver::instance = nullptr;
std::mutex MidasReceiver::initMutex;

// Constructor (private for singleton)
MidasReceiver::MidasReceiver()
    : hostName(""),
      exptName(""), 
      bufferName("SYSTEM"), 
      clientName("Event Receiver"), 
      eventID(EVENTID_ALL), 
      getAllEvents(true), 
      maxBufferSize(1000),
      cmYieldTimeout(300), 
      running(false) {}

// Destructor
MidasReceiver::~MidasReceiver() {
    stop();
}

// Get the singleton instance
MidasReceiver& MidasReceiver::getInstance() {
    std::lock_guard<std::mutex> lock(initMutex);

    if (instance == nullptr) {
        instance = new MidasReceiver();
    }
    return *instance;
}

// Initialize receiver with parameters
void MidasReceiver::init(const std::string& host,
                         const std::string& expt,
                         const std::string& bufferName, 
                         const std::string& clientName, 
                         int eventID, 
                         bool getAllEvents, 
                         size_t maxBufferSize, 
                         int cmYieldTimeout) {
    if (host.empty()) {
        char host_name[HOST_NAME_LENGTH], expt_name[NAME_LENGTH];
        cm_get_environment(host_name, sizeof(host_name), expt_name, sizeof(expt_name));
        this->hostName = host_name;
    } else {
        this->hostName = host;
    }
    if (expt.empty()) {
        char host_name[HOST_NAME_LENGTH], expt_name[NAME_LENGTH];
        cm_get_environment(host_name, sizeof(host_name), expt_name, sizeof(expt_name));
        this->exptName = expt_name;
    } else {
        this->exptName = expt;
    }

    this->bufferName = bufferName.empty() ? this->bufferName : bufferName;
    this->clientName = clientName.empty() ? this->clientName : clientName;
    this->eventID = eventID;
    this->getAllEvents = getAllEvents;
    this->maxBufferSize = maxBufferSize;
    this->cmYieldTimeout = cmYieldTimeout; // Set cm_yield timeout
}
// Start receiving events
void MidasReceiver::start() {
    if (!running) {
        running = true;
        listeningForEvents = true;
        workerThread = std::thread(&MidasReceiver::run, this);
    }
}

// Stop receiving events
void MidasReceiver::stop() {
    if (running) {
        if (workerThread.joinable()) {
            workerThread.join();
        }
        cm_disconnect_experiment();
        running = false;
    }
}

// Main worker thread
void MidasReceiver::run() {
    status = cm_connect_experiment(hostName.c_str(), exptName.c_str(), clientName.c_str(), nullptr); // Set status during connection

    if (status != CM_SUCCESS) {
        cm_msg(MERROR, "MidasReceiver::run", "Failed to connect to experiment. Status: %d", status);
        listeningForEvents = false;
        return;
    }

    status = bm_open_buffer(bufferName.c_str(), MAX_EVENT_SIZE * 2, &hBufEvent); // Set status for buffer opening
    if (status != BM_SUCCESS) {
        cm_msg(MERROR, "MidasReceiver::run", "Failed to open buffer. Status: %d", status);
        listeningForEvents = false;
        return;
    }

    status = bm_set_cache_size(hBufEvent, 100000, 0); // Update status if cache size fails
    if (status != BM_SUCCESS) {
        cm_msg(MERROR, "MidasReceiver::run", "Failed to set cache size. Status: %d", status);
        listeningForEvents = false;
        return;
    }

    status = bm_request_event(hBufEvent, (WORD)eventID, TRIGGER_ALL, getAllEvents ? GET_ALL : GET_NONBLOCKING,
                              &requestID, MidasReceiver::processEventCallback); // Update status when event request is made

    if (status != BM_SUCCESS) {
        cm_msg(MERROR, "MidasReceiver::run", "Failed to request event. Status: %d", status);
        listeningForEvents = false;
        return;
    }

    while (running && (status != RPC_SHUTDOWN && status != SS_ABORT)) {
        status = cm_yield(1000);
    }

    bm_close_buffer(hBufEvent);
    listeningForEvents = false;

}

// Static callback: process event
void MidasReceiver::processEventCallback(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent) {
    MidasReceiver& receiver = MidasReceiver::getInstance();
    receiver.processEvent(hBuf, request_id, pheader, pevent);
}

// Static callback: process message
void MidasReceiver::processMessageCallback(HNDLE hBuf, HNDLE id, EVENT_HEADER* pheader, void* message) {
    MidasReceiver& receiver = MidasReceiver::getInstance();
    receiver.processMessage(hBuf, id, pheader, message);
}

// Static callback: process transition
INT MidasReceiver::processTransitionCallback(INT run_number, char* error) {
    MidasReceiver& receiver = MidasReceiver::getInstance();
    return receiver.processTransition(run_number, error);
}

void MidasReceiver::processEvent(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent) {
    bool dataJam = false;
    int size, *pdata, id;

    // Accumulate received event size
    size = pheader->data_size;
    id = pheader->event_id;
    if (id > 9) {
        id = 9; // Clamp ID to range [0-9]
    }
    eventByteCount += size + sizeof(EVENT_HEADER);

    // Serial number checks for all events
    if (getAllEvents) {
        // Skip serial number check for the first event or if it's the first event of a new run
        if (!firstEvent && pheader->serial_number != 0 &&
            static_cast<INT>(pheader->serial_number) != serialNumbers[id] + 1) {
            cm_msg(MERROR, "processEvent",
                   "Serial number mismatch: Expected Serial: %d, Actual Serial: %d, Event ID: %d, Size: %d",
                   serialNumbers[id] + 1, pheader->serial_number, id, size);
            cm_msg(MERROR, "processEvent", "Event Header: [Event ID: %d, Serial: %d, Data Size: %d]",
                   pheader->event_id, pheader->serial_number, pheader->data_size);
            countMismatches++;
        }
        serialNumbers[id] = pheader->serial_number;
    }

    firstEvent = false; // After processing the first event

    // Create a TMEvent from the received buffer
    TMEvent timedEvent(pheader, size + sizeof(EVENT_HEADER));

    {
        std::lock_guard<std::mutex> lock(eventBufferMutex);
        if (eventBuffer.size() >= maxBufferSize) {
            eventBuffer.pop_front(); // Remove the oldest event to make space
        }

        // Create a TimedEvent with the current timestamp and the constructed TMEvent
        TimedEvent newTimedEvent;
        newTimedEvent.timestamp = std::chrono::system_clock::now();
        newTimedEvent.event = timedEvent;  // Store the TMEvent

        eventBuffer.push_back(std::move(newTimedEvent));
        bufferCV.notify_all(); // Notify any waiting threads
    }
}

// Process message (add to buffer with timestamp)
void MidasReceiver::processMessage(HNDLE hBuf, HNDLE id, EVENT_HEADER* pheader, void* message) {
    TimedMessage timedMessage;
    timedMessage.timestamp = std::chrono::system_clock::now();
    timedMessage.message = message; // Store the message data

    // Lock buffer for thread safety
    {
        std::lock_guard<std::mutex> lock(messageBufferMutex);
        if (messageBuffer.size() >= maxBufferSize) {
            messageBuffer.pop_front(); // Remove oldest message if buffer is full
        }
        messageBuffer.push_back(timedMessage);
    }
}

// Process transition (add to buffer with timestamp)
INT MidasReceiver::processTransition(INT run_number, char* error) {
    TimedTransition timedTransition;
    timedTransition.timestamp = std::chrono::system_clock::now();
    timedTransition.run_number = run_number;
    std::strncpy(timedTransition.error, error, sizeof(timedTransition.error) - 1);

    // Lock buffer for thread safety
    {
        std::lock_guard<std::mutex> lock(transitionBufferMutex);
        if (transitionBuffer.size() >= maxBufferSize) {
            transitionBuffer.pop_front(); // Remove oldest transition if buffer is full
        }
        transitionBuffer.push_back(timedTransition);
    }

    cm_msg(MINFO, "processTransition", "Processing transition for run: %d", run_number);
    return SUCCESS;
}


// Retrieve the latest N events (including timestamps)
std::vector<MidasReceiver::TimedEvent> MidasReceiver::getLatestEvents(size_t n) {
    std::vector<TimedEvent> events;
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    
    size_t start = eventBuffer.size() > n ? eventBuffer.size() - n : 0;
    for (size_t i = start; i < eventBuffer.size(); ++i) {
        events.push_back(eventBuffer[i]); // Directly use the entire TimedEvent from the buffer
    }
    return events;
}

// Retrieve the latest N events since a specific timestamp (including timestamps)
std::vector<MidasReceiver::TimedEvent> MidasReceiver::getLatestEvents(size_t n, std::chrono::system_clock::time_point since) {
    std::vector<TimedEvent> events;
    std::lock_guard<std::mutex> lock(eventBufferMutex);

    // Create a temporary list to store events that are after the 'since' timestamp
    std::vector<TimedEvent> filteredEvents;

    // Filter events based on the 'since' timestamp
    for (const auto& timedEvent : eventBuffer) {
        if (timedEvent.timestamp > since) {
            filteredEvents.push_back(timedEvent); // Store the entire TimedEvent (including timestamp)
        }
    }

    // Now return the most recent 'n' events from the filtered list
    size_t start = filteredEvents.size() > n ? filteredEvents.size() - n : 0;
    for (size_t i = start; i < filteredEvents.size(); ++i) {
        events.push_back(filteredEvents[i]);
    }

    return events;
}

// Retrieve events since a specific timestamp (including timestamps)
std::vector<MidasReceiver::TimedEvent> MidasReceiver::getLatestEvents(std::chrono::system_clock::time_point since) {
    std::vector<TimedEvent> events;
    std::lock_guard<std::mutex> lock(eventBufferMutex);

    // Iterate through the event buffer starting from the earliest event
    for (auto it = eventBuffer.begin(); it != eventBuffer.end(); ++it) {
        if (it->timestamp > since) {
            // Add all subsequent events (already in time order) to the result
            for (; it != eventBuffer.end(); ++it) {
                events.push_back(*it); // Directly use the entire TimedEvent from the buffer
            }
            break; // Exit the loop once we find the first event after the given timestamp
        }
    }
    return events;
}

// Retrieve all events (including timestamps)
std::vector<MidasReceiver::TimedEvent> MidasReceiver::getWholeBuffer() {
    std::vector<TimedEvent> events;
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    events.insert(events.end(), eventBuffer.begin(), eventBuffer.end()); // Directly use TimedEvent from the buffer
    return events;
}

// Retrieve all messages (including timestamps)
std::vector<MidasReceiver::TimedMessage> MidasReceiver::getMessageBuffer() {
    std::vector<TimedMessage> messages;
    std::lock_guard<std::mutex> lock(messageBufferMutex); // Ensure thread safety
    messages.insert(messages.end(), messageBuffer.begin(), messageBuffer.end()); // Directly use TimedMessage from the buffer
    return messages;
}

// Retrieve the latest N messages (including timestamps)
std::vector<MidasReceiver::TimedMessage> MidasReceiver::getLatestMessages(size_t n) {
    std::vector<TimedMessage> messages;
    std::lock_guard<std::mutex> lock(messageBufferMutex);

    size_t start = messageBuffer.size() > n ? messageBuffer.size() - n : 0;
    for (size_t i = start; i < messageBuffer.size(); ++i) {
        messages.push_back(messageBuffer[i]); // Directly use the entire TimedMessage from the buffer
    }
    return messages;
}

// Retrieve the latest N messages since a specific timestamp (including timestamps)
std::vector<MidasReceiver::TimedMessage> MidasReceiver::getLatestMessages(size_t n, std::chrono::system_clock::time_point since) {
    std::vector<TimedMessage> messages;
    std::lock_guard<std::mutex> lock(messageBufferMutex);

    // Create a temporary list to store messages that are after the 'since' timestamp
    std::vector<TimedMessage> filteredMessages;

    // Filter messages based on the 'since' timestamp
    for (const auto& timedMessage : messageBuffer) {
        if (timedMessage.timestamp > since) {
            filteredMessages.push_back(timedMessage); // Store the entire TimedMessage (including timestamp)
        }
    }

    // Now return the most recent 'n' messages from the filtered list
    size_t start = filteredMessages.size() > n ? filteredMessages.size() - n : 0;
    for (size_t i = start; i < filteredMessages.size(); ++i) {
        messages.push_back(filteredMessages[i]);
    }

    return messages;
}

// Retrieve messages since a specific timestamp (including timestamps)
std::vector<MidasReceiver::TimedMessage> MidasReceiver::getLatestMessages(std::chrono::system_clock::time_point since) {
    std::vector<TimedMessage> messages;
    std::lock_guard<std::mutex> lock(messageBufferMutex);

    // Iterate through the message buffer starting from the earliest message
    for (auto it = messageBuffer.begin(); it != messageBuffer.end(); ++it) {
        if (it->timestamp > since) {
            // Add all subsequent messages (already in time order) to the result
            for (; it != messageBuffer.end(); ++it) {
                messages.push_back(*it); // Directly use the entire TimedMessage from the buffer
            }
            break; // Exit the loop once we find the first message after the given timestamp
        }
    }
    return messages;
}

// Retrieve all transitions (including timestamps)
std::vector<MidasReceiver::TimedTransition> MidasReceiver::getTransitionBuffer() {
    std::vector<TimedTransition> transitions;
    std::lock_guard<std::mutex> lock(transitionBufferMutex); // Ensure thread safety
    transitions.insert(transitions.end(), transitionBuffer.begin(), transitionBuffer.end()); // Directly use TimedTransition from the buffer
    return transitions;
}

// Retrieve the latest N transitions (including timestamps)
std::vector<MidasReceiver::TimedTransition> MidasReceiver::getLatestTransitions(size_t n) {
    std::vector<TimedTransition> transitions;
    std::lock_guard<std::mutex> lock(transitionBufferMutex);

    size_t start = transitionBuffer.size() > n ? transitionBuffer.size() - n : 0;
    for (size_t i = start; i < transitionBuffer.size(); ++i) {
        transitions.push_back(transitionBuffer[i]); // Directly use the entire TimedTransition from the buffer
    }
    return transitions;
}

// Retrieve the latest N transitions since a specific timestamp (including timestamps)
std::vector<MidasReceiver::TimedTransition> MidasReceiver::getLatestTransitions(size_t n, std::chrono::system_clock::time_point since) {
    std::vector<TimedTransition> transitions;
    std::lock_guard<std::mutex> lock(transitionBufferMutex);

    // Create a temporary list to store transitions that are after the 'since' timestamp
    std::vector<TimedTransition> filteredTransitions;

    // Filter transitions based on the 'since' timestamp
    for (const auto& timedTransition : transitionBuffer) {
        if (timedTransition.timestamp > since) {
            filteredTransitions.push_back(timedTransition); // Store the entire TimedTransition (including timestamp)
        }
    }

    // Now return the most recent 'n' transitions from the filtered list
    size_t start = filteredTransitions.size() > n ? filteredTransitions.size() - n : 0;
    for (size_t i = start; i < filteredTransitions.size(); ++i) {
        transitions.push_back(filteredTransitions[i]);
    }

    return transitions;
}

// Retrieve transitions since a specific timestamp (including timestamps)
std::vector<MidasReceiver::TimedTransition> MidasReceiver::getLatestTransitions(std::chrono::system_clock::time_point since) {
    std::vector<TimedTransition> transitions;
    std::lock_guard<std::mutex> lock(transitionBufferMutex);

    // Iterate through the transition buffer starting from the earliest transition
    for (auto it = transitionBuffer.begin(); it != transitionBuffer.end(); ++it) {
        if (it->timestamp > since) {
            // Add all subsequent transitions (already in time order) to the result
            for (; it != transitionBuffer.end(); ++it) {
                transitions.push_back(*it); // Directly use the entire TimedTransition from the buffer
            }
            break; // Exit the loop once we find the first transition after the given timestamp
        }
    }
    return transitions;
}



// Getter for running state (thread-safe)
bool MidasReceiver::isListeningForEvents() const {
    return listeningForEvents.load(); // Atomic load of the running state
}

// Getter for status (thread-safe)
INT MidasReceiver::getStatus() const {
    std::lock_guard<std::mutex> lock(statusMutex); // Locking to ensure thread-safety
    return status;
}
