#pragma once

#include <pebble.h>

// Message type constants (must match JavaScript)
#define MSG_REQUEST_DATA 1
#define MSG_SEND_DEPARTURE 2
#define MSG_SEND_COUNT 3
#define MSG_REQUEST_DETAILS 4
#define MSG_SEND_DETAIL 5
#define MSG_SEND_STATION_COUNT 6
#define MSG_SEND_STATION 7
#define MSG_SET_ACTIVE_ROUTE 8
#define MSG_REQUEST_ACK 9

// Worker message type for glance updates
#define WORKER_REQUEST_GLANCE 100

// Maximum number of departures and stations
#define MAX_DEPARTURES 11
#define MAX_FAVORITE_STATIONS 6

// Loading timeout
#define LOADING_TIMEOUT_MS 10000  // 10 seconds

// Loading state machine for detailed user feedback
typedef enum {
  LOAD_STATE_IDLE,           // Not loading
  LOAD_STATE_CONNECTING,     // Waiting for JS to acknowledge request
  LOAD_STATE_FETCHING,       // JS is calling iRail API
  LOAD_STATE_RECEIVING,      // Receiving departure data
  LOAD_STATE_COMPLETE,       // All data received
  LOAD_STATE_ERROR          // Error occurred
} LoadState;

// Station data
typedef struct {
  char name[64];        // Display name (e.g., "Brussels-Central")
  char irail_id[32];    // iRail ID (e.g., "BE.NMBS.008813003")
} Station;

// Train schedule data structure
typedef struct {
  char destination[32];
  char depart_time[8];
  time_t depart_timestamp;  // Unix timestamp for glance expiration
  char arrive_time[8];
  char platform[4];
  char train_type[8];
  char duration[8];
  int8_t depart_delay;  // Minutes of departure delay (0 = on time)
  int8_t arrive_delay;  // Minutes of arrival delay (0 = on time)
  bool is_direct;  // true = direct train, false = requires connection
  bool platform_changed;  // true = platform changed from original
} TrainDeparture;

// Journey leg data structure
typedef struct {
  char depart_station[32];
  char arrive_station[32];
  char depart_time[8];
  char arrive_time[8];
  char depart_platform[4];
  char arrive_platform[4];
  int8_t depart_delay;
  int8_t arrive_delay;
  char vehicle[16];        // e.g., "IC 1234"
  char direction[32];
  uint8_t stop_count;
  bool depart_platform_changed;
  bool arrive_platform_changed;
} JourneyLeg;

// Journey detail (collection of legs)
typedef struct {
  JourneyLeg legs[4];      // Max 3 connections = 4 legs
  uint8_t leg_count;
} JourneyDetail;
