#include "state.h"

// Default fallback stations (if no config received)
const Station DEFAULT_STATIONS[] = {
  {"Brussels-Central", "BE.NMBS.008813003"},
  {"Antwerp-Central", "BE.NMBS.008821006"},
  {"Ghent-Sint-Pieters", "BE.NMBS.008892007"},
  {"Liège-Guillemins", "BE.NMBS.008841004"},
  {"Leuven", "BE.NMBS.008833001"}
};
const uint8_t NUM_DEFAULT_STATIONS = sizeof(DEFAULT_STATIONS) / sizeof(Station);

// Station data
static Station s_stations[MAX_FAVORITE_STATIONS];
static uint8_t s_num_stations = 0;
static uint8_t s_from_station_index = 0;
static uint8_t s_to_station_index = 1;
static bool s_stations_received = false;

// Departure data
static TrainDeparture s_departures[MAX_DEPARTURES];
static uint8_t s_num_departures = 0;

// Loading state
static LoadState s_load_state = LOAD_STATE_IDLE;
static bool s_data_loading = false;
static bool s_data_failed = false;
static bool s_is_background_update = false;
static AppTimer *s_timeout_timer = NULL;

// Request ID tracking
static uint32_t s_last_data_request_id = 0;
static uint32_t s_last_detail_request_id = 0;

// Detail window state
static uint16_t s_selected_departure_index = 0;
static JourneyDetail s_journey_detail;
static bool s_detail_received = false;

// Marquee animation state
static AppTimer *s_marquee_timer = NULL;
static int16_t s_marquee_offset = 0;
static uint16_t s_selected_row = 0;
static int16_t s_marquee_max_offset = 0;

// Initialize state with default stations
void state_init(void) {
  s_num_stations = NUM_DEFAULT_STATIONS;
  for (uint8_t i = 0; i < NUM_DEFAULT_STATIONS && i < MAX_FAVORITE_STATIONS; i++) {
    strncpy(s_stations[i].name, DEFAULT_STATIONS[i].name, sizeof(s_stations[i].name) - 1);
    s_stations[i].name[sizeof(s_stations[i].name) - 1] = '\0';
    strncpy(s_stations[i].irail_id, DEFAULT_STATIONS[i].irail_id, sizeof(s_stations[i].irail_id) - 1);
    s_stations[i].irail_id[sizeof(s_stations[i].irail_id) - 1] = '\0';
  }
  s_from_station_index = 0;
  s_to_station_index = 1;
  s_stations_received = true;
}

// Station management
Station* state_get_stations(void) { return s_stations; }
uint8_t state_get_num_stations(void) { return s_num_stations; }
void state_set_num_stations(uint8_t count) { s_num_stations = count; }
bool state_are_stations_received(void) { return s_stations_received; }
void state_set_stations_received(bool received) { s_stations_received = received; }

uint8_t state_get_from_station_index(void) { return s_from_station_index; }
void state_set_from_station_index(uint8_t index) { s_from_station_index = index; }
uint8_t state_get_to_station_index(void) { return s_to_station_index; }
void state_set_to_station_index(uint8_t index) { s_to_station_index = index; }

// Departure data management
TrainDeparture* state_get_departures(void) { return s_departures; }
uint8_t state_get_num_departures(void) { return s_num_departures; }
void state_set_num_departures(uint8_t count) { s_num_departures = count; }

// Loading state
LoadState state_get_load_state(void) { return s_load_state; }
void state_set_load_state(LoadState state) { s_load_state = state; }
bool state_is_data_loading(void) { return s_data_loading; }
void state_set_data_loading(bool loading) { s_data_loading = loading; }
bool state_is_data_failed(void) { return s_data_failed; }
void state_set_data_failed(bool failed) { s_data_failed = failed; }
bool state_is_background_update(void) { return s_is_background_update; }
void state_set_background_update(bool is_background) { s_is_background_update = is_background; }

// Request ID tracking
uint32_t state_get_last_data_request_id(void) { return s_last_data_request_id; }
void state_increment_data_request_id(void) { s_last_data_request_id++; }
uint32_t state_get_last_detail_request_id(void) { return s_last_detail_request_id; }
void state_increment_detail_request_id(void) { s_last_detail_request_id++; }

// Timeout timer management
AppTimer* state_get_timeout_timer(void) { return s_timeout_timer; }
void state_set_timeout_timer(AppTimer* timer) { s_timeout_timer = timer; }

// Detail window state
uint16_t state_get_selected_departure_index(void) { return s_selected_departure_index; }
void state_set_selected_departure_index(uint16_t index) { s_selected_departure_index = index; }
JourneyDetail* state_get_journey_detail(void) { return &s_journey_detail; }
bool state_is_detail_received(void) { return s_detail_received; }
void state_set_detail_received(bool received) { s_detail_received = received; }

// Marquee animation state
AppTimer* state_get_marquee_timer(void) { return s_marquee_timer; }
void state_set_marquee_timer(AppTimer* timer) { s_marquee_timer = timer; }
int16_t state_get_marquee_offset(void) { return s_marquee_offset; }
void state_set_marquee_offset(int16_t offset) { s_marquee_offset = offset; }
uint16_t state_get_selected_row(void) { return s_selected_row; }
void state_set_selected_row(uint16_t row) { s_selected_row = row; }
int16_t state_get_marquee_max_offset(void) { return s_marquee_max_offset; }
void state_set_marquee_max_offset(int16_t offset) { s_marquee_max_offset = offset; }
