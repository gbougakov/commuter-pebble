#pragma once

#include <pebble.h>

// Create and show detail window
void detail_window_show(void);

// Destroy detail window
void detail_window_destroy(void);

// Update detail window content (triggers redraw)
void detail_window_update(void);

// Get detail window instance (for checking if it's on stack)
Window* detail_window_get_instance(void);
