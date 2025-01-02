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
    // Constructor for common parameters
    MidasReceiver(const std::string& host, 
                  const std::string& bufferName, 
                  const std::string& clientName, 
                  size_t maxBufferSize, 
                  int cmYieldTimeout);

    // Virtual destructor for proper cleanup in derived classes
    virtual ~MidasReceiver();

    // Virtual methods to be overridden by derived classes
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void run() = 0;

    // Accessor methods
    const std::string& getHostName() const;
    const std::string& getBufferName() const;
    const std::string& getClientName() const;
    size_t getMaxBufferSize() const;
    int getCmYieldTimeout() const;

    // Setter methods
    void setHostName(const std::string& host);
    void setBufferName(const std::string& buffer);
    void setClientName(const std::string& client);
    void setMaxBufferSize(size_t size);
    void setCmYieldTimeout(int timeout);

protected:
    std::string hostName;
    std::string bufferName;
    std::string clientName;
    size_t maxBufferSize;
    int cmYieldTimeout;
    bool running;
    bool listeningForEvents;
    int status;
};

#endif // MIDAS_RECEIVER_H

