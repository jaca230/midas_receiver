#include "MidasReceiver.h"
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>

#define MAX_EVENT_SIZE (10 * 1024 * 1024)

// Debug printing macros
#define DEBUG_PRINT(msg) std::cout << "[DEBUG] " << __FUNCTION__ << ":" << __LINE__ << " - " << msg << std::endl
#define DEBUG_PRINTF(fmt, ...) printf("[DEBUG] %s:%d - " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

// Initialize the static member variables
MidasReceiver* MidasReceiver::instance = nullptr;
std::mutex MidasReceiver::initMutex;

// Default constructor calls init() with default config
MidasReceiver::MidasReceiver() :
    running(false) {
    DEBUG_PRINT("Creating MidasReceiver with default config");
    MidasReceiverConfig defaultConfig{};
    init(defaultConfig);
}

// Destructor
MidasReceiver::~MidasReceiver() {
    DEBUG_PRINT("Destroying MidasReceiver");
    stop();
}

// Get the singleton instance
MidasReceiver& MidasReceiver::getInstance() {
    std::lock_guard<std::mutex> lock(initMutex);

    if (instance == nullptr) {
        DEBUG_PRINT("Creating new singleton instance");
        instance = new MidasReceiver();
    } else {
        DEBUG_PRINT("Returning existing singleton instance");
    }
    return *instance;
}

// Initialize receiver with config struct
void MidasReceiver::init(const MidasReceiverConfig& config) {
    DEBUG_PRINT("Initializing MidasReceiver");
    DEBUG_PRINTF("Config - host: '%s', experiment: '%s', bufferName: '%s', clientName: '%s'", 
                 config.host.c_str(), config.experiment.c_str(), 
                 config.bufferName.c_str(), config.clientName.c_str());
    DEBUG_PRINTF("Config - eventID: %d, getAllEvents: %s, maxBufferSize: %zu, cmYieldTimeout: %d", 
                 config.eventID, config.getAllEvents ? "true" : "false", 
                 config.maxBufferSize, config.cmYieldTimeout);

    // If host or experiment are empty, use environment variables via cm_get_environment
    if (config.host.empty() || config.experiment.empty()) {
        DEBUG_PRINT("Host or experiment empty, getting from environment");
        char host_name[HOST_NAME_LENGTH] = {0};
        char expt_name[NAME_LENGTH] = {0};
        cm_get_environment(host_name, sizeof(host_name), expt_name, sizeof(expt_name));
        DEBUG_PRINTF("Environment - host: '%s', experiment: '%s'", host_name, expt_name);
        
        if (config.host.empty()) {
            this->hostName = host_name;
            DEBUG_PRINTF("Using environment host: '%s'", this->hostName.c_str());
        } else {
            this->hostName = config.host;
            DEBUG_PRINTF("Using config host: '%s'", this->hostName.c_str());
        }
        if (config.experiment.empty()) {
            this->exptName = expt_name;
            DEBUG_PRINTF("Using environment experiment: '%s'", this->exptName.c_str());
        } else {
            this->exptName = config.experiment;
            DEBUG_PRINTF("Using config experiment: '%s'", this->exptName.c_str());
        }
    } else {
        this->hostName = config.host;
        this->exptName = config.experiment;
        DEBUG_PRINTF("Using config values - host: '%s', experiment: '%s'", 
                     this->hostName.c_str(), this->exptName.c_str());
    }

    this->bufferName = config.bufferName.empty() ? this->bufferName : config.bufferName;
    this->clientName = config.clientName.empty() ? this->clientName : config.clientName;
    this->eventID = config.eventID;
    this->getAllEvents = config.getAllEvents;
    this->maxBufferSize = config.maxBufferSize;
    this->cmYieldTimeout = config.cmYieldTimeout;

    DEBUG_PRINTF("Final config - bufferName: '%s', clientName: '%s', eventID: %d", 
                 this->bufferName.c_str(), this->clientName.c_str(), this->eventID);
    DEBUG_PRINTF("Final config - getAllEvents: %s, maxBufferSize: %zu, cmYieldTimeout: %d", 
                 this->getAllEvents ? "true" : "false", this->maxBufferSize, this->cmYieldTimeout);

    // Save the transition registrations for later use when setting up transitions
    this->transitionRegistrations_ = config.transitionRegistrations;
    DEBUG_PRINTF("Registered %zu transition callbacks", this->transitionRegistrations_.size());
}

// Start receiving events
void MidasReceiver::start() {
    DEBUG_PRINTF("Start called - currently running: %s", running.load() ? "true" : "false");
    if (!running) {
        DEBUG_PRINT("Starting MidasReceiver");
        running = true;
        listeningForEvents = true;
        workerThread = std::thread(&MidasReceiver::run, this);
        DEBUG_PRINT("Worker thread started");
    } else {
        DEBUG_PRINT("Already running, ignoring start request");
    }
}

// Stop receiving events
void MidasReceiver::stop() {
    DEBUG_PRINTF("Stop called - currently running: %s", running.load() ? "true" : "false");
    if (running) {
        DEBUG_PRINT("Stopping MidasReceiver");
        running = false;
        if (workerThread.joinable()) {
            DEBUG_PRINT("Waiting for worker thread to join");
            workerThread.join();
            DEBUG_PRINT("Worker thread joined");
        }
        DEBUG_PRINT("Disconnecting from experiment");
        cm_disconnect_experiment();
        DEBUG_PRINT("MidasReceiver stopped");
    } else {
        DEBUG_PRINT("Not running, ignoring stop request");
    }
}

// Main worker thread
void MidasReceiver::run() {
    DEBUG_PRINT("Worker thread starting");
    DEBUG_PRINTF("Connecting to experiment - host: '%s', experiment: '%s', client: '%s'", 
                 hostName.c_str(), exptName.c_str(), clientName.c_str());
    
    status = cm_connect_experiment(hostName.c_str(), exptName.c_str(), clientName.c_str(), nullptr);

    if (status != CM_SUCCESS) {
        DEBUG_PRINTF("FAILED to connect to experiment. Status: %d", status);
        cm_msg(MERROR, "MidasReceiver::run", "Failed to connect to experiment. Status: %d", status);
        listeningForEvents = false;
        return;
    }
    DEBUG_PRINT("Successfully connected to experiment");

    DEBUG_PRINTF("Opening buffer '%s' with max event size: %d", bufferName.c_str(), MAX_EVENT_SIZE * 2);
    status = bm_open_buffer(bufferName.c_str(), MAX_EVENT_SIZE * 2, &hBufEvent);
    if (status != BM_SUCCESS) {
        DEBUG_PRINTF("FAILED to open buffer. Status: %d", status);
        cm_msg(MERROR, "MidasReceiver::run", "Failed to open buffer. Status: %d", status);
        listeningForEvents = false;
        return;
    }
    DEBUG_PRINT("Successfully opened buffer");

    DEBUG_PRINT("Setting cache size to 100000");
    status = bm_set_cache_size(hBufEvent, 100000, 0);
    if (status != BM_SUCCESS) {
        DEBUG_PRINTF("FAILED to set cache size. Status: %d", status);
        cm_msg(MERROR, "MidasReceiver::run", "Failed to set cache size. Status: %d", status);
        listeningForEvents = false;
        return;
    }
    DEBUG_PRINT("Successfully set cache size");

    DEBUG_PRINTF("Requesting events - eventID: %d, mode: %s", 
                 eventID, getAllEvents ? "GET_ALL" : "GET_NONBLOCKING");
    status = bm_request_event(hBufEvent, (WORD)eventID, TRIGGER_ALL, getAllEvents ? GET_ALL : GET_NONBLOCKING,
                              &requestID, MidasReceiver::processEventCallback);
    if (status != BM_SUCCESS) {
        DEBUG_PRINTF("FAILED to request event. Status: %d", status);
        cm_msg(MERROR, "MidasReceiver::run", "Failed to request event. Status: %d", status);
        listeningForEvents = false;
        return;
    }
    DEBUG_PRINTF("Successfully requested events with requestID: %d", requestID);

    DEBUG_PRINT("Registering message callback");
    status = cm_msg_register(MidasReceiver::processMessageCallback);
    if (status != CM_SUCCESS) {
        DEBUG_PRINTF("FAILED to register message callback. Status: %d", status);
        cm_msg(MERROR, "MidasReceiver::run", "Failed to register message callback. Status: %d", status);
        listeningForEvents = false;
        return;
    }
    DEBUG_PRINT("Successfully registered message callback");

    DEBUG_PRINTF("Registering %zu transition callbacks", transitionRegistrations_.size());
    for (const auto& reg : transitionRegistrations_) {
        DEBUG_PRINTF("Registering transition: %s with sequence: %d", 
                     cm_transition_name(reg.transition).c_str(), reg.sequence);
        status = cm_register_transition(reg.transition, MidasReceiver::processTransitionCallback, reg.sequence);
        if (status != CM_SUCCESS) {
            DEBUG_PRINTF("FAILED to register transition callback for %s. Status: %d",
                         cm_transition_name(reg.transition).c_str(), status);
            cm_msg(MERROR, "MidasReceiver::run",
                   "Failed to register transition callback for %s. Status: %d",
                   cm_transition_name(reg.transition).c_str(),
                   status);
            listeningForEvents = false;
            return;
        }
    }
    DEBUG_PRINT("All transition callbacks registered successfully");

    DEBUG_PRINT("Entering main event loop");
    int yieldCount = 0;
    while (running && (status != RPC_SHUTDOWN && status != SS_ABORT)) {
        status = cm_yield(1);
        yieldCount++;
        
        if (yieldCount % 1000 == 0) {  // Print every 1000 yields
            DEBUG_PRINTF("Main loop - yield count: %d, status: %d, running: %s", 
                         yieldCount, status, running.load() ? "true" : "false");
            
            // Print buffer statistics
            std::lock_guard<std::mutex> lock(eventBufferMutex);
            DEBUG_PRINTF("Buffer stats - events: %zu/%zu, bytes received: %llu, mismatches: %d", 
                         eventBuffer.size(), maxBufferSize, eventByteCount, countMismatches);
        }
        
        if (status != CM_SUCCESS && status != RPC_SHUTDOWN && status != SS_ABORT) {
            DEBUG_PRINTF("cm_yield returned error status: %d", status);
        }
    }

    DEBUG_PRINTF("Exiting main loop - running: %s, status: %d", 
                 running.load() ? "true" : "false", status);
    DEBUG_PRINT("Closing buffer");
    bm_close_buffer(hBufEvent);
    listeningForEvents = false;
    DEBUG_PRINT("Worker thread ending");
}

// Static callback: process event
void MidasReceiver::processEventCallback(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent) {
    static int callbackCount = 0;
    callbackCount++;
    
    if (callbackCount % 100 == 0) {  // Print every 100 events
        DEBUG_PRINTF("Event callback #%d - hBuf: %d, request_id: %d", callbackCount, hBuf, request_id);
        if (pheader) {
            DEBUG_PRINTF("Event header - ID: %d, serial: %d, size: %d", 
                         pheader->event_id, pheader->serial_number, pheader->data_size);
        }
    }
    
    MidasReceiver& receiver = MidasReceiver::getInstance();
    receiver.processEvent(hBuf, request_id, pheader, pevent);
}

// Static callback: process message
void MidasReceiver::processMessageCallback(HNDLE hBuf, HNDLE id, EVENT_HEADER* pheader, void* message) {
    DEBUG_PRINTF("Message callback - hBuf: %d, id: %d", hBuf, id);
    if (pheader) {
        DEBUG_PRINTF("Message header - ID: %d, serial: %d, size: %d", 
                     pheader->event_id, pheader->serial_number, pheader->data_size);
    }
    MidasReceiver& receiver = MidasReceiver::getInstance();
    receiver.processMessage(hBuf, id, pheader, message);
}

// Static callback: process transition
INT MidasReceiver::processTransitionCallback(INT run_number, char* error) {
    DEBUG_PRINTF("Transition callback - run_number: %d, error: '%s'", run_number, error ? error : "none");
    MidasReceiver& receiver = MidasReceiver::getInstance();
    return receiver.processTransition(run_number, error);
}

void MidasReceiver::processEvent(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent) {
    static int processCount = 0;
    processCount++;
    
    bool dataJam = false;
    int size, *pdata, id;

    if (!pheader) {
        DEBUG_PRINTF("ERROR: processEvent called with null header (event #%d)", processCount);
        return;
    }

    // Accumulate received event size
    size = pheader->data_size;
    id = pheader->event_id;
    int originalId = id;
    
    if (id > 9) {
        DEBUG_PRINTF("WARNING: Event ID %d clamped to 9 (event #%d)", id, processCount);
        id = 9; // Clamp ID to range [0-9]
    }
    
    eventByteCount += size + sizeof(EVENT_HEADER);

    if (processCount % 50 == 0 || originalId != id || size > 1000000) {  // Print frequent events or anomalies
        DEBUG_PRINTF("Processing event #%d - ID: %d->%d, serial: %d, size: %d bytes, total bytes: %llu", 
                     processCount, originalId, id, pheader->serial_number, size, eventByteCount);
    }

    // Serial number checks for all events
    if (getAllEvents) {
        bool skipSerialCheck = firstEvent || pheader->serial_number == 0;
        
        if (skipSerialCheck) {
            if (firstEvent) {
                DEBUG_PRINTF("Skipping serial check for FIRST event - ID: %d, serial: %d", id, pheader->serial_number);
            } else {
                DEBUG_PRINTF("Skipping serial check for ZERO serial - ID: %d, serial: %d", id, pheader->serial_number);
            }
        } else {
            int expectedSerial = serialNumbers[id] + 1;
            int actualSerial = static_cast<INT>(pheader->serial_number);
            
            if (actualSerial != expectedSerial) {
                DEBUG_PRINTF("SERIAL MISMATCH detected - Expected: %d, Actual: %d, Event ID: %d, Size: %d", 
                             expectedSerial, actualSerial, id, size);
                DEBUG_PRINTF("Event Header details - [Event ID: %d, Serial: %d, Data Size: %d]",
                             pheader->event_id, pheader->serial_number, pheader->data_size);
                
                cm_msg(MERROR, "processEvent",
                       "Serial number mismatch: Expected Serial: %d, Actual Serial: %d, Event ID: %d, Size: %d",
                       expectedSerial, actualSerial, id, size);
                cm_msg(MERROR, "processEvent", "Event Header: [Event ID: %d, Serial: %d, Data Size: %d]",
                       pheader->event_id, pheader->serial_number, pheader->data_size);
                countMismatches++;
            } else if (processCount % 100 == 0) {
                DEBUG_PRINTF("Serial OK - Expected: %d, Actual: %d, Event ID: %d", 
                             expectedSerial, actualSerial, id);
            }
        }
        
        int oldSerial = serialNumbers[id];
        serialNumbers[id] = pheader->serial_number;
        
        if (processCount % 100 == 0) {
            DEBUG_PRINTF("Serial number updated - ID: %d, old: %d, new: %d", 
                         id, oldSerial, pheader->serial_number);
        }
    }

    if (firstEvent) {
        DEBUG_PRINT("Processing FIRST event - setting firstEvent = false");
        firstEvent = false; // After processing the first event
    }

    // Create a TMEvent from the received buffer
    TMEvent timedEvent(pheader, size + sizeof(EVENT_HEADER));

    // Buffer management with detailed logging
    {
        std::lock_guard<std::mutex> lock(eventBufferMutex);
        size_t bufferSizeBefore = eventBuffer.size();
        bool willDropOldest = (bufferSizeBefore >= maxBufferSize);
        
        if (willDropOldest) {
            auto& oldestEvent = eventBuffer.front();
            DEBUG_PRINTF("BUFFER FULL - dropping oldest event (buffer: %zu/%zu)", 
                         bufferSizeBefore, maxBufferSize);
            DEBUG_PRINTF("Dropped event details - timestamp age: %.3f seconds", 
                         std::chrono::duration<double>(std::chrono::system_clock::now() - oldestEvent.timestamp).count());
            eventBuffer.pop_front(); // Remove the oldest event to make space
        }

        // Create a TimedEvent with the current timestamp and the constructed TMEvent
        TimedEvent newTimedEvent;
        newTimedEvent.timestamp = std::chrono::system_clock::now();
        newTimedEvent.event = timedEvent;  // Store the TMEvent

        eventBuffer.push_back(std::move(newTimedEvent));
        size_t bufferSizeAfter = eventBuffer.size();
        
        if (processCount % 50 == 0 || willDropOldest) {
            DEBUG_PRINTF("Buffer updated - size: %zu->%zu/%zu, dropped oldest: %s", 
                         bufferSizeBefore, bufferSizeAfter, maxBufferSize, 
                         willDropOldest ? "YES" : "NO");
        }
        
        bufferCV.notify_all(); // Notify any waiting threads
    }
    
    if (processCount % 100 == 0) {
        DEBUG_PRINTF("Event processing complete - total processed: %d, mismatches: %d", 
                     processCount, countMismatches);
    }
}

// Process message (add to buffer with timestamp)
void MidasReceiver::processMessage(HNDLE hBuf, HNDLE id, EVENT_HEADER* pheader, void* message) {
    static int messageCount = 0;
    messageCount++;
    
    DEBUG_PRINTF("Processing message #%d - hBuf: %d, id: %d", messageCount, hBuf, id);
    
    TimedMessage timedMessage;
    timedMessage.timestamp = std::chrono::system_clock::now();
    timedMessage.message = message; // Store the message data

    // Lock buffer for thread safety
    {
        std::lock_guard<std::mutex> lock(messageBufferMutex);
        size_t bufferSizeBefore = messageBuffer.size();
        bool willDropOldest = (bufferSizeBefore >= maxBufferSize);
        
        if (willDropOldest) {
            DEBUG_PRINTF("MESSAGE BUFFER FULL - dropping oldest message (buffer: %zu/%zu)", 
                         bufferSizeBefore, maxBufferSize);
            messageBuffer.pop_front(); // Remove oldest message if buffer is full
        }
        
        messageBuffer.push_back(timedMessage);
        size_t bufferSizeAfter = messageBuffer.size();
        
        DEBUG_PRINTF("Message buffer updated - size: %zu->%zu/%zu, dropped oldest: %s", 
                     bufferSizeBefore, bufferSizeAfter, maxBufferSize, 
                     willDropOldest ? "YES" : "NO");
    }
}

// Process transition (add to buffer with timestamp)
INT MidasReceiver::processTransition(INT run_number, char* error) {
    static int transitionCount = 0;
    transitionCount++;
    
    DEBUG_PRINTF("Processing transition #%d - run_number: %d, error: '%s'", 
                 transitionCount, run_number, error ? error : "none");
    
    TimedTransition timedTransition;
    timedTransition.timestamp = std::chrono::system_clock::now();
    timedTransition.run_number = run_number;
    std::strncpy(timedTransition.error, error, sizeof(timedTransition.error) - 1);

    // Lock buffer for thread safety
    {
        std::lock_guard<std::mutex> lock(transitionBufferMutex);
        size_t bufferSizeBefore = transitionBuffer.size();
        bool willDropOldest = (bufferSizeBefore >= maxBufferSize);
        
        if (willDropOldest) {
            DEBUG_PRINTF("TRANSITION BUFFER FULL - dropping oldest transition (buffer: %zu/%zu)", 
                         bufferSizeBefore, maxBufferSize);
            transitionBuffer.pop_front(); // Remove oldest transition if buffer is full
        }
        
        transitionBuffer.push_back(timedTransition);
        size_t bufferSizeAfter = transitionBuffer.size();
        
        DEBUG_PRINTF("Transition buffer updated - size: %zu->%zu/%zu, dropped oldest: %s", 
                     bufferSizeBefore, bufferSizeAfter, maxBufferSize, 
                     willDropOldest ? "YES" : "NO");
    }

    DEBUG_PRINTF("Transition processing complete - returning SUCCESS");
    return SUCCESS;
}

// Retrieve the latest N events (including timestamps)
std::vector<MidasReceiver::TimedEvent> MidasReceiver::getLatestEvents(size_t n) {
    std::vector<TimedEvent> events;
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    
    DEBUG_PRINTF("getLatestEvents(%zu) - buffer has %zu events", n, eventBuffer.size());
    
    size_t start = eventBuffer.size() > n ? eventBuffer.size() - n : 0;
    size_t actualEvents = eventBuffer.size() - start;
    
    DEBUG_PRINTF("Returning %zu events (start index: %zu)", actualEvents, start);
    
    for (size_t i = start; i < eventBuffer.size(); ++i) {
        events.push_back(eventBuffer[i]); // Directly use the entire TimedEvent from the buffer
    }
    return events;
}

// Retrieve the latest N events since a specific timestamp (including timestamps)
std::vector<MidasReceiver::TimedEvent> MidasReceiver::getLatestEvents(size_t n, std::chrono::system_clock::time_point since) {
    std::vector<TimedEvent> events;
    std::lock_guard<std::mutex> lock(eventBufferMutex);

    auto now = std::chrono::system_clock::now();
    double sinceSeconds = std::chrono::duration<double>(now - since).count();
    DEBUG_PRINTF("getLatestEvents(%zu, since %.3f seconds ago) - buffer has %zu events", 
                 n, sinceSeconds, eventBuffer.size());

    // Create a temporary list to store events that are after the 'since' timestamp
    std::vector<TimedEvent> filteredEvents;

    // Filter events based on the 'since' timestamp
    for (const auto& timedEvent : eventBuffer) {
        if (timedEvent.timestamp > since) {
            filteredEvents.push_back(timedEvent); // Store the entire TimedEvent (including timestamp)
        }
    }

    DEBUG_PRINTF("Found %zu events after timestamp", filteredEvents.size());

    // Now return the most recent 'n' events from the filtered list
    size_t start = filteredEvents.size() > n ? filteredEvents.size() - n : 0;
    size_t actualEvents = filteredEvents.size() - start;
    
    DEBUG_PRINTF("Returning %zu events (start index: %zu)", actualEvents, start);
    
    for (size_t i = start; i < filteredEvents.size(); ++i) {
        events.push_back(filteredEvents[i]);
    }

    return events;
}

// Retrieve events since a specific timestamp (including timestamps)
std::vector<MidasReceiver::TimedEvent> MidasReceiver::getLatestEvents(std::chrono::system_clock::time_point since) {
    std::vector<TimedEvent> events;
    std::lock_guard<std::mutex> lock(eventBufferMutex);

    auto now = std::chrono::system_clock::now();
    double sinceSeconds = std::chrono::duration<double>(now - since).count();
    DEBUG_PRINTF("getLatestEvents(since %.3f seconds ago) - buffer has %zu events", 
                 sinceSeconds, eventBuffer.size());

    size_t eventsFound = 0;
    // Iterate through the event buffer starting from the earliest event
    for (auto it = eventBuffer.begin(); it != eventBuffer.end(); ++it) {
        if (it->timestamp > since) {
            // Add all subsequent events (already in time order) to the result
            for (; it != eventBuffer.end(); ++it) {
                events.push_back(*it); // Directly use the entire TimedEvent from the buffer
                eventsFound++;
            }
            break; // Exit the loop once we find the first event after the given timestamp
        }
    }
    
    DEBUG_PRINTF("Found and returning %zu events after timestamp", eventsFound);
    return events;
}

// Retrieve all events (including timestamps)
std::vector<MidasReceiver::TimedEvent> MidasReceiver::getWholeBuffer() {
    std::vector<TimedEvent> events;
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    
    DEBUG_PRINTF("getWholeBuffer() - returning all %zu events", eventBuffer.size());
    
    events.insert(events.end(), eventBuffer.begin(), eventBuffer.end()); // Directly use TimedEvent from the buffer
    return events;
}

// Retrieve all messages (including timestamps)
std::vector<MidasReceiver::TimedMessage> MidasReceiver::getMessageBuffer() {
    std::vector<TimedMessage> messages;
    std::lock_guard<std::mutex> lock(messageBufferMutex); // Ensure thread safety
    
    DEBUG_PRINTF("getMessageBuffer() - returning all %zu messages", messageBuffer.size());
    
    messages.insert(messages.end(), messageBuffer.begin(), messageBuffer.end()); // Directly use TimedMessage from the buffer
    return messages;
}

// Retrieve the latest N messages (including timestamps)
std::vector<MidasReceiver::TimedMessage> MidasReceiver::getLatestMessages(size_t n) {
    std::vector<TimedMessage> messages;
    std::lock_guard<std::mutex> lock(messageBufferMutex);

    DEBUG_PRINTF("getLatestMessages(%zu) - buffer has %zu messages", n, messageBuffer.size());

    size_t start = messageBuffer.size() > n ? messageBuffer.size() - n : 0;
    size_t actualMessages = messageBuffer.size() - start;
    
    DEBUG_PRINTF("Returning %zu messages (start index: %zu)", actualMessages, start);
    
    for (size_t i = start; i < messageBuffer.size(); ++i) {
        messages.push_back(messageBuffer[i]); // Directly use the entire TimedMessage from the buffer
    }
    return messages;
}

// Retrieve the latest N messages since a specific timestamp (including timestamps)
std::vector<MidasReceiver::TimedMessage> MidasReceiver::getLatestMessages(size_t n, std::chrono::system_clock::time_point since) {
    std::vector<TimedMessage> messages;
    std::lock_guard<std::mutex> lock(messageBufferMutex);

    auto now = std::chrono::system_clock::now();
    double sinceSeconds = std::chrono::duration<double>(now - since).count();
    DEBUG_PRINTF("getLatestMessages(%zu, since %.3f seconds ago) - buffer has %zu messages", 
                 n, sinceSeconds, messageBuffer.size());

    // Create a temporary list to store messages that are after the 'since' timestamp
    std::vector<TimedMessage> filteredMessages;

    // Filter messages based on the 'since' timestamp
    for (const auto& timedMessage : messageBuffer) {
        if (timedMessage.timestamp > since) {
            filteredMessages.push_back(timedMessage); // Store the entire TimedMessage (including timestamp)
        }
    }

    DEBUG_PRINTF("Found %zu messages after timestamp", filteredMessages.size());

    // Now return the most recent 'n' messages from the filtered list
    size_t start = filteredMessages.size() > n ? filteredMessages.size() - n : 0;
    size_t actualMessages = filteredMessages.size() - start;
    
    DEBUG_PRINTF("Returning %zu messages (start index: %zu)", actualMessages, start);
    
    for (size_t i = start; i < filteredMessages.size(); ++i) {
        messages.push_back(filteredMessages[i]);
    }

    return messages;
}

// Retrieve messages since a specific timestamp (including timestamps)
std::vector<MidasReceiver::TimedMessage> MidasReceiver::getLatestMessages(std::chrono::system_clock::time_point since) {
    std::vector<TimedMessage> messages;
    std::lock_guard<std::mutex> lock(messageBufferMutex);

    auto now = std::chrono::system_clock::now();
    double sinceSeconds = std::chrono::duration<double>(now - since).count();
    DEBUG_PRINTF("getLatestMessages(since %.3f seconds ago) - buffer has %zu messages", 
                 sinceSeconds, messageBuffer.size());

    size_t messagesFound = 0;
    // Iterate through the message buffer starting from the earliest message
    for (auto it = messageBuffer.begin(); it != messageBuffer.end(); ++it) {
        if (it->timestamp > since) {
            // Add all subsequent messages (already in time order) to the result
            for (; it != messageBuffer.end(); ++it) {
                messages.push_back(*it); // Directly use the entire TimedMessage from the buffer
                messagesFound++;
            }
            break; // Exit the loop once we find the first message after the given timestamp
        }
    }
    
    DEBUG_PRINTF("Found and returning %zu messages after timestamp", messagesFound);
    return messages;
}

// Retrieve all transitions (including timestamps)
std::vector<MidasReceiver::TimedTransition> MidasReceiver::getTransitionBuffer() {
    std::vector<TimedTransition> transitions;
    std::lock_guard<std::mutex> lock(transitionBufferMutex); // Ensure thread safety
    
    DEBUG_PRINTF("getTransitionBuffer() - returning all %zu transitions", transitionBuffer.size());
    
    transitions.insert(transitions.end(), transitionBuffer.begin(), transitionBuffer.end()); // Directly use TimedTransition from the buffer
    return transitions;
}

// Retrieve the latest N transitions (including timestamps)
std::vector<MidasReceiver::TimedTransition> MidasReceiver::getLatestTransitions(size_t n) {
    std::vector<TimedTransition> transitions;
    std::lock_guard<std::mutex> lock(transitionBufferMutex);

    DEBUG_PRINTF("getLatestTransitions(%zu) - buffer has %zu transitions", n, transitionBuffer.size());

    size_t start = transitionBuffer.size() > n ? transitionBuffer.size() - n : 0;
    size_t actualTransitions = transitionBuffer.size() - start;
    
    DEBUG_PRINTF("Returning %zu transitions (start index: %zu)", actualTransitions, start);
    
    for (size_t i = start; i < transitionBuffer.size(); ++i) {
        transitions.push_back(transitionBuffer[i]); // Directly use the entire TimedTransition from the buffer
    }
    return transitions;
}

// Retrieve the latest N transitions since a specific timestamp (including timestamps)
std::vector<MidasReceiver::TimedTransition> MidasReceiver::getLatestTransitions(size_t n, std::chrono::system_clock::time_point since) {
    std::vector<TimedTransition> transitions;
    std::lock_guard<std::mutex> lock(transitionBufferMutex);

    auto now = std::chrono::system_clock::now();
    double sinceSeconds = std::chrono::duration<double>(now - since).count();
    DEBUG_PRINTF("getLatestTransitions(%zu, since %.3f seconds ago) - buffer has %zu transitions", 
                 n, sinceSeconds, transitionBuffer.size());

    // Create a temporary list to store transitions that are after the 'since' timestamp
    std::vector<TimedTransition> filteredTransitions;

    // Filter transitions based on the 'since' timestamp
    for (const auto& timedTransition : transitionBuffer) {
        if (timedTransition.timestamp > since) {
            filteredTransitions.push_back(timedTransition); // Store the entire TimedTransition (including timestamp)
        }
    }

    DEBUG_PRINTF("Found %zu transitions after timestamp", filteredTransitions.size());

    // Now return the most recent 'n' transitions from the filtered list
    size_t start = filteredTransitions.size() > n ? filteredTransitions.size() - n : 0;
    size_t actualTransitions = filteredTransitions.size() - start;
    
    DEBUG_PRINTF("Returning %zu transitions (start index: %zu)", actualTransitions, start);
    
    for (size_t i = start; i < filteredTransitions.size(); ++i) {
        transitions.push_back(filteredTransitions[i]);
    }

    return transitions;
}

// Retrieve transitions since a specific timestamp (including timestamps)
std::vector<MidasReceiver::TimedTransition> MidasReceiver::getLatestTransitions(std::chrono::system_clock::time_point since) {
    std::vector<TimedTransition> transitions;
    std::lock_guard<std::mutex> lock(transitionBufferMutex);

    auto now = std::chrono::system_clock::now();
    double sinceSeconds = std::chrono::duration<double>(now - since).count();
    DEBUG_PRINTF("getLatestTransitions(since %.3f seconds ago) - buffer has %zu transitions", 
                 sinceSeconds, transitionBuffer.size());

    size_t transitionsFound = 0;
    // Iterate through the transition buffer starting from the earliest transition
    for (auto it = transitionBuffer.begin(); it != transitionBuffer.end(); ++it) {
        if (it->timestamp > since) {
            // Add all subsequent transitions (already in time order) to the result
            for (; it != transitionBuffer.end(); ++it) {
                transitions.push_back(*it); // Directly use the entire TimedTransition from the buffer
                transitionsFound++;
            }
            break; // Exit the loop once we find the first transition after the given timestamp
        }
    }
    
    DEBUG_PRINTF("Found and returning %zu transitions after timestamp", transitionsFound);
    return transitions;
}

// Getter for listening state (thread-safe)
bool MidasReceiver::isListeningForEvents() const {
    bool listening = listeningForEvents.load();
    DEBUG_PRINTF("isListeningForEvents() returning: %s", listening ? "true" : "false");
    return listening; // Atomic load of the running state
}

// Getter for running state (thread-safe)
bool MidasReceiver::IsRunning() const {
    bool isRunning = running.load();
    DEBUG_PRINTF("IsRunning() returning: %s", isRunning ? "true" : "false");
    return isRunning;
}

// Getter for status (thread-safe)
INT MidasReceiver::getStatus() const {
    std::lock_guard<std::mutex> lock(statusMutex); // Locking to ensure thread-safety
    DEBUG_PRINTF("getStatus() returning: %d", status);
    return status;
}