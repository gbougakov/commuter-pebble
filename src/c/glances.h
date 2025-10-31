#pragma once

#include <pebble.h>

// Update app glances (handles platform checks internally)
void glances_update(void);

// Update glances on app exit
void glances_update_on_exit(void);

// Handle worker message requesting glance update
void glances_handle_worker_request(void);
