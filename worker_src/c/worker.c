#include <pebble_worker.h>

// Message type for requesting glance update
// This must match the value in package.json messageKeys
#define WORKER_REQUEST_GLANCE 100

// Counter for tracking minutes since last update
static uint8_t s_minutes_since_update = 0;

// Update interval in minutes (how often to refresh glances)
#define UPDATE_INTERVAL_MINUTES 10

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Increment minute counter
  s_minutes_since_update++;

  // Every UPDATE_INTERVAL_MINUTES, request a glance update
  if (s_minutes_since_update >= UPDATE_INTERVAL_MINUTES) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Worker requesting glance update (every %d minutes)", UPDATE_INTERVAL_MINUTES);

    // Construct a message packet
    AppWorkerMessage msg_data = {
      .data0 = 1  // Simple flag indicating update request
    };

    // Send message to PebbleKit JS (via app if running, or triggers app launch)
    app_worker_send_message(WORKER_REQUEST_GLANCE, &msg_data);

    // Reset counter
    s_minutes_since_update = 0;
  }
}

static void worker_init(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "NMBS Background Worker initialized");

  // Subscribe to minute tick timer
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Request immediate update on worker start
  AppWorkerMessage msg_data = {.data0 = 1};
  app_worker_send_message(WORKER_REQUEST_GLANCE, &msg_data);
}

static void worker_deinit(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "NMBS Background Worker deinitialized");

  // Unsubscribe from tick timer
  tick_timer_service_unsubscribe();
}

int main(void) {
  worker_init();
  worker_event_loop();
  worker_deinit();
}
