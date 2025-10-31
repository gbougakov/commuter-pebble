#pragma once

#include <pebble.h>

// Initialize API handler (register AppMessage callbacks)
void api_handler_init(MenuLayer *menu_layer);

// Request train data from JavaScript
void api_handler_request_train_data(void);

// Request detail data for selected departure
void api_handler_request_detail_data(void);

// Handle timeout (called by timeout timer)
void api_handler_handle_timeout(void);
