#include <pebble_worker.h>

// Message type for requesting glance update
// This must match the value in package.json messageKeys
#define WORKER_REQUEST_GLANCE 100

// Tick counter for periodic updates
static uint32_t s_tick_count = 0;

// Update interval in minutes (how often to refresh glances)
#define UPDATE_INTERVAL_MINUTES 10

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_tick_count++;

  // Every UPDATE_INTERVAL_MINUTES, request a glance update
  if (s_tick_count % UPDATE_INTERVAL_MINUTES == 0) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Worker requesting glance update (every %d minutes)", UPDATE_INTERVAL_MINUTES);

    // Construct a message packet
    AppWorkerMessage msg_data = {
      .data0 = 1  // Simple flag indicating update request
    };

    // Send message to PebbleKit JS (via app if running, or triggers app launch)
    app_worker_send_message(WORKER_REQUEST_GLANCE, &msg_data);
  }
}

static void worker_init(void) {
  APP_LOG(APP_LOG_LEVEL_INFO, "NMBS Background Worker initialized");

  // Subscribe to minute tick timer
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
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
