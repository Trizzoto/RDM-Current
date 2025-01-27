#ifndef SIGNALS_H
#define SIGNALS_H

#include <stddef.h>
#include <stdint.h>

// Define the signal_t structure to hold signal data
typedef struct {
    const char* name;      // Signal name
    const char* unit;      // Unit of the signal (e.g., "rpm", "kPa")
    float scale;           // Scale factor for converting raw data
    uint32_t can_id;       // CAN ID associated with the signal
    uint8_t offset;        // Byte offset within the CAN message
    uint8_t length;        // Number of bytes for the signal
} signal_t;

// Declare the signals array and its size
extern const signal_t signals[];
extern const size_t NUM_SIGNALS;

#endif // SIGNALS_H

