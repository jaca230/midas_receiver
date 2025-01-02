#include "MidasReceiver.h"
#include <iostream>


MidasReceiver::MidasReceiver(const std::string& host,
                             const std::string& exp,
                             const std::string& clientName, 
                             size_t maxBufferSize, 
                             int cmYieldTimeout)
    : hostName(host),
      expName(exp),
      clientName(clientName.empty() ? "Midas Consumer" : clientName), 
      maxBufferSize(maxBufferSize), 
      cmYieldTimeout(cmYieldTimeout), 
      running(false), 
      listeningForData(false),
      status(0) {
    // Fallback to environment values if host is empty
    /*
    if (hostName.empty()) {
        char host_name[HOST_NAME_LENGTH], expt_name[NAME_LENGTH];
        cm_get_environment(host_name, sizeof(host_name), expt_name, sizeof(expt_name));
        hostName = host_name;
    }
    if (expName.empty()) {
        char host_name[HOST_NAME_LENGTH], expt_name[NAME_LENGTH];
        cm_get_environment(host_name, sizeof(host_name), expt_name, sizeof(expt_name));
        expName = expt_name;
    }
    */
}

MidasReceiver::~MidasReceiver() = default;


// Default implementation for starting event reception
void MidasReceiver::start() {
    if (!running) {
        running = true;
        listeningForData = true;
        workerThread = std::thread(&MidasReceiver::run, this);
    }
}

// Default implementation for stopping event reception
void MidasReceiver::stop() {
    if (running) {
        if (workerThread.joinable()) {
            workerThread.join();
        }
        cm_disconnect_experiment();  // Assuming cm_disconnect_experiment is defined elsewhere
        running = false;
    }
}


void MidasReceiver::run() {
    // Override this in derived classes for actual event listening behavior
}

// Retrieve the latest N events
std::vector<std::shared_ptr<ReceivedMidasData>> MidasReceiver::getLatestData(size_t n) {
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    std::vector<std::shared_ptr<ReceivedMidasData>> events;

    // Ensure we fetch only the latest 'n' events from the deque
    size_t start = eventBuffer.size() > n ? eventBuffer.size() - n : 0;
    for (size_t i = start; i < eventBuffer.size(); ++i) {
        events.push_back(eventBuffer[i]);  // Use shared_ptr directly
    }
    return events;
}

// Retrieve the latest N events since a specific timestamp
std::vector<std::shared_ptr<ReceivedMidasData>> MidasReceiver::getLatestData(size_t n, std::chrono::system_clock::time_point since) {
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    std::vector<std::shared_ptr<ReceivedMidasData>> filteredEvents;

    for (auto& event : eventBuffer) {
        if (event->timestamp > since) {
            filteredEvents.push_back(event);  // Add shared_ptr if event matches
        }
    }

    size_t start = filteredEvents.size() > n ? filteredEvents.size() - n : 0;
    return std::vector<std::shared_ptr<ReceivedMidasData>>(filteredEvents.begin() + start, filteredEvents.end());
}

// Retrieve events since a specific timestamp
std::vector<std::shared_ptr<ReceivedMidasData>> MidasReceiver::getLatestData(std::chrono::system_clock::time_point since) {
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    std::vector<std::shared_ptr<ReceivedMidasData>> events;

    for (auto& event : eventBuffer) {
        if (event->timestamp > since) {
            events.push_back(event);  // Directly push shared_ptr to the event
        }
    }
    return events;
}

// Retrieve all data currently in the buffer
std::vector<std::shared_ptr<ReceivedMidasData>> MidasReceiver::getDataInBuffer() {
    std::lock_guard<std::mutex> lock(eventBufferMutex);
    std::vector<std::shared_ptr<ReceivedMidasData>> events;

    for (auto& event : eventBuffer) {
        events.push_back(event);  // Push shared_ptr to the vector
    }
    return events;
}



// Accessor methods
const std::string& MidasReceiver::getHostName() const {
    return hostName;
}

const std::string& MidasReceiver::getExpName() const {
    return expName;
}


const std::string& MidasReceiver::getClientName() const {
    return clientName;
}

size_t MidasReceiver::getMaxBufferSize() const {
    return maxBufferSize;
}

int MidasReceiver::getCmYieldTimeout() const {
    return cmYieldTimeout;
}

// Getter for listeningForData
bool MidasReceiver::isListeningForData() const {
    return listeningForData;
}

// Setter methods
void MidasReceiver::setHostName(const std::string& host) {
    if (host.empty()) {
        char host_name[HOST_NAME_LENGTH], expt_name[NAME_LENGTH];
        cm_get_environment(host_name, sizeof(host_name), expt_name, sizeof(expt_name));
        hostName = host_name;
    } else {
        hostName = host;
    }
}

void MidasReceiver::setClientName(const std::string& client) {
    clientName = client;
}

void MidasReceiver::setExpName(const std::string& exp) {
    if (exp.empty()) {
        char host_name[HOST_NAME_LENGTH], expt_name[NAME_LENGTH];
        cm_get_environment(host_name, sizeof(host_name), expt_name, sizeof(expt_name));
        expName = expt_name;
    } else {
        expName = exp;
    }
}

void MidasReceiver::setMaxBufferSize(size_t size) {
    maxBufferSize = size;
}

void MidasReceiver::setCmYieldTimeout(int timeout) {
    cmYieldTimeout = timeout;
}

// Setter for listeningForData
void MidasReceiver::setListeningForData(bool listening) {
    listeningForData = listening;
}
