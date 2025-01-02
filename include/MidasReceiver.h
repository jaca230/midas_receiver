#ifndef MIDAS_RECEIVER_H
#define MIDAS_RECEIVER_H

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <thread>
#include <deque>
#include <memory>
#include "midas.h"
#include "midasio.h"
#include "ReceivedMidasData.h"


class MidasReceiver {
public:
    // Constructor for common parameters
    MidasReceiver(const std::string& host,
                  const std::string& exp,
                  const std::string& clientName, 
                  size_t maxBufferSize, 
                  int cmYieldTimeout);

    // Virtual destructor for proper cleanup in derived classes
    virtual ~MidasReceiver();

    // Virtual methods to be overridden by derived classes
    virtual void start();
    virtual void stop();
    virtual void run();

    // Accessor methods
    const std::string& getHostName() const;
    const std::string& getExpName() const;
    const std::string& getClientName() const;
    size_t getMaxBufferSize() const;
    int getCmYieldTimeout() const;
    bool isListeningForData() const;

    // Setter methods
    void setHostName(const std::string& host);
    void setExpName(const std::string& exp);
    void setClientName(const std::string& client);
    void setMaxBufferSize(size_t size);
    void setCmYieldTimeout(int timeout);
    void setListeningForData(bool listening);

    // Methods to get the latest events (ReceivedMidasData)
    virtual std::vector<std::shared_ptr<ReceivedMidasData>> getLatestData(size_t n);
    virtual std::vector<std::shared_ptr<ReceivedMidasData>> getLatestData(size_t n, std::chrono::system_clock::time_point since);
    virtual std::vector<std::shared_ptr<ReceivedMidasData>> getLatestData(std::chrono::system_clock::time_point since);
    virtual std::vector<std::shared_ptr<ReceivedMidasData>> getDataInBuffer();



protected:
    std::string hostName;
    std::string expName;
    std::string clientName;
    size_t maxBufferSize;
    int cmYieldTimeout;
    bool running;
    bool listeningForData;
    int status;

    std::thread workerThread;

    // Buffer to store ReceivedMidasData objects
    std::deque<std::shared_ptr<ReceivedMidasData>> eventBuffer;
    
    // Mutex for thread safety when accessing the eventBuffer
    std::mutex eventBufferMutex;
};

#endif // MIDAS_RECEIVER_H
