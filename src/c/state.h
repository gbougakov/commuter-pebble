#pragma once

#include "types.h"

// Default fallback stations
extern const Station DEFAULT_STATIONS[];
extern const uint8_t NUM_DEFAULT_STATIONS;

// Initialize state (start with no stations - wait for config from JS)
void state_init(void);

// Load default fallback stations (called if config fails)
void state_load_default_stations(void);

// Station management
Station* state_get_stations(void);
uint8_t state_get_num_stations(void);
void state_set_num_stations(uint8_t count);
bool state_are_stations_received(void);
void state_set_stations_received(bool received);

uint8_t state_get_from_station_index(void);
void state_set_from_station_index(uint8_t index);
uint8_t state_get_to_station_index(void);
void state_set_to_station_index(uint8_t index);

// Departure data management
TrainDeparture* state_get_departures(void);
uint8_t state_get_num_departures(void);
void state_set_num_departures(uint8_t count);

// Loading state
LoadState state_get_load_state(void);
void state_set_load_state(LoadState state);
bool state_is_data_loading(void);
void state_set_data_loading(bool loading);
bool state_is_data_failed(void);
void state_set_data_failed(bool failed);
bool state_is_background_update(void);
void state_set_background_update(bool is_background);

// Request ID tracking (for idempotency)
uint32_t state_get_last_data_request_id(void);
void state_increment_data_request_id(void);
uint32_t state_get_last_detail_request_id(void);
void state_increment_detail_request_id(void);

// Timeout timer management
AppTimer* state_get_timeout_timer(void);
void state_set_timeout_timer(AppTimer* timer);

// Config timeout timer
AppTimer* state_get_config_timeout_timer(void);
void state_set_config_timeout_timer(AppTimer* timer);

// Detail window state
uint16_t state_get_selected_departure_index(void);
void state_set_selected_departure_index(uint16_t index);
JourneyDetail* state_get_journey_detail(void);
bool state_is_detail_received(void);
void state_set_detail_received(bool received);

// Marquee animation state
AppTimer* state_get_marquee_timer(void);
void state_set_marquee_timer(AppTimer* timer);
int16_t state_get_marquee_offset(void);
void state_set_marquee_offset(int16_t offset);
uint16_t state_get_selected_row(void);
void state_set_selected_row(uint16_t row);
int16_t state_get_marquee_max_offset(void);
void state_set_marquee_max_offset(int16_t offset);
