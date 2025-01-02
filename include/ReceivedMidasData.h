#ifndef RECEIVED_MIDAS_DATA_H
#define RECEIVED_MIDAS_DATA_H

#include <chrono>
#include "midas.h"
#include "midasio.h"

// Base struct for all Midas data types
struct ReceivedMidasData {
    std::chrono::system_clock::time_point timestamp; // Common timestamp for all derived structs

    // Make the base struct polymorphic by adding a virtual destructor
    virtual ~ReceivedMidasData() = default; // Virtual destructor to make it polymorphic
};

#endif // RECEIVED_MIDAS_DATA_H
