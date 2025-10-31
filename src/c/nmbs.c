#include <pebble.h>
#include "types.h"
#include "state.h"
#include "utils.h"
#include "menu_layer.h"
#include "detail_window.h"
#include "api_handler.h"
#include "glances.h"

// UI elements
static Window *s_main_window = NULL;
static MenuLayer *s_menu_layer = NULL;
static StatusBarLayer *s_status_bar = NULL;

// Resources
static GBitmap *s_icon_switch = NULL;
static GBitmap *s_icon_switch_white = NULL;
static GBitmap *s_icon_airport = NULL;
static GBitmap *s_icon_airport_white = NULL;
static GBitmap *s_icon_start = NULL;
static GBitmap *s_icon_start_white = NULL;
static GBitmap *s_icon_finish = NULL;
static GBitmap *s_icon_finish_white = NULL;

// Main window lifecycle
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create StatusBarLayer
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorBlack, GColorWhite);
  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));

  // Calculate bounds for MenuLayer (below status bar)
  int16_t status_bar_height = STATUS_BAR_LAYER_HEIGHT;
  GRect menu_bounds = bounds;
  menu_bounds.origin.y = status_bar_height;
  menu_bounds.size.h -= status_bar_height;

  // Create MenuLayer
  s_menu_layer = menu_layer_create(menu_bounds);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);

  // Initialize menu layer module with icons
  menu_layer_init(s_menu_layer,
                   s_icon_switch, s_icon_switch_white,
                   s_icon_airport, s_icon_airport_white,
                   s_icon_start, s_icon_start_white,
                   s_icon_finish, s_icon_finish_white);

  // Set up MenuLayer callbacks
  menu_layer_set_callbacks(s_menu_layer, NULL, menu_layer_get_callbacks());

  // Add MenuLayer to window
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
}

static void window_unload(Window *window) {
  // Cancel marquee timer if running
  AppTimer *timer = state_get_marquee_timer();
  if (timer) {
    app_timer_cancel(timer);
    state_set_marquee_timer(NULL);
  }

  // Destroy MenuLayer
  menu_layer_destroy(s_menu_layer);

  // Destroy StatusBarLayer
  status_bar_layer_destroy(s_status_bar);
}

// App initialization
static void init(void) {
  // Load resources
  s_icon_switch = gbitmap_create_with_resource(RESOURCE_ID_ICON_SWITCH);
  s_icon_switch_white = gbitmap_create_with_resource(RESOURCE_ID_ICON_SWITCH_WHITE);
  s_icon_airport = gbitmap_create_with_resource(RESOURCE_ID_ICON_AIRPORT);
  s_icon_airport_white = gbitmap_create_with_resource(RESOURCE_ID_ICON_AIRPORT_WHITE);
  s_icon_start = gbitmap_create_with_resource(RESOURCE_ID_ICON_START);
  s_icon_start_white = gbitmap_create_with_resource(RESOURCE_ID_ICON_START_WHITE);
  s_icon_finish = gbitmap_create_with_resource(RESOURCE_ID_ICON_FINISH);
  s_icon_finish_white = gbitmap_create_with_resource(RESOURCE_ID_ICON_FINISH_WHITE);

  // Initialize state with default stations
  state_init();

  // Create main window
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  // Push window to stack
  const bool animated = true;
  window_stack_push(s_main_window, animated);

  // Initialize API handler (registers AppMessage callbacks)
  api_handler_init(s_menu_layer);

  // Subscribe to worker messages for background glance updates
  glances_handle_worker_request();

  // Don't request data immediately - wait for JavaScript to be ready
  // JavaScript will send stations or we'll use defaults, then request data
  APP_LOG(APP_LOG_LEVEL_DEBUG, "NMBS Schedule App initialized");
  APP_LOG(APP_LOG_LEVEL_INFO, "Waiting for JavaScript to send configuration...");
}

static void deinit(void) {
  // Destroy resources
  gbitmap_destroy(s_icon_switch);
  gbitmap_destroy(s_icon_switch_white);
  gbitmap_destroy(s_icon_airport);
  gbitmap_destroy(s_icon_airport_white);
  gbitmap_destroy(s_icon_start);
  gbitmap_destroy(s_icon_start_white);
  gbitmap_destroy(s_icon_finish);
  gbitmap_destroy(s_icon_finish_white);

  // Update glances before exiting
  glances_update_on_exit();

  // Unsubscribe from worker messages
  app_worker_message_unsubscribe();

  // Destroy detail window if it exists
  detail_window_destroy();

  // Destroy main window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
