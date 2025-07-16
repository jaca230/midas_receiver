#include "MidasReceiver.h"
#include <cstring>
#include <iostream>

#define MAX_EVENT_SIZE (10 * 1024 * 1024)

// Initialize the static member variables
MidasReceiver* MidasReceiver::instance = nullptr;
std::mutex MidasReceiver::initMutex;

// Default constructor calls init() with default config
MidasReceiver::MidasReceiver() :
    running(false) {
    MidasReceiverConfig defaultConfig{};
    init(defaultConfig);
}

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

// Initialize receiver with config struct
void MidasReceiver::init(const MidasReceiverConfig& config) {
    // If host or experiment are empty, use environment variables via cm_get_environment
    if (config.host.empty() || config.experiment.empty()) {
        char host_name[HOST_NAME_LENGTH] = {0};
        char expt_name[NAME_LENGTH] = {0};
        cm_get_environment(host_name, sizeof(host_name), expt_name, sizeof(expt_name));
        if (config.host.empty()) {
            this->hostName = host_name;
        } else {
            this->hostName = config.host;
        }
        if (config.experiment.empty()) {
            this->exptName = expt_name;
        } else {
            this->exptName = config.experiment;
        }
    } else {
        this->hostName = config.host;
        this->exptName = config.experiment;
    }

    this->bufferName = config.bufferName.empty() ? this->bufferName : config.bufferName;
    this->clientName = config.clientName.empty() ? this->clientName : config.clientName;
    this->eventID = config.eventID;
    this->getAllEvents = config.getAllEvents;
    this->maxBufferSize = config.maxBufferSize;
    this->cmYieldTimeout = config.cmYieldTimeout;

    // Save the transition registrations for later use when setting up transitions
    this->transitionRegistrations_ = config.transitionRegistrations;
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
        running = false;
        if (workerThread.joinable()) {
            workerThread.join();
        }
        cm_disconnect_experiment();
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
                              &requestID, MidasReceiver::processEventCallback);
    if (status != BM_SUCCESS) {
        cm_msg(MERROR, "MidasReceiver::run", "Failed to request event. Status: %d", status);
        listeningForEvents = false;
        return;
    }

    status = cm_msg_register(MidasReceiver::processMessageCallback);
    if (status != CM_SUCCESS) {
        cm_msg(MERROR, "MidasReceiver::run", "Failed to register message callback. Status: %d", status);
        listeningForEvents = false;
        return;
    }

    for (const auto& reg : transitionRegistrations_) {
        status = cm_register_transition(reg.transition, MidasReceiver::processTransitionCallback, reg.sequence);
        if (status != CM_SUCCESS) {
            cm_msg(MERROR, "MidasReceiver::run",
                   "Failed to register transition callback for %s. Status: %d",
                   cm_transition_name(reg.transition).c_str(),
                   status);
            listeningForEvents = false;
            return;
        }
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
    int size = pheader->data_size;
    int id = pheader->event_id;
    if (id > 9) id = 9;
    eventByteCount += size + sizeof(EVENT_HEADER);

    if (getAllEvents) {
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
    firstEvent = false;

    // Create shared_ptr<TimedEvent>
    auto newTimedEvent = std::make_shared<TimedEvent>();
    newTimedEvent->timestamp = std::chrono::system_clock::now();
    newTimedEvent->event = std::make_shared<TMEvent>(pheader, size + sizeof(EVENT_HEADER));

    {
        std::lock_guard<std::mutex> lock(eventBufferMutex);
        if (eventBuffer.size() >= maxBufferSize) {
            eventBuffer.pop_front();
        }
        eventBuffer.push_back(std::move(newTimedEvent));
        bufferCV.notify_all();
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

    return SUCCESS;
}


std::vector<std::shared_ptr<MidasReceiver::TimedEvent>> MidasReceiver::getLatestEvents(size_t n) {
    std::vector<std::shared_ptr<TimedEvent>> events;
    std::lock_guard<std::mutex> lock(eventBufferMutex);

    size_t start = eventBuffer.size() > n ? eventBuffer.size() - n : 0;
    for (size_t i = start; i < eventBuffer.size(); ++i) {
        events.push_back(eventBuffer[i]);
    }
    return events;
}

std::vector<std::shared_ptr<MidasReceiver::TimedEvent>> MidasReceiver::getLatestEvents(size_t n, std::chrono::system_clock::time_point since) {
    std::vector<std::shared_ptr<TimedEvent>> events;
    std::lock_guard<std::mutex> lock(eventBufferMutex);

    std::vector<std::shared_ptr<TimedEvent>> filteredEvents;
    for (auto const& e : eventBuffer) {
        if (e->timestamp > since) {
            filteredEvents.push_back(e);
        }
    }

    size_t start = filteredEvents.size() > n ? filteredEvents.size() - n : 0;
    for (size_t i = start; i < filteredEvents.size(); ++i) {
        events.push_back(filteredEvents[i]);
    }
    return events;
}

std::vector<std::shared_ptr<MidasReceiver::TimedEvent>> MidasReceiver::getLatestEvents(std::chrono::system_clock::time_point since) {
    std::vector<std::shared_ptr<TimedEvent>> events;
    std::lock_guard<std::mutex> lock(eventBufferMutex);

    for (auto it = eventBuffer.begin(); it != eventBuffer.end(); ++it) {
        if ((*it)->timestamp > since) {
            for (; it != eventBuffer.end(); ++it) {
                events.push_back(*it);
            }
            break;
        }
    }
    return events;
}

std::vector<std::shared_ptr<MidasReceiver::TimedEvent>> MidasReceiver::getWholeBuffer() {
    std::vector<std::shared_ptr<TimedEvent>> events;
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    events.insert(events.end(), eventBuffer.begin(), eventBuffer.end());
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



// Getter for listening state (thread-safe)
bool MidasReceiver::isListeningForEvents() const {
    return listeningForEvents.load(); // Atomic load of the running state
}

// Getter for running state (thread-safe)
bool MidasReceiver::IsRunning() const {
    return running.load();
}


// Getter for status (thread-safe)
INT MidasReceiver::getStatus() const {
    std::lock_guard<std::mutex> lock(statusMutex); // Locking to ensure thread-safety
    return status;
}
