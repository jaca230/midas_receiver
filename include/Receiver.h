#ifndef RECEIVER_H
#define RECEIVER_H

#include <thread>
#include <atomic>
#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <string>
#include "midas.h"

class Receiver {
public:
    // Get the singleton instance
    static Receiver& getInstance();

    // Initialize the receiver with parameters (only the first time)
    void init(const std::string& host = "", 
              const std::string& bufferName = "SYSTEM", 
              const std::string& clientName = "Event Receiver", 
              int eventID = EVENTID_ALL, 
              bool getAllEvents = true, 
              size_t maxBufferSize = 1000);

    // Start and stop the receiver
    void start();
    void stop();

    // Buffer retrieval methods
    std::vector<std::vector<char>> getAllLatest(size_t& lastIndex);
    std::vector<std::vector<char>> getNLatest(size_t n);
    std::vector<std::vector<char>> getWholeBuffer();

    // Getter for status (thread-safe)
    INT getStatus() const;

    // Getter for running state (thread-safe)
    bool isListeningForEvents() const;

private:
    // Constructor is private to enforce singleton pattern
    Receiver();

    // Destructor
    ~Receiver();

    // Add the static callback function declaration
    static void processEventCallback(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent);

    // Main worker thread
    void run();

    // Event processing
    void processEvent(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent);

    // Configuration parameters
    std::string hostName;
    std::string bufferName;
    std::string clientName;
    int eventID;
    bool getAllEvents;
    size_t maxBufferSize;

    // MIDAS buffer management
    HNDLE hBufEvent;
    INT requestID;

    // Event integrity checks
    std::vector<int> serialNumbers = std::vector<int>(10, 0); // Serial numbers for each event ID (0-9)
    bool firstEvent = true;                                  // Flag to track the first event
    size_t countMismatches = 0;                              // Count of serial number mismatches
    size_t eventByteCount = 0;                               // Accumulated byte count

    // FIFO buffer
    std::deque<std::vector<char>> eventBuffer;
    std::mutex bufferMutex;
    std::condition_variable bufferCV;

    // Thread control
    std::thread workerThread;
    std::atomic<bool> running;
    std::atomic<bool> listeningForEvents;

    // Singleton instance
    static Receiver* instance;
    static std::mutex initMutex;

    // Status variable to track receiver status
    mutable std::mutex statusMutex;  // Protect access to status
    INT status;
};

#endif // RECEIVER_H
