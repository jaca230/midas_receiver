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

// Struct to hold transition and sequence number for registration ordering
struct TransitionRegistration {
    int transition; // e.g. TR_START, TR_STOP, etc.
    int sequence;   // registration ordering sequence
};

// Configuration struct for MidasReceiver initialization parameters
struct MidasReceiverConfig {
    std::string host = "";
    std::string experiment = "";
    std::string bufferName = "SYSTEM";
    std::string clientName = "Event Receiver";
    int eventID = EVENTID_ALL;
    bool getAllEvents = true;
    size_t maxBufferSize = 1000;
    int cmYieldTimeout = 300;

    // Default transitions with preferred registration order
    std::vector<TransitionRegistration> transitionRegistrations {
        {TR_START, 100},
        {TR_STOP, 900},
        {TR_PAUSE, 100},
        {TR_RESUME, 100},
        {TR_STARTABORT, 500}
    };
};

class MidasReceiver {
public:
    struct TimedEvent {
        std::chrono::system_clock::time_point timestamp; // More precise timestamp than TMEvent
        TMEvent event;
    };

    struct TimedMessage {
        std::chrono::system_clock::time_point timestamp;
        void* message; // Pointer to message data
    };

    struct TimedTransition {
        std::chrono::system_clock::time_point timestamp;
        INT run_number;
        char error[256]; // Fixed-size error message buffer
    };

    static MidasReceiver& getInstance();

    // Initialize receiver with config struct including transition registrations
    void init(const MidasReceiverConfig& config);

    void start();
    void stop();

    std::vector<TimedEvent> getWholeBuffer();
    std::vector<TimedEvent> getLatestEvents(size_t n);
    std::vector<TimedEvent> getLatestEvents(std::chrono::system_clock::time_point since);
    std::vector<TimedEvent> getLatestEvents(size_t n, std::chrono::system_clock::time_point since);

    std::vector<TimedMessage> getMessageBuffer();
    std::vector<TimedMessage> getLatestMessages(size_t n);
    std::vector<TimedMessage> getLatestMessages(std::chrono::system_clock::time_point since);
    std::vector<TimedMessage> getLatestMessages(size_t n, std::chrono::system_clock::time_point since);

    std::vector<TimedTransition> getTransitionBuffer();
    std::vector<TimedTransition> getLatestTransitions(size_t n);
    std::vector<TimedTransition> getLatestTransitions(std::chrono::system_clock::time_point since);
    std::vector<TimedTransition> getLatestTransitions(size_t n, std::chrono::system_clock::time_point since);

    INT getStatus() const;
    bool isListeningForEvents() const;
    bool IsRunning() const;

private:
    MidasReceiver();
    ~MidasReceiver();

    // Static callbacks used internally by MIDAS system
    static void processEventCallback(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent);
    static void processMessageCallback(HNDLE hBuf, HNDLE id, EVENT_HEADER* pheader, void* message);
    static INT processTransitionCallback(INT run_number, char* error);

    // Internal worker thread main loop and event/message/transition handlers
    void run();
    void processEvent(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent);
    void processMessage(HNDLE hBuf, HNDLE id, EVENT_HEADER* pheader, void* message);
    INT processTransition(INT run_number, char* error);

    // Configuration parameters
    std::string hostName;
    std::string exptName;
    std::string bufferName;
    std::string clientName;
    int eventID;
    bool getAllEvents;
    size_t maxBufferSize;
    int cmYieldTimeout;

    // MIDAS handles
    HNDLE hBufEvent;
    INT requestID;

    // Internal bookkeeping
    std::vector<int> serialNumbers = std::vector<int>(10, 0);
    bool firstEvent = true;
    size_t countMismatches = 0;
    size_t eventByteCount = 0;

    // Buffers and associated synchronization
    std::deque<TimedEvent> eventBuffer;
    std::deque<TimedMessage> messageBuffer;
    std::deque<TimedTransition> transitionBuffer;

    std::mutex eventBufferMutex;
    std::mutex messageBufferMutex;
    std::mutex transitionBufferMutex;

    std::condition_variable bufferCV;

    // Worker thread and state flags
    std::thread workerThread;
    std::atomic<bool> running;
    std::atomic<bool> listeningForEvents;

    // Transition registrations configured at init
    std::vector<TransitionRegistration> transitionRegistrations_;

    // Singleton instance control
    static MidasReceiver* instance;
    static std::mutex initMutex;

    mutable std::mutex statusMutex;
    INT status;
};

#endif // MIDAS_RECEIVER_H
