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
#include <memory>
#include "midas.h"
#include "midasio.h"

struct TransitionRegistration {
    int transition;
    int sequence;
};

struct MidasReceiverConfig {
    std::string host = "";
    std::string experiment = "";
    std::string bufferName = "SYSTEM";
    std::string clientName = "Event Receiver";
    int eventID = EVENTID_ALL;
    bool getAllEvents = true;
    size_t maxBufferSize = 1000;
    int cmYieldTimeout = 300;
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
        std::chrono::system_clock::time_point timestamp;
        std::shared_ptr<TMEvent> event;
    };

    struct TimedMessage {
        std::chrono::system_clock::time_point timestamp;
        void* message;
    };

    struct TimedTransition {
        std::chrono::system_clock::time_point timestamp;
        INT run_number;
        char error[256];
    };

    static MidasReceiver& getInstance();

    void init(const MidasReceiverConfig& config);
    void start();
    void stop();

    std::vector<std::shared_ptr<TimedEvent>> getWholeBuffer();
    std::vector<std::shared_ptr<TimedEvent>> getLatestEvents(size_t n);
    std::vector<std::shared_ptr<TimedEvent>> getLatestEvents(std::chrono::system_clock::time_point since);
    std::vector<std::shared_ptr<TimedEvent>> getLatestEvents(size_t n, std::chrono::system_clock::time_point since);

    std::vector<TimedMessage> getMessageBuffer();
    std::vector<TimedMessage> getLatestMessages(size_t n);
    std::vector<TimedMessage> getLatestMessages(std::chrono::system_clock::time_point since);
    std::vector<TimedMessage> getLatestMessages(size_t n, std::chrono::system_clock::time_point since);

    std::vector<TimedTransition> getTransitionBuffer();
    std::vector<TimedTransition> getLatestTransitions(size_t n);
    std::vector<TimedTransition> getLatestTransitions(std::chrono::system_clock::time_point since);
    std::vector<TimedTransition> getLatestTransitions(size_t n, std::chrono::system_clock::time_point since);

    std::string getOdb(const std::string& path = "/");

    INT getStatus() const;
    bool isListeningForEvents() const;
    bool IsRunning() const;

private:
    MidasReceiver();
    ~MidasReceiver();

    static void processEventCallback(HNDLE, HNDLE, EVENT_HEADER*, void*);
    static void processMessageCallback(HNDLE, HNDLE, EVENT_HEADER*, void*);
    static INT  processTransitionCallback(INT, char*);

    void run();
    void processEvent(HNDLE, HNDLE, EVENT_HEADER*, void*);
    void processMessage(HNDLE, HNDLE, EVENT_HEADER*, void*);
    INT  processTransition(INT, char*);

    std::string hostName, exptName, bufferName, clientName;
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

    std::deque<std::shared_ptr<TimedEvent>> eventBuffer;
    std::deque<TimedMessage> messageBuffer;
    std::deque<TimedTransition> transitionBuffer;

    std::mutex eventBufferMutex, messageBufferMutex, transitionBufferMutex;
    std::condition_variable bufferCV;

    std::thread workerThread;
    std::atomic<bool> running;
    std::atomic<bool> listeningForEvents;

    std::vector<TransitionRegistration> transitionRegistrations_;

    static MidasReceiver* instance;
    static std::mutex initMutex;

    mutable std::mutex statusMutex;
    INT status;
};

#endif
