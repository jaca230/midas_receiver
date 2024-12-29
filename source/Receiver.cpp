#include "Receiver.h"
#include <cstring>
#include <iostream>

#define MAX_EVENT_SIZE (10 * 1024 * 1024)

// Initialize the static member variables
Receiver* Receiver::instance = nullptr;
std::mutex Receiver::initMutex;

// Constructor (private for singleton)
Receiver::Receiver()
    : hostName(""), 
      bufferName("SYSTEM"), 
      clientName("Event Receiver"), 
      eventID(EVENTID_ALL), 
      getAllEvents(true), 
      maxBufferSize(1000), 
      running(false) {}

// Destructor
Receiver::~Receiver() {
    stop();
}

// Get the singleton instance
Receiver& Receiver::getInstance() {
    std::lock_guard<std::mutex> lock(initMutex);

    if (instance == nullptr) {
        instance = new Receiver();
    }
    return *instance;
}

// Initialize receiver with parameters
void Receiver::init(const std::string& host, 
                    const std::string& bufferName, 
                    const std::string& clientName, 
                    int eventID, 
                    bool getAllEvents, 
                    size_t maxBufferSize) {
    // Only initialize if not already initialized
    if (hostName.empty()) {
        // If host is empty, retrieve it from cm_get_environment
        if (host.empty()) {
            char host_name[HOST_NAME_LENGTH], expt_name[NAME_LENGTH], str[80];
            cm_get_environment(host_name, sizeof(host_name), expt_name, sizeof(expt_name));
            this->hostName = host_name;
        } else {
            this->hostName = host;
        }

        this->bufferName = bufferName.empty() ? this->bufferName : bufferName;
        this->clientName = clientName.empty() ? this->clientName : clientName;
        this->eventID = eventID;
        this->getAllEvents = getAllEvents;
        this->maxBufferSize = maxBufferSize;
    }
}

// Start receiving events
void Receiver::start() {
    if (!running) {
        running = true;
        listeningForEvents = true;
        workerThread = std::thread(&Receiver::run, this);
    }
}

// Stop receiving events
void Receiver::stop() {
    if (running) {
        if (workerThread.joinable()) {
            workerThread.join();
        }
        cm_disconnect_experiment();
        running = false;
    }
}

// Main worker thread
void Receiver::run() {
    status = cm_connect_experiment(hostName.c_str(), "", clientName.c_str(), nullptr); // Set status during connection

    if (status != CM_SUCCESS) {
        cm_msg(MERROR, "Receiver::run", "Failed to connect to experiment. Status: %d", status);
        listeningForEvents = false;
        return;
    }

    status = bm_open_buffer(bufferName.c_str(), MAX_EVENT_SIZE * 2, &hBufEvent); // Set status for buffer opening
    if (status != BM_SUCCESS) {
        cm_msg(MERROR, "Receiver::run", "Failed to open buffer. Status: %d", status);
        listeningForEvents = false;
        return;
    }

    status = bm_set_cache_size(hBufEvent, 100000, 0); // Update status if cache size fails
    if (status != BM_SUCCESS) {
        cm_msg(MERROR, "Receiver::run", "Failed to set cache size. Status: %d", status);
        listeningForEvents = false;
        return;
    }

    status = bm_request_event(hBufEvent, (WORD)eventID, TRIGGER_ALL, getAllEvents ? GET_ALL : GET_NONBLOCKING,
                              &requestID, Receiver::processEventCallback); // Update status when event request is made

    if (status != BM_SUCCESS) {
        cm_msg(MERROR, "Receiver::run", "Failed to request event. Status: %d", status);
        listeningForEvents = false;
        return;
    }

    while (running && (status != RPC_SHUTDOWN && status != SS_ABORT)) {
        status = cm_yield(1000);
    }

    bm_close_buffer(hBufEvent);
    listeningForEvents = false;

}

// Static callback
void Receiver::processEventCallback(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent) {
    Receiver& receiver = Receiver::getInstance();  // Get the singleton instance of Receiver
    receiver.processEvent(hBuf, request_id, pheader, pevent);        // Process the event
}

// Process individual events
void Receiver::processEvent(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent) {
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

    // Store the event data in the buffer
    std::vector<char> event(pheader->data_size + sizeof(EVENT_HEADER));
    std::memcpy(event.data(), pheader, sizeof(EVENT_HEADER));
    std::memcpy(event.data() + sizeof(EVENT_HEADER), pevent, pheader->data_size);

    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        if (eventBuffer.size() >= maxBufferSize) {
            eventBuffer.pop_front(); // Remove the oldest event to make space
        }
        eventBuffer.push_back(std::move(event));
        bufferCV.notify_all(); // Notify any waiting threads
    }
}

// Retrieve all latest events since the last retrieved index
std::vector<std::vector<char>> Receiver::getAllLatest(size_t& lastIndex) {
    std::vector<std::vector<char>> events;
    std::lock_guard<std::mutex> lock(bufferMutex);
    while (lastIndex < eventBuffer.size()) {
        events.push_back(eventBuffer[lastIndex]);
        ++lastIndex;
    }
    return events;
}

// Retrieve the last N events
std::vector<std::vector<char>> Receiver::getNLatest(size_t n) {
    std::vector<std::vector<char>> events;
    std::lock_guard<std::mutex> lock(bufferMutex);
    size_t start = eventBuffer.size() > n ? eventBuffer.size() - n : 0;
    for (size_t i = start; i < eventBuffer.size(); ++i) {
        events.push_back(eventBuffer[i]);
    }
    return events;
}

// Retrieve the whole event buffer
std::vector<std::vector<char>> Receiver::getWholeBuffer() {
    std::vector<std::vector<char>> events;
    std::lock_guard<std::mutex> lock(bufferMutex);
    events.insert(events.end(), eventBuffer.begin(), eventBuffer.end());
    return events;
}

// Getter for running state (thread-safe)
bool Receiver::isListeningForEvents() const {
    return listeningForEvents.load(); // Atomic load of the running state
}

// Getter for status (thread-safe)
INT Receiver::getStatus() const {
    std::lock_guard<std::mutex> lock(statusMutex); // Locking to ensure thread-safety
    return status;
}
