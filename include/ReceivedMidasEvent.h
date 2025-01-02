#ifndef RECEIVED_MIDAS_EVENT_H
#define RECEIVED_MIDAS_EVENT_H

#include "ReceivedMidasData.h"

// Derived struct for Midas Event
struct ReceivedMidasEvent : public ReceivedMidasData {
    TMEvent event; // Specific to Midas Event
};

#endif // RECEIVED_MIDAS_EVENT_H
