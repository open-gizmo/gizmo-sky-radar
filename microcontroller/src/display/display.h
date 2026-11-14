#ifndef DISPLAY_H
#define DISPLAY_H

// The round display: the radar, and the screens of the setup and the factory reset

// Start the display and play the boot animation (call first, while the memory is free for the frame)
void setupDisplay();

// Draw the next frame when it's due (call on every loop iteration)
void updateDisplay();

// Show the countdown of the factory reset while the BOOT button is held, and hide it when it's released
void showFactoryResetCountdown(unsigned long remainingMs);
void hideFactoryResetCountdown();

// Show that the factory reset is happening (the device restarts right after)
void showFactoryResetDone();

#endif
