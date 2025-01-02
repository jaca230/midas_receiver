#ifndef MIDAS_RECEIVER_MANAGER_H
#define MIDAS_RECEIVER_MANAGER_H

#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include "MidasReceiver.h"
#include "midas.h"
#include "midasio.h"

class MidasReceiver;

class MidasReceiverManager {
public:
    static MidasReceiverManager& getInstance();

    void registerReceiver(int id);
    void registerReceiver(MidasReceiver* receiver);
    void deregisterReceiver(int id);
    MidasReceiver* getReceiver(int id);

    // Static callback handlers
    static void processEventCallback(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent);
    static void processMessageCallback(HNDLE hBuf, HNDLE id, EVENT_HEADER* pheader, void* message);
    static INT processTransitionCallback(INT run_number, char* error);

private:
    MidasReceiverManager() = default;
    ~MidasReceiverManager() = default;

    std::mutex mutex_;
    std::unordered_map<int, MidasReceiver*> receivers_;
};

#endif // MIDAS_RECEIVER_MANAGER_H
