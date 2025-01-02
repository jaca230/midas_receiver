#ifndef MIDAS_EVENT_RECEIVER_H
#define MIDAS_EVENT_RECEIVER_H

#include "MidasReceiver.h"

#include <map>
#include <mutex>
#include <condition_variable>
#include "ReceivedMidasEvent.h"


class MidasEventReceiver : public MidasReceiver {
public:
    // Static method to get the singleton instance
    static MidasEventReceiver* getInstance(const std::string& midasEventBufferName);

    // Init function to configure additional parameters with default values
    void init(const std::string& host = "",
              const std::string& exp = "", 
              const std::string& clientName = "Event Receiver", 
              int eventID = EVENTID_ALL, 
              bool getAllEvents = true, 
              size_t maxBufferSize = 1000, 
              int cmYieldTimeout = 300);

    // Override getLatestData methods to return ReceivedMidasEvent objects
    std::vector<ReceivedMidasEvent*> getLatestEvents(size_t n);
    std::vector<ReceivedMidasEvent*> getLatestEvents(size_t n, std::chrono::system_clock::time_point since);
    std::vector<ReceivedMidasEvent*> getLatestEvents(std::chrono::system_clock::time_point since);
    std::vector<ReceivedMidasEvent*> getEventsInBuffer();

    // Setters and Getters for eventID and getAllEvents
    void setEventID(int newEventID);
    int getEventID() const;

    void setGetAllEvents(bool newGetAllEvents);
    bool getGetAllEvents() const;

    // Prevent copying
    MidasEventReceiver(const MidasEventReceiver&) = delete;
    MidasEventReceiver& operator=(const MidasEventReceiver&) = delete;

private:
    // Private constructor to enforce singleton pattern
    MidasEventReceiver(const std::string& midasEventBufferName);

    // Static map to hold the instances by midasEventBufferName
    static std::map<std::string, MidasEventReceiver*> instances;
    
    // Mutex for thread safety when accessing the static map
    static std::mutex instanceMutex;

    // Static callback method for processing events
    static void processEventCallback(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent);
    void processEvent(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent);

    // Static map to associate hBufEvent with midasEventBufferName
    static std::map<HNDLE, std::string> bufferMap;

    // Mutex for thread safety when accessing bufferMap
    static std::mutex bufferMapMutex;

    std::string midasEventBufferName;
    int eventID;
    bool getAllEvents;

    HNDLE hBufEvent;
    INT requestID;

    std::vector<int> serialNumbers = std::vector<int>(10, 0);
    bool firstEvent = true;
    size_t countMismatches = 0;
    size_t eventByteCount = 0;
    std::condition_variable bufferCV;

    // Override run method for specific event reception logic
    void run() override;
};

#endif // MIDAS_EVENT_RECEIVER_H
