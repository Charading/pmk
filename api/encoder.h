#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <stdbool.h>

// Initialize encoder GPIOs (uses pins from config.h)
void encoder_init(void);

// Poll encoder; returns signed step count (positive = CW, negative = CCW)
int encoder_poll(void);

// Return true if encoder switch was just pressed (edge-detected)
bool encoder_switch_pressed(void);

#endif // ENCODER_H
