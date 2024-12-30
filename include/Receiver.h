#ifndef RECEIVER_H
#define RECEIVER_H

#include <thread>
#include <atomic>
#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <string>
#include <chrono>
#include "midas.h"
#include "midasio.h"

class Receiver {
public:
    struct TimedEvent {
        std::chrono::system_clock::time_point timestamp; //TMEvent has a timestamp, but it's on accurate to seconds
        TMEvent event;
    };

    static Receiver& getInstance();

    void init(const std::string& host = "", 
              const std::string& bufferName = "SYSTEM", 
              const std::string& clientName = "Event Receiver", 
              int eventID = EVENTID_ALL, 
              bool getAllEvents = true, 
              size_t maxBufferSize = 1000, 
              int cmYieldTimeout = 300); // Default timeout is 300 ms

    void start();
    void stop();

    std::vector<TimedEvent> getWholeBuffer(); // Retrieve all events with timestamps
    std::vector<TimedEvent> getLatestEvents(size_t n); // Events excluding timestamps
    std::vector<TimedEvent> getLatestEvents(std::chrono::system_clock::time_point since);
    std::vector<TimedEvent> getLatestEvents(size_t n, std::chrono::system_clock::time_point since);

    INT getStatus() const;
    bool isListeningForEvents() const;

private:
    Receiver();
    ~Receiver();

    static void processEventCallback(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent);

    void run();
    void processEvent(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent);

    std::string hostName;
    std::string bufferName;
    std::string clientName;
    int eventID;
    bool getAllEvents;
    size_t maxBufferSize;
    int cmYieldTimeout;

    HNDLE hBufEvent;
    INT requestID;

    std::vector<int> serialNumbers = std::vector<int>(10, 0);
    bool firstEvent = true;
    size_t countMismatches = 0;
    size_t eventByteCount = 0;

    std::deque<TimedEvent> eventBuffer;
    std::mutex bufferMutex;
    std::condition_variable bufferCV;

    std::thread workerThread;
    std::atomic<bool> running;
    std::atomic<bool> listeningForEvents;

    static Receiver* instance;
    static std::mutex initMutex;

    mutable std::mutex statusMutex;
    INT status;
};

#endif // RECEIVER_H
