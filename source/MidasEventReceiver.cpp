#include "MidasEventReceiver.h"
#include <iostream>

// Define the static variables
std::map<HNDLE, std::string> MidasEventReceiver::bufferMap;
std::mutex MidasEventReceiver::bufferMapMutex;


#define MAX_EVENT_SIZE (10 * 1024 * 1024)

// Initialize static members
std::map<std::string, MidasEventReceiver*> MidasEventReceiver::instances;
std::mutex MidasEventReceiver::instanceMutex;

// Private constructor
MidasEventReceiver::MidasEventReceiver(const std::string& midasEventBufferName)
    : MidasReceiver("", "", "", 1000, 300), // Default values for parameters
      midasEventBufferName(midasEventBufferName), 
      eventID(EVENTID_ALL),  // Default value for eventID
      getAllEvents(true) {   // Default value for getAllEvents
    // Any additional initialization can go here
}

// Singleton method to get or create the instance
MidasEventReceiver* MidasEventReceiver::getInstance(const std::string& midasEventBufferName) {
    std::lock_guard<std::mutex> lock(instanceMutex);

    // Check if an instance for the given midasEventBufferName already exists
    auto it = instances.find(midasEventBufferName);
    if (it != instances.end()) {
        return it->second;  // Return the existing instance
    }

    // If it doesn't exist, create a new instance
    MidasEventReceiver* newReceiver = new MidasEventReceiver(midasEventBufferName);
    instances[midasEventBufferName] = newReceiver;  // Store the new instance in the map
    return newReceiver;
}

// Init method to set additional parameters with default values
void MidasEventReceiver::init(const std::string& host, 
                              const std::string& exp,
                              const std::string& clientName, 
                              int eventID, 
                              bool getAllEvents, 
                              size_t maxBufferSize, 
                              int cmYieldTimeout) {
    setHostName(host);
    setExpName(exp);
    setClientName(clientName);
    setEventID(eventID);
    setGetAllEvents(getAllEvents);
    setMaxBufferSize(maxBufferSize);
    setCmYieldTimeout(cmYieldTimeout);
}


// Override the run method for MidasEventReceiver
void MidasEventReceiver::run() {
    status = cm_connect_experiment(hostName.c_str(), expName.c_str(), clientName.c_str(), nullptr); // Set status during connection

    if (status != CM_SUCCESS) {
        cm_msg(MERROR, "MidasEventReceiver::run", "Failed to connect to experiment. Status: %d", status);
        listeningForData = false;
        return;
    }

    status = bm_open_buffer(midasEventBufferName.c_str(), MAX_EVENT_SIZE * 2, &hBufEvent); // Set status for buffer opening
    if (status != BM_SUCCESS) {
        cm_msg(MERROR, "MidasEventReceiver::run", "Failed to open buffer. Status: %d", status);
        listeningForData = false;
        return;
    }

    status = bm_set_cache_size(hBufEvent, 100000, 0); // Update status if cache size fails
    if (status != BM_SUCCESS) {
        cm_msg(MERROR, "MidasEventReceiver::run", "Failed to set cache size. Status: %d", status);
        listeningForData = false;
        return;
    }

    // Lock the buffer map mutex before accessing it
    {
        std::lock_guard<std::mutex> lock(bufferMapMutex);
        // Link the buffer to its name in the static map
        bufferMap[hBufEvent] = midasEventBufferName;
    }

    status = bm_request_event(hBufEvent, (WORD)eventID, TRIGGER_ALL, getAllEvents ? GET_ALL : GET_NONBLOCKING,
                              &requestID, MidasEventReceiver::processEventCallback); // Update status when event request is made

    if (status != BM_SUCCESS) {
        cm_msg(MERROR, "MidasEventReceiver::run", "Failed to request event. Status: %d", status);
        listeningForData = false;
        return;
    }
    // Worker loop for collecting events and filling the buffer
    while (running && (status != RPC_SHUTDOWN && status != SS_ABORT)) {
        status = cm_yield(cmYieldTimeout);
    }

    bm_close_buffer(hBufEvent);

    // Lock the buffer map mutex before modifying it
    {
        std::lock_guard<std::mutex> lock(bufferMapMutex);
        // Remove the buffer from the static map once processing is done
        bufferMap.erase(hBufEvent);
    }

    listeningForData = false;
}

void MidasEventReceiver::processEventCallback(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent) {
    // Lock the buffer map mutex to safely access the map
    std::lock_guard<std::mutex> lock(bufferMapMutex);

    // Look up the midasEventBufferName corresponding to the hBuf
    auto it = bufferMap.find(hBuf);
    if (it != bufferMap.end()) {
        const std::string& midasEventBufferName = it->second;
        
        // Get the correct receiver instance using the midasEventBufferName
        MidasEventReceiver* receiver = MidasEventReceiver::getInstance(midasEventBufferName);
        if (receiver) {
            // Call the receiver's processEvent method to handle the event
            receiver->processEvent(hBuf, request_id, pheader, pevent);
        } else {
            cm_msg(MERROR, "MidasEventReceiver::processEventCallback", "Receiver not found for hBuf: %p", hBuf);
        }
    } else {
        cm_msg(MERROR, "MidasEventReceiver::processEventCallback", "No receiver found for hBuf: %p in buffer map", hBuf);
    }
}

void MidasEventReceiver::processEvent(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent) {
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
    TMEvent tmEvent(pheader, size + sizeof(EVENT_HEADER));

    {
        std::lock_guard<std::mutex> lock(eventBufferMutex);
        if (eventBuffer.size() >= maxBufferSize) {
            eventBuffer.pop_front(); // Remove the oldest event to make space
        }

        // Create a ReceivedMidasEvent and push it as a shared_ptr
        auto newReceivedMidasEvent = std::make_shared<ReceivedMidasEvent>();
        newReceivedMidasEvent->timestamp = std::chrono::system_clock::now();
        newReceivedMidasEvent->event = tmEvent;  // Store the TMEvent

        eventBuffer.push_back(newReceivedMidasEvent);
        bufferCV.notify_all(); // Notify any waiting threads

    }
}


// Retrieve the latest N events
std::vector<ReceivedMidasEvent*> MidasEventReceiver::getLatestEvents(size_t n) {
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    std::vector<ReceivedMidasEvent*> events;

    // Ensure we fetch only the latest 'n' events from the deque
    size_t start = eventBuffer.size() > n ? eventBuffer.size() - n : 0;
    for (size_t i = start; i < eventBuffer.size(); ++i) {
        // Cast each element of the deque to a ReceivedMidasEvent if possible
        if (auto midasEvent = std::dynamic_pointer_cast<ReceivedMidasEvent>(eventBuffer[i])) {
            events.push_back(midasEvent.get());
        }
    }
    return events;
}

// Retrieve the latest N events since a specific timestamp
std::vector<ReceivedMidasEvent*> MidasEventReceiver::getLatestEvents(size_t n, std::chrono::system_clock::time_point since) {
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    std::vector<ReceivedMidasEvent*> filteredEvents;

    for (auto& event : eventBuffer) {
        // Cast each element of the deque to a ReceivedMidasEvent if possible
        if (auto midasEvent = std::dynamic_pointer_cast<ReceivedMidasEvent>(event)) {

            // Print the timestamp in a readable format
            if (midasEvent->timestamp > since) {
                filteredEvents.push_back(midasEvent.get());
            }
        }
    }

    size_t start = filteredEvents.size() > n ? filteredEvents.size() - n : 0;
    return std::vector<ReceivedMidasEvent*>(filteredEvents.begin() + start, filteredEvents.end());
}

// Retrieve events since a specific timestamp
std::vector<ReceivedMidasEvent*> MidasEventReceiver::getLatestEvents(std::chrono::system_clock::time_point since) {
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    std::vector<ReceivedMidasEvent*> events;

    for (auto& event : eventBuffer) {
        // Cast each element of the deque to a ReceivedMidasEvent if possible
        if (auto midasEvent = std::dynamic_pointer_cast<ReceivedMidasEvent>(event)) {
            if (midasEvent->timestamp > since) {
                events.push_back(midasEvent.get());
            }
        }
    }
    return events;
}

// Retrieve all events in the buffer
std::vector<ReceivedMidasEvent*> MidasEventReceiver::getEventsInBuffer() {
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    std::vector<ReceivedMidasEvent*> allEvents;

    for (auto& event : eventBuffer) {
        // Cast each element of the deque to a ReceivedMidasEvent if possible
        if (auto midasEvent = std::dynamic_pointer_cast<ReceivedMidasEvent>(event)) {
            allEvents.push_back(midasEvent.get());
        }
    }
    return allEvents;
}




// Setter for eventID
void MidasEventReceiver::setEventID(int newEventID) {
    eventID = newEventID;
}

// Getter for eventID
int MidasEventReceiver::getEventID() const {
    return eventID;
}

// Setter for getAllEvents
void MidasEventReceiver::setGetAllEvents(bool newGetAllEvents) {
    getAllEvents = newGetAllEvents;
}

// Getter for getAllEvents
bool MidasEventReceiver::getGetAllEvents() const {
    return getAllEvents;
}

