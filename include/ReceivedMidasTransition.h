#ifndef RECEIVED_MIDAS_TRANSITION_H
#define RECEIVED_MIDAS_TRANSITION_H

#include "ReceivedMidasData.h"

// Derived struct for Midas Transition
struct ReceivedMidasTransition : public ReceivedMidasData {
    INT run_number;
    char error[256];
};

#endif // RECEIVED_MIDAS_TRANSITION_H
