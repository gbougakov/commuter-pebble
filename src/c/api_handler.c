#include "api_handler.h"
#include "state.h"
#include "detail_window.h"
#include "glances.h"

// Menu layer reference (needed for reload)
static MenuLayer *s_menu_layer = NULL;

// Config timeout callback - fallback to defaults if no config received
static void config_timeout_callback(void *data) {
  state_set_config_timeout_timer(NULL);

  // If stations haven't been received yet, fall back to defaults
  if (!state_are_stations_received()) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Config timeout - falling back to default stations");
    state_load_default_stations();
    menu_layer_reload_data(s_menu_layer);

    // Request initial train data with default stations
    api_handler_request_train_data();
  }
}

// Timeout watchdog callback
static void loading_timeout_callback(void *data) {
  state_set_timeout_timer(NULL);

  if (state_get_load_state() == LOAD_STATE_CONNECTING ||
      state_get_load_state() == LOAD_STATE_FETCHING) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Loading timeout - transitioning to ERROR state");
    state_set_load_state(LOAD_STATE_ERROR);
    state_set_data_loading(false);
    state_set_data_failed(true);
    menu_layer_reload_data(s_menu_layer);
  }
}

// Request train data from JavaScript
void api_handler_request_train_data(void) {
  if (state_get_num_stations() == 0) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Cannot request data: no stations loaded");
    return;
  }

  // Generate unique request ID
  state_increment_data_request_id();

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  Station *stations = state_get_stations();
  dict_write_uint8(iter, MESSAGE_KEY_MESSAGE_TYPE, MSG_REQUEST_DATA);
  dict_write_cstring(iter, MESSAGE_KEY_FROM_STATION_ID,
                     stations[state_get_from_station_index()].irail_id);
  dict_write_cstring(iter, MESSAGE_KEY_TO_STATION_ID,
                     stations[state_get_to_station_index()].irail_id);
  dict_write_uint32(iter, MESSAGE_KEY_REQUEST_ID, state_get_last_data_request_id());

  app_message_outbox_send();

  // Update state machine
  state_set_load_state(LOAD_STATE_CONNECTING);
  state_set_data_loading(true);
  state_set_data_failed(false);
  state_set_num_departures(0);

  // Start timeout watchdog
  AppTimer *timer = state_get_timeout_timer();
  if (timer) {
    app_timer_cancel(timer);
  }
  state_set_timeout_timer(app_timer_register(LOADING_TIMEOUT_MS, loading_timeout_callback, NULL));

  menu_layer_reload_data(s_menu_layer);

  APP_LOG(APP_LOG_LEVEL_INFO, "Requesting data [ID %lu]: %s -> %s",
          (unsigned long)state_get_last_data_request_id(),
          stations[state_get_from_station_index()].name,
          stations[state_get_to_station_index()].name);
}

// Request detail data for selected departure
void api_handler_request_detail_data(void) {
  uint16_t index = state_get_selected_departure_index();
  TrainDeparture *departure = &state_get_departures()[index];

  APP_LOG(APP_LOG_LEVEL_INFO, "Selected train to %s", departure->destination);

  // Generate unique request ID for detail request
  state_increment_detail_request_id();

  // Request fresh details from JavaScript
  state_set_detail_received(false);

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, MESSAGE_KEY_MESSAGE_TYPE, MSG_REQUEST_DETAILS);
  dict_write_uint8(iter, MESSAGE_KEY_DEPARTURE_INDEX, index);
  dict_write_uint32(iter, MESSAGE_KEY_REQUEST_ID, state_get_last_detail_request_id());
  app_message_outbox_send();

  APP_LOG(APP_LOG_LEVEL_INFO, "Detail request [ID %lu] sent for departure %d",
          (unsigned long)state_get_last_detail_request_id(), index);

  // Show detail window
  detail_window_show();
}

// AppMessage callbacks
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Read message type
  Tuple *message_type_tuple = dict_find(iterator, MESSAGE_KEY_MESSAGE_TYPE);
  if (!message_type_tuple) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "No message type");
    return;
  }

  uint8_t message_type = message_type_tuple->value->uint8;

  if (message_type == MSG_REQUEST_ACK) {
    // JavaScript acknowledged the request and is fetching from API
    Tuple *request_id_tuple = dict_find(iterator, MESSAGE_KEY_REQUEST_ID);
    if (request_id_tuple) {
      uint32_t request_id = request_id_tuple->value->uint32;

      // Validate this acknowledgment is for our current request
      if (request_id == state_get_last_data_request_id()) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Request acknowledged [ID %lu], fetching from iRail...",
                (unsigned long)request_id);
        state_set_load_state(LOAD_STATE_FETCHING);
        menu_layer_reload_data(s_menu_layer);
      } else {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring stale acknowledgment [ID %lu] (expected %lu)",
                (unsigned long)request_id, (unsigned long)state_get_last_data_request_id());
      }
    }
  } else if (message_type == MSG_SEND_COUNT) {
    // Received departure count
    Tuple *request_id_tuple = dict_find(iterator, MESSAGE_KEY_REQUEST_ID);
    Tuple *count_tuple = dict_find(iterator, MESSAGE_KEY_DATA_COUNT);

    if (request_id_tuple && count_tuple) {
      uint32_t request_id = request_id_tuple->value->uint32;

      // Validate this response is for our current request
      if (request_id != state_get_last_data_request_id()) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring stale count [ID %lu] (expected %lu)",
                (unsigned long)request_id, (unsigned long)state_get_last_data_request_id());
        return;
      }

      state_set_num_departures(count_tuple->value->uint8);
      state_set_load_state(LOAD_STATE_RECEIVING);
      APP_LOG(APP_LOG_LEVEL_INFO, "Expecting %d departures [ID %lu]",
              state_get_num_departures(), (unsigned long)request_id);

      if (state_get_num_departures() == 0) {
        state_set_load_state(LOAD_STATE_COMPLETE);
        state_set_data_loading(false);
        AppTimer *timer = state_get_timeout_timer();
        if (timer) {
          app_timer_cancel(timer);
          state_set_timeout_timer(NULL);
        }
        menu_layer_reload_data(s_menu_layer);
      }
    }
  } else if (message_type == MSG_SEND_DEPARTURE) {
    // Received departure data
    Tuple *request_id_tuple = dict_find(iterator, MESSAGE_KEY_REQUEST_ID);
    Tuple *index_tuple = dict_find(iterator, MESSAGE_KEY_DEPARTURE_INDEX);
    if (!index_tuple) return;

    // Validate request ID if present
    if (request_id_tuple) {
      uint32_t request_id = request_id_tuple->value->uint32;
      if (request_id != state_get_last_data_request_id()) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring stale departure [ID %lu] (expected %lu)",
                (unsigned long)request_id, (unsigned long)state_get_last_data_request_id());
        return;
      }
    }

    uint8_t index = index_tuple->value->uint8;
    if (index >= MAX_DEPARTURES) return;

    TrainDeparture *dep = &state_get_departures()[index];

    // Read all fields
    Tuple *dest = dict_find(iterator, MESSAGE_KEY_DESTINATION);
    Tuple *depart = dict_find(iterator, MESSAGE_KEY_DEPART_TIME);
    Tuple *depart_ts = dict_find(iterator, MESSAGE_KEY_DEPART_TIMESTAMP);
    Tuple *arrive = dict_find(iterator, MESSAGE_KEY_ARRIVE_TIME);
    Tuple *platform = dict_find(iterator, MESSAGE_KEY_PLATFORM);
    Tuple *train_type = dict_find(iterator, MESSAGE_KEY_TRAIN_TYPE);
    Tuple *duration = dict_find(iterator, MESSAGE_KEY_DURATION);
    Tuple *depart_delay = dict_find(iterator, MESSAGE_KEY_DEPART_DELAY);
    Tuple *arrive_delay = dict_find(iterator, MESSAGE_KEY_ARRIVE_DELAY);
    Tuple *is_direct = dict_find(iterator, MESSAGE_KEY_IS_DIRECT);
    Tuple *platform_changed = dict_find(iterator, MESSAGE_KEY_PLATFORM_CHANGED);

    // Copy string data
    if (dest) strncpy(dep->destination, dest->value->cstring, sizeof(dep->destination) - 1);
    if (depart) strncpy(dep->depart_time, depart->value->cstring, sizeof(dep->depart_time) - 1);
    if (arrive) strncpy(dep->arrive_time, arrive->value->cstring, sizeof(dep->arrive_time) - 1);
    if (platform) strncpy(dep->platform, platform->value->cstring, sizeof(dep->platform) - 1);
    if (train_type) strncpy(dep->train_type, train_type->value->cstring, sizeof(dep->train_type) - 1);
    if (duration) strncpy(dep->duration, duration->value->cstring, sizeof(dep->duration) - 1);

    // Copy numeric data
    dep->depart_delay = depart_delay ? depart_delay->value->int8 : 0;
    dep->arrive_delay = arrive_delay ? arrive_delay->value->int8 : 0;
    dep->is_direct = is_direct ? (is_direct->value->uint8 != 0) : true;
    dep->platform_changed = platform_changed ? (platform_changed->value->uint8 != 0) : false;

    // Store timestamp for glance expiration (avoids parsing later)
    dep->depart_timestamp = depart_ts ? (time_t)depart_ts->value->int32 : 0;

    APP_LOG(APP_LOG_LEVEL_INFO, "Received departure %d: %s", index, dep->destination);

    // If this is the last departure, complete loading
    if (index == state_get_num_departures() - 1) {
      state_set_load_state(LOAD_STATE_COMPLETE);
      state_set_data_loading(false);

      // Cancel timeout timer
      AppTimer *timer = state_get_timeout_timer();
      if (timer) {
        app_timer_cancel(timer);
        state_set_timeout_timer(NULL);
      }

      APP_LOG(APP_LOG_LEVEL_INFO, "All departures received");

      // Update glances with fresh data
      glances_update();

      if (state_is_background_update()) {
        // Background update - don't show UI, just exit
        APP_LOG(APP_LOG_LEVEL_INFO, "Background glance update complete, exiting");
        state_set_background_update(false);
        // App will exit naturally when window stack is empty
      } else {
        // Normal update - refresh UI (only after all departures received)
        menu_layer_reload_data(s_menu_layer);
      }
    } else {
      // Intermediate departure - just mark dirty to redraw without resetting scroll
      if (!state_is_background_update()) {
        layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
      }
    }
  } else if (message_type == MSG_SEND_DETAIL) {
    // Received connection detail data (leg-by-leg)
    Tuple *request_id_tuple = dict_find(iterator, MESSAGE_KEY_REQUEST_ID);
    Tuple *leg_count_tuple = dict_find(iterator, MESSAGE_KEY_LEG_COUNT);
    Tuple *leg_index_tuple = dict_find(iterator, MESSAGE_KEY_LEG_INDEX);

    // Validate request ID if present
    if (request_id_tuple) {
      uint32_t request_id = request_id_tuple->value->uint32;
      if (request_id != state_get_last_detail_request_id()) {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring stale detail [ID %lu] (expected %lu)",
                (unsigned long)request_id, (unsigned long)state_get_last_detail_request_id());
        return;
      }
    }

    JourneyDetail *journey = state_get_journey_detail();

    if (leg_count_tuple) {
      // First message: leg count
      journey->leg_count = leg_count_tuple->value->uint8;
      APP_LOG(APP_LOG_LEVEL_INFO, "Expecting %d legs [ID %lu]", journey->leg_count,
              request_id_tuple ? (unsigned long)request_id_tuple->value->uint32 : 0);
    } else if (leg_index_tuple) {
      // Subsequent messages: individual leg data
      uint8_t leg_index = leg_index_tuple->value->uint8;
      if (leg_index < 4) {  // Max 4 legs
        JourneyLeg *leg = &journey->legs[leg_index];

        // Read all leg fields
        Tuple *depart_station = dict_find(iterator, MESSAGE_KEY_LEG_DEPART_STATION);
        Tuple *arrive_station = dict_find(iterator, MESSAGE_KEY_LEG_ARRIVE_STATION);
        Tuple *depart_time = dict_find(iterator, MESSAGE_KEY_LEG_DEPART_TIME);
        Tuple *arrive_time = dict_find(iterator, MESSAGE_KEY_LEG_ARRIVE_TIME);
        Tuple *depart_platform = dict_find(iterator, MESSAGE_KEY_LEG_DEPART_PLATFORM);
        Tuple *arrive_platform = dict_find(iterator, MESSAGE_KEY_LEG_ARRIVE_PLATFORM);
        Tuple *depart_delay = dict_find(iterator, MESSAGE_KEY_LEG_DEPART_DELAY);
        Tuple *arrive_delay = dict_find(iterator, MESSAGE_KEY_LEG_ARRIVE_DELAY);
        Tuple *vehicle = dict_find(iterator, MESSAGE_KEY_LEG_VEHICLE);
        Tuple *direction = dict_find(iterator, MESSAGE_KEY_LEG_DIRECTION);
        Tuple *stop_count = dict_find(iterator, MESSAGE_KEY_LEG_STOP_COUNT);
        Tuple *depart_platform_changed = dict_find(iterator, MESSAGE_KEY_LEG_DEPART_PLATFORM_CHANGED);
        Tuple *arrive_platform_changed = dict_find(iterator, MESSAGE_KEY_LEG_ARRIVE_PLATFORM_CHANGED);

        // Copy string data
        if (depart_station) strncpy(leg->depart_station, depart_station->value->cstring, sizeof(leg->depart_station) - 1);
        if (arrive_station) strncpy(leg->arrive_station, arrive_station->value->cstring, sizeof(leg->arrive_station) - 1);
        if (depart_time) strncpy(leg->depart_time, depart_time->value->cstring, sizeof(leg->depart_time) - 1);
        if (arrive_time) strncpy(leg->arrive_time, arrive_time->value->cstring, sizeof(leg->arrive_time) - 1);
        if (depart_platform) strncpy(leg->depart_platform, depart_platform->value->cstring, sizeof(leg->depart_platform) - 1);
        if (arrive_platform) strncpy(leg->arrive_platform, arrive_platform->value->cstring, sizeof(leg->arrive_platform) - 1);
        if (vehicle) strncpy(leg->vehicle, vehicle->value->cstring, sizeof(leg->vehicle) - 1);
        if (direction) strncpy(leg->direction, direction->value->cstring, sizeof(leg->direction) - 1);

        // Copy numeric data
        leg->depart_delay = depart_delay ? depart_delay->value->int8 : 0;
        leg->arrive_delay = arrive_delay ? arrive_delay->value->int8 : 0;
        leg->stop_count = stop_count ? stop_count->value->uint8 : 0;
        leg->depart_platform_changed = depart_platform_changed ? (depart_platform_changed->value->uint8 != 0) : false;
        leg->arrive_platform_changed = arrive_platform_changed ? (arrive_platform_changed->value->uint8 != 0) : false;

        APP_LOG(APP_LOG_LEVEL_INFO, "Received leg %d: %s -> %s", leg_index, leg->depart_station, leg->arrive_station);

        // If this is the last leg, mark as received and update UI
        if (leg_index == journey->leg_count - 1) {
          state_set_detail_received(true);
          APP_LOG(APP_LOG_LEVEL_INFO, "All legs received");

          // Update detail window if it's currently shown
          Window *detail_win = detail_window_get_instance();
          if (detail_win && window_stack_contains_window(detail_win)) {
            detail_window_update();
          }
        }
      }
    }
  } else if (message_type == MSG_SEND_STATION_COUNT) {
    // Received station count from JavaScript
    Tuple *count_tuple = dict_find(iterator, MESSAGE_KEY_CONFIG_STATION_COUNT);
    if (count_tuple) {
      uint8_t count = count_tuple->value->uint8;
      state_set_num_stations((count > MAX_FAVORITE_STATIONS) ? MAX_FAVORITE_STATIONS : count);
      state_set_stations_received(false);  // Reset flag
      APP_LOG(APP_LOG_LEVEL_INFO, "Expecting %d favorite stations", state_get_num_stations());
    }
  } else if (message_type == MSG_SEND_STATION) {
    // Received individual station data
    Tuple *index_tuple = dict_find(iterator, MESSAGE_KEY_CONFIG_STATION_INDEX);
    if (!index_tuple) return;

    uint8_t index = index_tuple->value->uint8;
    if (index >= MAX_FAVORITE_STATIONS) return;

    Station *station = &state_get_stations()[index];

    // Read station name
    Tuple *name_tuple = dict_find(iterator, MESSAGE_KEY_CONFIG_STATION_NAME);
    if (name_tuple) {
      strncpy(station->name, name_tuple->value->cstring, sizeof(station->name) - 1);
      station->name[sizeof(station->name) - 1] = '\0';
    }

    // Read iRail ID
    Tuple *id_tuple = dict_find(iterator, MESSAGE_KEY_CONFIG_STATION_IRAIL_ID);
    if (id_tuple) {
      strncpy(station->irail_id, id_tuple->value->cstring, sizeof(station->irail_id) - 1);
      station->irail_id[sizeof(station->irail_id) - 1] = '\0';
    }

    APP_LOG(APP_LOG_LEVEL_INFO, "Received station %d: %s (%s)", index, station->name, station->irail_id);

    // If this is the last station, mark as complete and update UI
    if (index == state_get_num_stations() - 1) {
      state_set_stations_received(true);
      APP_LOG(APP_LOG_LEVEL_INFO, "All stations received, requesting initial data");

      // Cancel config timeout timer since we got the config
      AppTimer *config_timer = state_get_config_timeout_timer();
      if (config_timer) {
        app_timer_cancel(config_timer);
        state_set_config_timeout_timer(NULL);
        APP_LOG(APP_LOG_LEVEL_INFO, "Config timeout timer cancelled");
      }

      menu_layer_reload_data(s_menu_layer);

      // Request initial train data now that we have stations
      api_handler_request_train_data();
    }
  } else if (message_type == MSG_SET_ACTIVE_ROUTE) {
    // Set active route based on smart schedule
    Tuple *from_index_tuple = dict_find(iterator, MESSAGE_KEY_CONFIG_FROM_INDEX);
    Tuple *to_index_tuple = dict_find(iterator, MESSAGE_KEY_CONFIG_TO_INDEX);

    if (from_index_tuple && to_index_tuple) {
      uint8_t from_idx = from_index_tuple->value->uint8;
      uint8_t to_idx = to_index_tuple->value->uint8;

      if (from_idx < state_get_num_stations() && to_idx < state_get_num_stations()) {
        state_set_from_station_index(from_idx);
        state_set_to_station_index(to_idx);
        APP_LOG(APP_LOG_LEVEL_INFO, "Active route set: %s -> %s",
                state_get_stations()[from_idx].name, state_get_stations()[to_idx].name);
        menu_layer_reload_data(s_menu_layer);
        api_handler_request_train_data();
      }
    }
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", (int)reason);

  // Set error state and update UI
  state_set_data_loading(false);
  state_set_data_failed(true);
  menu_layer_reload_data(s_menu_layer);
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

// Initialize API handler
void api_handler_init(MenuLayer *menu_layer) {
  s_menu_layer = menu_layer;

  // Register AppMessage callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Open AppMessage with appropriate buffer sizes
  app_message_open(512, 512);

  // Start config timeout timer (fallback to defaults if no config received)
  state_set_config_timeout_timer(app_timer_register(CONFIG_TIMEOUT_MS, config_timeout_callback, NULL));
  APP_LOG(APP_LOG_LEVEL_INFO, "Config timeout timer started (%d ms)", CONFIG_TIMEOUT_MS);
}

// Handle timeout
void api_handler_handle_timeout(void) {
  loading_timeout_callback(NULL);
}
