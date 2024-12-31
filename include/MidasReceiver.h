#ifndef MIDAS_RECEIVER_H
#define MIDAS_RECEIVER_H

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

class MidasReceiver {
public:
    struct TimedEvent {
        std::chrono::system_clock::time_point timestamp; // TMEvent has a timestamp, but it's only accurate to seconds
        TMEvent event;
    };
    struct TimedMessage {
        std::chrono::system_clock::time_point timestamp;
        void* message; // Message data
    };
    struct TimedTransition {
        std::chrono::system_clock::time_point timestamp;
        INT run_number;
        char error[256]; // Assuming a fixed-size error message
    };

    static MidasReceiver& getInstance();

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

    std::vector<TimedMessage> getMessageBuffer(); // Retrieve all messages with timestamps
    std::vector<TimedMessage> getLatestMessages(size_t n);
    std::vector<TimedMessage> getLatestMessages(std::chrono::system_clock::time_point since);
    std::vector<TimedMessage> getLatestMessages(size_t n, std::chrono::system_clock::time_point since);

    std::vector<TimedTransition> getTransitionBuffer(); // Retrieve all transitions with timestamps
    std::vector<TimedTransition> getLatestTransitions(size_t n);
    std::vector<TimedTransition> getLatestTransitions(std::chrono::system_clock::time_point since);
    std::vector<TimedTransition> getLatestTransitions(size_t n, std::chrono::system_clock::time_point since);

    INT getStatus() const;
    bool isListeningForEvents() const;

private:
    MidasReceiver();
    ~MidasReceiver();

    static void processEventCallback(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent);
    static void processMessageCallback(HNDLE hBuf, HNDLE id, EVENT_HEADER* pheader, void* message);
    static INT processTransitionCallback(INT run_number, char* error);

    void run();
    void processEvent(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent);
    void processMessage(HNDLE hBuf, HNDLE id, EVENT_HEADER* pheader, void* message);
    INT processTransition(INT run_number, char* error);

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

    // Buffers and mutexes
    std::deque<TimedEvent> eventBuffer;
    std::deque<TimedMessage> messageBuffer;
    std::deque<TimedTransition> transitionBuffer;
    
    std::mutex eventBufferMutex;
    std::mutex messageBufferMutex;
    std::mutex transitionBufferMutex;

    std::condition_variable bufferCV;

    std::thread workerThread;
    std::atomic<bool> running;
    std::atomic<bool> listeningForEvents;

    static MidasReceiver* instance;
    static std::mutex initMutex;

    mutable std::mutex statusMutex;
    INT status;
};

#endif // MIDAS_RECEIVER_H
