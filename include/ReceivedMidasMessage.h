#ifndef RECEIVED_MIDAS_MESSAGE_H
#define RECEIVED_MIDAS_MESSAGE_H

#include "ReceivedMidasData.h"

// Derived struct for Midas Message
struct ReceivedMidasMessage : public ReceivedMidasData {
    void* message;
};

#endif // RECEIVED_MIDAS_MESSAGE_H
