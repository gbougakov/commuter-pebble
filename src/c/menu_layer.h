#pragma once

#include <pebble.h>
#include "types.h"

// Initialize menu layer (must be called after resources are loaded)
void menu_layer_init(MenuLayer *menu_layer, GBitmap *icon_switch, GBitmap *icon_switch_white,
                      GBitmap *icon_airport, GBitmap *icon_airport_white,
                      GBitmap *icon_start, GBitmap *icon_start_white,
                      GBitmap *icon_finish, GBitmap *icon_finish_white);

// Get menu layer callbacks (returns MenuLayerCallbacks struct)
MenuLayerCallbacks menu_layer_get_callbacks(void);
