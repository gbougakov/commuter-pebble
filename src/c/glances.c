#include "glances.h"
#include "state.h"
#include "api_handler.h"

// AppGlance update callback (only on platforms with AppGlance support)
#if defined(PBL_HEALTH)
static void update_app_glance(AppGlanceReloadSession *session, size_t limit, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Updating AppGlance (limit: %zu, departures: %d)",
          limit, state_get_num_departures());

  // Check if we have space for at least one slice
  if (limit < 1) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "AppGlance limit too low: %zu", limit);
    return;
  }

  // If no trains available
  if (state_get_num_departures() == 0) {
    return;
  }

  // Add glances for each departure
  int max_slices = limit < state_get_num_departures() ? limit : state_get_num_departures();
  TrainDeparture *departures = state_get_departures();

  for (int i = 0; i < max_slices; i++) {
    TrainDeparture *dep = &departures[i];

    char subtitle[80];
    if (dep->depart_delay > 0) {
      snprintf(subtitle, sizeof(subtitle), "%s (+%d) • Plat. %s • %s",
               dep->depart_time, dep->depart_delay, dep->platform, dep->destination);
    } else {
      snprintf(subtitle, sizeof(subtitle), "%s • Plat. %s • %s",
               dep->depart_time, dep->platform, dep->destination);
    }

    AppGlanceSlice slice = {
      .layout = {
        .subtitle_template_string = subtitle
      },
      .expiration_time = dep->depart_timestamp
    };

    AppGlanceResult result = app_glance_add_slice(session, slice);
    if (result != APP_GLANCE_RESULT_SUCCESS) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to add train slice %d: %d", i, result);
    }
  }

  APP_LOG(APP_LOG_LEVEL_INFO, "AppGlance updated with %d train slices", max_slices);
}
#endif  // PBL_HEALTH

// Update app glances (handles platform checks internally)
void glances_update(void) {
  #if defined(PBL_HEALTH)
    app_glance_reload(update_app_glance, NULL);
  #endif
}

// Update glances on app exit
void glances_update_on_exit(void) {
  #if defined(PBL_HEALTH)
    if (state_get_num_departures() > 0) {
      APP_LOG(APP_LOG_LEVEL_INFO, "Updating glances on app exit (departures: %d)",
              state_get_num_departures());
      app_glance_reload(update_app_glance, NULL);
    } else {
      APP_LOG(APP_LOG_LEVEL_INFO, "No departures to show in glances");
    }
  #endif
}

// Worker message handler
static void worker_message_handler(uint16_t type, AppWorkerMessage *data) {
  if (type == WORKER_REQUEST_GLANCE) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Worker requesting glance update");
    state_set_background_update(true);
    api_handler_request_train_data();  // Reuse existing function!
  }
}

// Handle worker message requesting glance update
void glances_handle_worker_request(void) {
  // Subscribe to worker messages for background glance updates
  app_worker_message_subscribe(worker_message_handler);
}
