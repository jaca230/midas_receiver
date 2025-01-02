#include "MidasReceiverManager.h"
#include "MidasReceiver.h"

MidasReceiverManager& MidasReceiverManager::getInstance() {
    static MidasReceiverManager instance;
    return instance;
}

void MidasReceiverManager::registerReceiver(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Construct a new MidasReceiver instance dynamically
    receivers_[id] = new MidasReceiver(id);
}
void MidasReceiverManager::registerReceiver(MidasReceiver* receiver) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Use the receiver's ID to register it
    receivers_[receiver->getId()] = receiver;
}

void MidasReceiverManager::deregisterReceiver(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    receivers_.erase(id);
}

MidasReceiver* MidasReceiverManager::getReceiver(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = receivers_.find(id);
    return (it != receivers_.end()) ? it->second : nullptr;
}

void MidasReceiverManager::processEventCallback(HNDLE hBuf, HNDLE request_id, EVENT_HEADER* pheader, void* pevent) {
    MidasReceiver* receiver = MidasReceiverManager::getInstance().getReceiver(request_id);
    if (receiver) {
        receiver->processEvent(hBuf, request_id, pheader, pevent);
    }
}

void MidasReceiverManager::processMessageCallback(HNDLE hBuf, HNDLE id, EVENT_HEADER* pheader, void* message) {
    MidasReceiver* receiver = MidasReceiverManager::getInstance().getReceiver(id);
    if (receiver) {
        receiver->processMessage(hBuf, id, pheader, message);
    }
}

INT MidasReceiverManager::processTransitionCallback(INT run_number, char* error) {
    // Assuming we have a way to determine which receiver should handle this
    // For simplicity, this example assumes all receivers handle the same transition
    auto& manager = MidasReceiverManager::getInstance();
    for (auto& [id, receiver] : manager.receivers_) {
        if (receiver) {
            return receiver->processTransition(run_number, error);
        }
    }
    return 0; // Default return
}
