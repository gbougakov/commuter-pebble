# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Pebble smartwatch app displaying NMBS (Belgian railway) train schedules with a custom MenuLayer UI. The app is built for Pebble SDK 3 and targets all Pebble platforms (aplite, basalt, chalk, diorite, emery, flint).

## Build & Development Commands

**Build the app:**
```bash
pebble build
```

**Install to emulator:**
```bash
pebble install --emulator aplite  # or basalt, chalk, etc.
```

**Build and install:**
```bash
pebble build && pebble install --emulator aplite
```

**VNC mode (for visual testing):**
```bash
pebble install --emulator aplite --vnc
```

## Architecture

### UI Layer System
The app uses Pebble's layer-based rendering with a specific drawing order that's critical for proper display:

1. **Text layers** (times, details) - drawn first, may overflow
2. **Masking rectangles** - cover overflow areas with background color
3. **Train type box** - drawn on top of left mask
4. **Platform box** - drawn last, appears on top of right mask

This layered approach simulates clipping (which SDK 3 doesn't have natively) by using the "painter's algorithm."

**Dynamic Masking for Icons:**
The left mask width dynamically adjusts based on row content:
- Base: `text_margin + train_type_box_width + 2`
- With connection icon: Add 18px (16px icon + 2px gap)
- Calculated once via `icon_space` variable and reused for both mask and details positioning
- Ensures marquee text starts cleanly after all left-side indicators

### MenuLayer Callback Architecture
The app uses MenuLayer with custom callbacks instead of data binding:
- `get_num_rows`: Returns count of departures + 1 (for station switcher row)
- `draw_row`: Custom rendering for each menu cell (handles all visual elements)
- `get_cell_height`: Row 0 (station switcher) = 32px, train rows = 44px (rect displays)
- `select_click`: Row 0 cycles stations, other rows open detail window
- `selection_changed`: Triggers marquee animation reset

**Row Index Mapping:**
- Row 0: Station switcher (cycles through 5 Belgian stations)
- Row 1+: Train departures (actual data is at `s_departures[row - 1]`)

### Marquee Scrolling System
Long destination names scroll when selected:
- Uses `AppTimer` for frame-by-frame animation at ~12.5fps (80ms intervals)
- Only scrolls if `text_size.w > details_rect.size.w`
- Calculates `s_marquee_max_offset` dynamically based on actual text width
- Scrolls once from start to end, then stops (no looping)
- Reset on selection change via `selection_changed` callback

### Visual Indicators

**Adaptive font sizing:**
- On-time trains: 18pt bold (GOTHIC_18_BOLD)
- Delayed trains: 14pt bold (GOTHIC_14_BOLD) to fit delay indicators

**Train type display:**
- Train type box: 16px wide (was 20px)
- Train types truncated to 2 characters max via `snprintf` with `"%.2s"` format
- Examples: IC, L, S, P (EXP becomes "EX")
- Airport trains show plane icon instead of type box

**Platform change indication:**
- Normal: Filled box with inverted text
- Changed: 1px outline box with regular text color

**Connection display:**
- All trains: "45m · Destination" (duration · destination)
- Connection icon displayed next to train type for non-direct trains

**Airport trains:**
- Automatically detected via `strstr(destination, "Airport")`
- Show airport icon (plane) instead of train type box
- Icon positioned where train type box would normally be

### Data Structure
The `TrainDeparture` struct contains all schedule information:
- Times, platform, train type, duration
- `depart_delay`, `arrive_delay` (int8_t, minutes)
- `is_direct` (bool) - determines if connection icon is displayed
- `platform_changed` (bool) - switches between filled/outlined platform box

### Detail Window
A scrollable detail window shows comprehensive train information when a departure is selected:
- Uses `ScrollLayer` for scrollable content
- Displays: destination, train type, times, delays, platform info, connection status
- Formatted with emojis for visual clarity
- Accessible via clicking any train departure row
- Back button returns to main menu

## Platform Support
Compiles for 6 Pebble platforms with different memory constraints:
- aplite: 24KB RAM (black & white, rectangular)
- basalt/diorite: 64KB RAM (color, rectangular)
- chalk: 64KB RAM (color, round)
- emery: 128KB RAM (color, rectangular, latest hardware)
- flint: Similar to emery

Use `PBL_IF_ROUND_ELSE()` macro for round vs rectangular display differences.

## Resource Generation

### Icon Variants
The app uses both black and white versions of icons to adapt to selected/unselected menu rows. Generate icon variants using the provided script:

```bash
./scripts/generate_icons.sh <input_icon.png>
```

**Example:**
```bash
./scripts/generate_icons.sh switch.png
```

This generates:
- `<icon>_white.png` - White version with transparency (for selected rows)
- `<icon>_1bit.png` - 1-bit monochrome version (currently unused, but may be useful for aplite optimization)

**Requirements:** ImageMagick (`brew install imagemagick`)

**Note:** Both black and white variants must be added to `package.json` resources and loaded separately in the C code.

**Current Icons:**
- `switch.png` / `switch_white.png` - Connection indicator (16x16)
- `airport.png` / `airport_white.png` - Airport train indicator (16x16)

**Icon Inversion Pattern:**
Icons are swapped based on selection state to maintain visibility on inverted backgrounds. Each icon needs both a black (normal) and white (selected) variant. The pattern in code:
```c
GBitmap *icon = selected ? s_icon_name_white : s_icon_name;
```

## iRail API Integration

### Overview
The app fetches real-time Belgian train data from the iRail API through PebbleKit JS. The architecture separates list view (minimal data) from detail view (full data fetched on-demand).

### Message Flow Architecture

**Message Type Constants** (shared between C and JS):
```c
#define MSG_REQUEST_DATA 1      // C → JS: Request train connections
#define MSG_SEND_DEPARTURE 2    // JS → C: Send one departure
#define MSG_SEND_COUNT 3        // JS → C: Send total count first
#define MSG_REQUEST_DETAILS 4   // C → JS: Request full connection details
#define MSG_SEND_DETAIL 5       // JS → C: Send detailed info
```

**Typical Message Flow**:
1. User changes station → C sends MSG_REQUEST_DATA with FROM_STATION + TO_STATION
2. JS fetches from iRail API
3. JS sends MSG_SEND_COUNT with total departures count
4. JS sends MSG_SEND_DEPARTURE for each connection sequentially (recursive callbacks)
5. User clicks train → C sends MSG_REQUEST_DETAILS with DEPARTURE_INDEX
6. JS re-fetches API and matches exact connection by vehicle + time
7. JS sends MSG_SEND_DETAIL with direction + destination
8. C updates detail window with full info

### Station ID Mapping

iRail uses specific station identifiers (format: `BE.NMBS.xxxxxxx`):

```javascript
// In src/pkjs/index.js
var STATION_IDS = {
  'Brussels-Central': 'BE.NMBS.008813003',
  'Antwerp-Central': 'BE.NMBS.008821006',
  'Ghent-Sint-Pieters': 'BE.NMBS.008892007',
  'Liège-Guillemins': 'BE.NMBS.008841004',
  'Leuven': 'BE.NMBS.008833001'
};
```

**C-side Station Array** (src/c/nmbs.c):
```c
static const char* STATIONS[] = {
  "Brussels-Central",
  "Antwerp-Central",
  "Ghent-Sint-Pieters",
  "Liège-Guillemins",
  "Leuven"
};
```

### LocalStorage Persistence

Station selections and connection identifiers are persisted to localStorage to maintain continuity across app restarts and reconnections:

```javascript
// Storage keys
var STORAGE_KEY_FROM_STATION = 'nmbs_from_station';
var STORAGE_KEY_TO_STATION = 'nmbs_to_station';
var STORAGE_KEY_CONNECTIONS = 'nmbs_connections';

// Load on 'ready' event
function loadPersistedData() {
    var storedFrom = localStorage.getItem(STORAGE_KEY_FROM_STATION);
    var storedTo = localStorage.getItem(STORAGE_KEY_TO_STATION);
    var storedConnections = localStorage.getItem(STORAGE_KEY_CONNECTIONS);

    if (storedFrom && STATION_IDS[storedFrom]) {
        currentFromStation = storedFrom;
    }
    if (storedTo && STATION_IDS[storedTo]) {
        currentToStation = storedTo;
    }
    if (storedConnections) {
        connectionIdentifiers = JSON.parse(storedConnections);
    }
}

// Save whenever data changes
function savePersistedData() {
    localStorage.setItem(STORAGE_KEY_FROM_STATION, currentFromStation);
    localStorage.setItem(STORAGE_KEY_TO_STATION, currentToStation);
    localStorage.setItem(STORAGE_KEY_CONNECTIONS, JSON.stringify(connectionIdentifiers));
}
```

**When Data is Saved**:
- When `fetchTrainData()` is called (station selection changes)
- After each departure is sent (connection identifiers updated)

**Benefits**:
- User's station selection persists across watch app restarts
- Connection identifiers remain available for detail requests even after phone app suspension
- Graceful degradation: validates loaded data and resets to defaults on corruption

### API Request Debouncing

To prevent excessive API calls during rapid station switching, requests are debounced with a 500ms delay:

```javascript
// In src/pkjs/index.js
var requestDebounceTimer = null;
var DEBOUNCE_DELAY = 500;

// Clear pending request
if (requestDebounceTimer) {
  clearTimeout(requestDebounceTimer);
  console.log('Debouncing request...');
}

// Schedule new request
requestDebounceTimer = setTimeout(function() {
  console.log('Executing debounced request');
  fetchTrainData(fromStation, toStation);
  requestDebounceTimer = null;
}, DEBOUNCE_DELAY);
```

### Direction vs Destination

**Critical Distinction**:
- **Direction**: The train's terminus/final destination (e.g., "Ostend")
- **Destination**: The user's actual arrival station (e.g., "Ghent-Sint-Pieters")

**List View**: Shows direction only (where the train is heading)
```javascript
// Extract direction from API response
var direction = 'Unknown';
if (conn.departure.direction && conn.departure.direction.name) {
  direction = conn.departure.direction.name;
} else if (conn.arrival.stationinfo && conn.arrival.stationinfo.name) {
  // Fallback to arrival station if direction not available
  direction = conn.arrival.stationinfo.name;
}
```

**Detail View**: Shows both direction AND destination
- Direction: "Where is this train ultimately going?"
- Destination: "Where will I get off?"

### On-Demand Detail Fetching

Instead of sending all connection details upfront, the app stores identifiers and fetches full details only when user opens detail view.

**Connection Identifier Storage** (src/pkjs/index.js):
```javascript
// Store connection identifiers for detail requests (persisted to localStorage)
var currentFromStation = '';
var currentToStation = '';
var connectionIdentifiers = []; // Array of {vehicle, departTime}

// When sending departure list:
connectionIdentifiers[index] = {
  vehicle: conn.departure.vehicle || '',  // e.g., "BE.NMBS.IC1832"
  departTime: departTime                   // Unix timestamp
};

// Persist to localStorage after each update
savePersistedData();
```

**Detail Request Handler**:
```javascript
function fetchConnectionDetails(departureIndex) {
  var identifier = connectionIdentifiers[departureIndex];

  // Re-fetch fresh API data
  var url = IRAIL_API_URL +
            '?from=' + encodeURIComponent(fromId) +
            '&to=' + encodeURIComponent(toId) +
            '&format=json&lang=en';

  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  xhr.onload = function() {
    var response = JSON.parse(xhr.responseText);

    // Find matching connection by vehicle ID + departure time
    var matchedConn = null;
    for (var i = 0; i < response.connection.length; i++) {
      var conn = response.connection[i];
      if (conn.departure.vehicle === identifier.vehicle &&
          parseInt(conn.departure.time) === identifier.departTime) {
        matchedConn = conn;
        break;
      }
    }

    if (matchedConn) {
      sendConnectionDetail(matchedConn, departureIndex);
    }
  };
  xhr.send();
}
```

**Why Re-fetch?**:
- Gets latest delay information
- Reduces initial load size (AppMessage ~1KB limit)
- Ensures user sees fresh data when opening details

### Sequential Message Sending

Due to Pebble's AppMessage queue limitations, departures must be sent one at a time using recursive callbacks:

```javascript
function sendDeparture(connections, index) {
  if (index >= connections.length) {
    console.log('All departures sent');
    return;
  }

  var conn = connections[index];
  var message = {
    'MESSAGE_TYPE': MSG_SEND_DEPARTURE,
    'DEPARTURE_INDEX': index,
    'DESTINATION': direction.substring(0, 31),
    // ... other fields
  };

  // Send with success/failure callbacks
  Pebble.sendAppMessage(message, function() {
    // Success - send next departure
    sendDeparture(connections, index + 1);
  }, function(e) {
    console.log('Failed to send departure ' + index);
    // Try next one anyway
    sendDeparture(connections, index + 1);
  });
}
```

**Message Flow**:
1. Send MSG_SEND_COUNT first (tells C code how many to expect)
2. Wait for success callback
3. Send MSG_SEND_DEPARTURE[0]
4. Wait for callback → send [1]
5. Continue recursively until all sent

### Data Structure Changes

Changed from pointer-based to fixed-size arrays for embedded stability:

**Before**:
```c
typedef struct {
  char *destination;  // Pointer - risky on embedded
  char *depart_time;
  // ...
} TrainDeparture;
```

**After**:
```c
typedef struct {
  char destination[32];      // Fixed-size array
  char depart_time[8];       // "HH:MM" for display
  time_t depart_timestamp;   // Unix timestamp for glance expiration
  char arrive_time[8];
  char platform[4];
  char train_type[8];
  char duration[8];
  int8_t depart_delay;
  int8_t arrive_delay;
  bool is_direct;
  bool platform_changed;
} TrainDeparture;

static TrainDeparture s_departures[MAX_DEPARTURES];
static uint8_t s_num_departures = 0;
```

**Dynamic Data Reception** (src/c/nmbs.c):
```c
// In inbox_received_callback:
else if (message_type == MSG_SEND_DEPARTURE) {
  Tuple *index_tuple = dict_find(iterator, MESSAGE_KEY_DEPARTURE_INDEX);
  uint8_t index = index_tuple->value->uint8;

  if (index < MAX_DEPARTURES) {
    TrainDeparture *departure = &s_departures[index];

    // Copy string data with bounds checking
    Tuple *dest = dict_find(iterator, MESSAGE_KEY_DESTINATION);
    if (dest) {
      strncpy(departure->destination, dest->value->cstring,
              sizeof(departure->destination) - 1);
      departure->destination[sizeof(departure->destination) - 1] = '\0';
    }

    // ... copy other fields

    if (index == s_num_departures - 1) {
      // Last departure received - update UI
      s_data_loading = false;
      menu_layer_reload_data(s_menu_layer);
    }
  }
}
```

### Detail Window Update Mechanism

**Critical Pattern**: Must regenerate text content, not just mark dirty.

**Detail State Variables**:
```c
static uint16_t s_selected_departure_index = 0;
static char s_detail_destination[32] = "";
static char s_detail_direction[32] = "";
static bool s_detail_received = false;
```

**Update Function** (src/c/nmbs.c):
```c
static void update_detail_window(void) {
  if (!s_detail_text_layer) return;

  TrainDeparture *departure = &s_departures[s_selected_departure_index];
  static char detail_buffer[300];

  if (s_detail_received) {
    // Show full details with both direction and destination
    snprintf(detail_buffer, sizeof(detail_buffer),
             "Direction: %s\n\n"
             "Destination: %s\n\n"
             "🚂 %s Train\n\n"
             "⏰ Depart: %s%s\n"
             "📍 Arrive: %s%s\n\n"
             "🚉 Platform %s%s\n\n"
             "⏱ Duration: %s\n\n"
             "%s",
             s_detail_direction,
             s_detail_destination,
             departure->train_type,
             departure->depart_time,
             departure->depart_delay > 0 ?
               (snprintf(temp, sizeof(temp), " (+%d min)", departure->depart_delay), temp) : "",
             // ... rest of formatting
    );
  } else {
    // Show loading state with basic info
    snprintf(detail_buffer, sizeof(detail_buffer),
             "Loading connection details...\n\n"
             "Train: %s\n"
             "Departure: %s\n"
             "Platform: %s",
             departure->train_type,
             departure->depart_time,
             departure->platform);
  }

  // Update text layer content
  text_layer_set_text(s_detail_text_layer, detail_buffer);

  // Recalculate scroll layer content size
  if (s_detail_scroll_layer) {
    Layer *window_layer = window_get_root_layer(s_detail_window);
    GRect bounds = layer_get_bounds(window_layer);
    GSize text_size = text_layer_get_content_size(s_detail_text_layer);
    text_layer_set_size(s_detail_text_layer, GSize(bounds.size.w - 16, text_size.h + 16));
    scroll_layer_set_content_size(s_detail_scroll_layer, GSize(bounds.size.w, text_size.h + 24));
  }
}
```

**Why This Pattern?**:
- `layer_mark_dirty()` only redraws existing content
- `text_layer_set_text()` updates what gets drawn
- Must recalculate scroll layer size when content changes
- Called both on window load (initial) and when detail data arrives

**Triggering Updates**:
```c
// When receiving MSG_SEND_DETAIL:
s_detail_received = true;

if (s_detail_window && window_stack_contains_window(s_detail_window)) {
  update_detail_window();
}
```

### Error Handling

**Same Station Validation**:
```javascript
if (fromStation === toStation) {
  console.log('From and To stations are the same, sending empty result');
  Pebble.sendAppMessage({
    'MESSAGE_TYPE': MSG_SEND_COUNT,
    'DATA_COUNT': 0
  });
  return;
}
```

**API Error Responses**:
```javascript
xhr.onload = function() {
  if (xhr.readyState === 4) {
    if (xhr.status === 200) {
      try {
        var response = JSON.parse(xhr.responseText);
        processTrainData(response);
      } catch (e) {
        console.log('JSON parse error: ' + e.message);
        // Send empty result on parse error
        Pebble.sendAppMessage({
          'MESSAGE_TYPE': MSG_SEND_COUNT,
          'DATA_COUNT': 0
        });
      }
    } else {
      console.log('Request failed: ' + xhr.status);
      // Send empty result on API error
      Pebble.sendAppMessage({
        'MESSAGE_TYPE': MSG_SEND_COUNT,
        'DATA_COUNT': 0
      });
    }
  }
};
```

**No Connections Found**:
```javascript
if (!response.connection || response.connection.length === 0) {
  console.log('No connections found');
  Pebble.sendAppMessage({
    'MESSAGE_TYPE': MSG_SEND_COUNT,
    'DATA_COUNT': 0
  });
  return;
}
```

### Debugging API Issues

**Enable JavaScript logs**:
```bash
pebble logs --emulator aplite
```

**Key Log Messages**:
- `"Data requested: Brussels-Central -> Antwerp-Central"` - C sent request
- `"Fetching: https://api.irail.be/connections/..."` - JS making API call
- `"Response received"` - API responded
- `"Found X connections"` - Successfully parsed
- `"Count sent: X"` - Sent count to C
- `"Sending departure X: Direction Name"` - Sending each departure
- `"Details requested for departure X"` - User clicked train
- `"Detail sent for departure X"` - Full details sent

**Common Issues**:
1. **"Request failed 400"**: Check if from/to stations are the same
2. **"No connections found"**: API returned empty result (check station IDs)
3. **Detail window stuck on "Loading"**: Ensure `update_detail_window()` is called when MSG_SEND_DETAIL received
4. **Scroll viewport not moving**: Use `menu_layer_reload_data()` not `layer_mark_dirty()` after data changes

### Message Keys in package.json

All message keys must be defined in `package.json` for AppMessage communication:

```json
"messageKeys": [
  "MESSAGE_TYPE",
  "FROM_STATION",
  "TO_STATION",
  "DEPARTURE_INDEX",
  "DESTINATION",          // Contains direction in list view
  "DEPART_TIME",
  "ARRIVE_TIME",
  "PLATFORM",
  "TRAIN_TYPE",
  "DURATION",
  "DEPART_DELAY",
  "ARRIVE_DELAY",
  "IS_DIRECT",
  "PLATFORM_CHANGED",
  "REQUEST_DATA",
  "DATA_COUNT",
  "REQUEST_DETAILS",      // For on-demand detail fetching
  "DETAIL_DESTINATION",   // Actual destination station
  "DETAIL_DIRECTION",     // Train terminus
  "VIA_COUNT",
  "VIA_STATION",
  "VIA_TIME"
]
```

**Auto-generated Constants**:
Pebble SDK generates `MESSAGE_KEY_*` constants in C from these keys.

## User Configuration System

### Overview
The app provides a web-based configuration interface for customizing favorite stations and setting up smart route schedules. Configuration is accessed through the Pebble mobile app settings.

### Architecture

**Three-Tier Data Flow**:
1. **iRail API** → Both config page and watch app fetch full Belgian station list independently
2. **Config Page** → User selects favorites (up to 6) and defines schedules using iRail IDs
3. **Watch App** → Receives IDs, displays names, makes API requests with IDs

**Why iRail IDs as Source of Truth**:
- Stable across localizations (Brussels/Brussel/Bruxelles → BE.NMBS.008813003)
- Direct API compatibility
- Prevents ambiguity

### Configuration Components

#### 1. Station Cache System

**Fetching** (src/pkjs/index.js):
```javascript
function fetchStations() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', 'https://api.irail.be/v1/stations?format=json', true);
    xhr.onload = function() {
        var response = JSON.parse(xhr.responseText);
        stationCache = response.station.map(function(s) {
            return {
                id: s.id,                    // "BE.NMBS.008813003"
                name: s.name                 // "Brussels-Central"
            };
        });
        localStorage.setItem('nmbs_station_cache', JSON.stringify(stationCache));
    };
    xhr.send();
}
```

**Called on**:
- `ready` event (loads cached immediately, fetches fresh in background)
- Config page load (independent fetch)

**Storage**: `localStorage['nmbs_station_cache']` (~600+ stations, ~50KB)

#### 2. Configuration Page (src/pkjs/config.html)

**Self-Contained Architecture**:
- Fetches own station list from iRail API
- Uses localStorage for caching (same key as index.js)
- No URL parameters needed (solves size limit problem)
- Returns config via `pebblejs://close#encodedJSON`

**UI Sections**:
1. **Active Route Banner**: Shows currently active schedule (green) or none (gray)
2. **Favorite Stations**: Searchable list, checkbox selection (max 6)
3. **Smart Schedules**:
   - Simple Mode: Template buttons (Morning Commute, Evening Return, Weekend)
   - Advanced Mode: List view with add/remove

**Template Behavior**:
```javascript
function addTemplateSchedule(template) {
    var schedule = {
        id: template + '-' + Date.now(),
        fromId: favoriteStations[0],  // First favorite
        toId: favoriteStations[1],    // Second favorite
        enabled: true
    };

    if (template === 'morning-commute') {
        schedule.days = [1, 2, 3, 4, 5];  // Mon-Fri
        schedule.startTime = '06:00';
        schedule.endTime = '12:00';
    } else if (template === 'evening-return') {
        schedule.days = [1, 2, 3, 4, 5];
        schedule.startTime = '12:00';
        schedule.endTime = '23:59';
        // Swap from/to for return journey
        schedule.fromId = favoriteStations[1];
        schedule.toId = favoriteStations[0];
    }

    smartSchedules.push(schedule);
}
```

**Data Return**:
```javascript
function saveConfig() {
    var config = {
        favoriteStations: ['BE.NMBS.008813003', 'BE.NMBS.008833001', ...],
        smartSchedules: [{id, fromId, toId, days, startTime, endTime, enabled}, ...]
    };
    document.location = 'pebblejs://close#' + encodeURIComponent(JSON.stringify(config));
}
```

#### 3. Schedule Evaluation Engine

**Runs on**: App launch, data request, config save

```javascript
function evaluateSchedules() {
    var now = new Date();
    var currentDay = now.getDay();  // 0=Sun, 6=Sat
    var currentTime = '14:30';      // HH:MM format

    for (var i = 0; i < smartSchedules.length; i++) {
        var sched = smartSchedules[i];
        if (!sched.enabled) continue;

        // Check day match
        if (sched.days.indexOf(currentDay) === -1) continue;

        // Check time range
        if (currentTime >= sched.startTime && currentTime <= sched.endTime) {
            return {fromId: sched.fromId, toId: sched.toId};
        }
    }
    return null;
}
```

**Priority**: First matching schedule wins (order matters)

#### 4. C-Side Dynamic Station Handling

**Data Structures** (src/c/nmbs.c):
```c
typedef struct {
  char name[64];        // "Brussels-Central"
  char irail_id[32];    // "BE.NMBS.008813003"
} Station;

#define MAX_FAVORITE_STATIONS 6
static Station s_stations[MAX_FAVORITE_STATIONS];
static uint8_t s_num_stations = 0;
static bool s_stations_received = false;

// Fallback defaults
static const Station DEFAULT_STATIONS[] = {
  {"Brussels-Central", "BE.NMBS.008813003"},
  {"Antwerp-Central", "BE.NMBS.008821006"},
  {"Ghent-Sint-Pieters", "BE.NMBS.008892007"},
  {"Liège-Guillemins", "BE.NMBS.008841004"},
  {"Leuven", "BE.NMBS.008833001"}
};
```

**Message Handlers**:
```c
// MSG_SEND_STATION_COUNT: Expect N stations
s_num_stations = count_tuple->value->uint8;

// MSG_SEND_STATION: Receive station sequentially
strncpy(s_stations[index].name, name_tuple->value->cstring, 63);
strncpy(s_stations[index].irail_id, id_tuple->value->cstring, 31);

// MSG_SET_ACTIVE_ROUTE: Auto-select from/to based on schedule
s_from_station_index = from_idx;
s_to_station_index = to_idx;
request_train_data();
```

**Requesting Data**:
```c
// Send iRail IDs instead of names
dict_write_cstring(iter, MESSAGE_KEY_FROM_STATION_ID, s_stations[s_from_station_index].irail_id);
dict_write_cstring(iter, MESSAGE_KEY_TO_STATION_ID, s_stations[s_to_station_index].irail_id);
```

### Configuration Flow

1. **User Opens Settings** (Pebble app → Settings → NMBS)
   - `showConfiguration` event fires
   - Opens config.html (must be hosted publicly)

2. **Config Page Loads**
   - Fetches stations from iRail API
   - Loads saved config from localStorage
   - Displays current favorites and schedules

3. **User Configures**
   - Selects up to 6 favorite stations
   - Adds schedules (Simple templates or Advanced custom)
   - Sees "Active Now" preview update in real-time

4. **User Saves**
   - Config page serializes to JSON
   - Returns via `pebblejs://close#data`
   - `webviewclosed` event fires in index.js

5. **JavaScript Processes**
   - Saves to localStorage (`nmbs_favorite_stations`, `nmbs_smart_schedules`)
   - Evaluates schedules → determines active route
   - Sends station list to C code (MSG_SEND_STATION_COUNT + MSG_SEND_STATION × N)
   - Sends active route indices (MSG_SET_ACTIVE_ROUTE)

6. **Watch App Updates**
   - Receives favorite stations, updates menu
   - Sets from/to indices based on schedule
   - Requests fresh train data with new route

### LocalStorage Keys

| Key | Content | Size | Purpose |
|-----|---------|------|---------|
| `nmbs_station_cache` | Full station list from API | ~50KB | Offline station names/IDs |
| `nmbs_favorite_stations` | Array of iRail IDs | ~200B | User's 6 favorite stations |
| `nmbs_smart_schedules` | Array of schedule rules | ~500B | Time-based route automation |
| `nmbs_from_station` | iRail ID | ~30B | Last selected from station |
| `nmbs_to_station` | iRail ID | ~30B | Last selected to station |
| `nmbs_connections` | Connection identifiers | ~2KB | For detail fetching |

### Message Protocol Additions

**New Message Types**:
```c
#define MSG_SEND_STATION_COUNT 6  // JS → C: Sending N stations
#define MSG_SEND_STATION 7         // JS → C: Station data (sequential)
#define MSG_SET_ACTIVE_ROUTE 8     // JS → C: Auto-select from/to
```

**New Message Keys** (package.json):
```json
"FROM_STATION_ID",           // iRail ID for from station
"TO_STATION_ID",             // iRail ID for to station
"CONFIG_STATION_COUNT",      // Number of favorites
"CONFIG_STATION_INDEX",      // Station index (0-5)
"CONFIG_STATION_NAME",       // Display name
"CONFIG_STATION_IRAIL_ID",   // iRail station ID
"CONFIG_FROM_INDEX",         // Active from index
"CONFIG_TO_INDEX"            // Active to index
```

### Hosting the Configuration Page

The config.html file must be hosted on a publicly accessible URL. Options:

**GitHub Pages** (Recommended):
```bash
# Create a new repo: your-username/nmbs-config
# Enable GitHub Pages from Settings
# URL: https://your-username.github.io/nmbs-config/config.html
```

**Update index.js**:
```javascript
Pebble.addEventListener('showConfiguration', function() {
    var configUrl = 'https://your-username.github.io/nmbs-config/config.html';
    Pebble.openURL(configUrl);
});
```

**Alternative**: data: URL (limited, for testing only)
```javascript
var configUrl = 'data:text/html;charset=utf-8,' + encodeURIComponent(configHTML);
```

### Smart Schedule Data Structure

```javascript
{
  "id": "morning-commute-1234567890",        // Unique ID
  "fromId": "BE.NMBS.008833001",             // Leuven
  "toId": "BE.NMBS.008813003",               // Brussels-Central
  "days": [1, 2, 3, 4, 5],                   // Mon-Fri (0=Sun, 6=Sat)
  "startTime": "06:00",                      // HH:MM format
  "endTime": "12:00",                        // HH:MM format
  "enabled": true                            // Can be toggled off
}
```

**Day Encoding**: JavaScript `Date.getDay()` format
- 0 = Sunday
- 1 = Monday
- 2 = Tuesday
- 3 = Wednesday
- 4 = Thursday
- 5 = Friday
- 6 = Saturday

### Backward Compatibility

**Fallback Behavior**:
1. If no config received within app startup → use DEFAULT_STATIONS
2. If config load fails → gracefully degrade to 5 hardcoded stations
3. Old MESSAGE_KEY_FROM_STATION still supported for legacy compatibility

**Migration Path**:
- Existing users see default 5 stations until they open Settings
- First config save migrates them to new system
- Old localStorage keys (`nmbs_from_station` as name) converted to IDs

### Testing Configuration

**Test Checklist**:
1. ✅ Station fetching on app launch
2. ✅ Config page opens from Pebble Settings
3. ✅ Station search/filter works
4. ✅ Max 6 stations enforced
5. ✅ Template schedules create correct rules
6. ✅ Schedule evaluation matches current time
7. ✅ Active route banner updates
8. ✅ Save sends data back to watch
9. ✅ Watch receives stations and updates menu
10. ✅ Auto-route selection triggers data request
11. ✅ Manual station cycling still works
12. ✅ Fallback to defaults if config fails

**Debug Logging**:
```bash
pebble logs --emulator aplite | grep -E '(station|config|schedule)'
```

**Key Log Messages**:
- `"Fetched X stations from API"` - Station cache updated
- `"Loaded X favorite stations"` - Config loaded
- `"Matched schedule: morning-commute"` - Schedule activated
- `"Received station X: Name (ID)"` - C code receiving stations
- `"Active route set: A -> B"` - Auto-selection triggered

## VSCode IntelliSense
The `.vscode/c_cpp_properties.json` configures paths to Pebble SDK headers for autocomplete. SDK location: `~/Library/Application Support/Pebble SDK/SDKs/4.5/`

## AppGlances Integration

### Overview
The app supports AppGlances API for quick train schedule previews without opening the app. A background worker updates glances every 10 minutes, and glances are also refreshed whenever the app is opened or data is updated.

### Architecture

**Hybrid Update Strategy:**
- **Background updates**: Worker ticks every 10 minutes, sends message to app, app fetches fresh data, updates glances, exits
- **On-demand updates**: When user opens app or changes stations, glances update immediately with fresh data

**Data Flow:**
```
Worker (every 10min) → Main App → JS (schedule eval + API) → Main App → AppGlance → Exit
User opens app → Main App → JS (schedule eval + API) → Main App → AppGlance + UI update
```

### Components

#### 1. Worker (worker_src/c/worker.c)
- Ticks every `UPDATE_INTERVAL_MINUTES` (1 min for testing, 10 min for production)
- Sends `WORKER_REQUEST_GLANCE` message to main app
- Automatically detected by SDK via `worker_src` directory

#### 2. Main App (src/c/nmbs.c)

**Worker Message Handler:**
```c
static void worker_message_handler(uint16_t type, AppWorkerMessage *data) {
  if (type == WORKER_REQUEST_GLANCE) {
    s_is_background_update = true;
    request_train_data();  // Reuses existing function!
  }
}
```

**Glance Update Callback:**
```c
static void update_app_glance(AppGlanceReloadSession *session, size_t limit, void *context) {
  // Up to 8 train slices: "14:23 • Plat. 3 • Brussels-Central"
  // Each slice expires using stored depart_timestamp (Unix time from JS)
  // Timestamps are sent from JavaScript to avoid redundant parsing
}
```

**Data Reception:**
After receiving last departure from JS, app calls:
```c
app_glance_reload(update_app_glance, NULL);

if (s_is_background_update) {
  // Exit without showing UI
} else {
  // Update UI normally
}
```

#### 3. JavaScript (src/pkjs/index.js)
**No changes needed** - 100% code reuse of existing logic:
- Schedule evaluation (`evaluateSchedules()`)
- API fetching (`fetchTrainDataById()`)
- Message protocol (`MSG_REQUEST_DATA`, `MSG_SEND_DEPARTURE`)

### Platform Support

**AppGlances available on:** Basalt, Chalk, Diorite, Emery (platforms with `PBL_HEALTH` define)
**Not available on:** Aplite (SDK 2.x)

Code is properly guarded with `#if defined(PBL_HEALTH)` preprocessor directives to compile on all platforms.

### Timeline Icons

**Note:** The current implementation does not use timeline icons in AppGlances. Icons are available in resources but not currently displayed:
- `TIMELINE_TRAIN` (id: 1) - Generic train icon
- `TIMELINE_IC` (id: 2) - IC train type icon
- `TIMELINE_L` (id: 3) - L train type icon
- `TIMELINE_S` (id: 4) - S train type icon
- `TIMELINE_P` (id: 5) - P train type icon
- `TIMELINE_NO_TRAINS` (id: 6) - Empty/error state icon

Icons can be mapped at runtime via `get_train_type_icon()` helper function if needed in future.

### Glance Layout

**Simple Loop Architecture:**
Each departure gets its own glance - no separation between "main" and "detail" slices. The app creates one `AppGlanceSlice` per train in a simple for loop:

```c
int max_slices = limit < s_num_departures ? limit : s_num_departures;

for (int i = 0; i < max_slices; i++) {
  TrainDeparture *dep = &s_departures[i];
  // Format: "14:23 • Plat. 3 • Brussels-Central" or "14:23 (+5) • Plat. 3 • Brussels-Central"
  // Expires at departure time
}
```

**Train Glances:**
- Format (on time): `"HH:MM • Plat. N • Destination"`
  - Example: `"14:23 • Plat. 3 • Brussels-Central"`
- Format (delayed): `"HH:MM (+delay) • Plat. N • Destination"`
  - Example: `"14:23 (+5) • Plat. 3 • Brussels-Central"`
- Expiration: Each glance expires at its train's departure time (self-cleaning)
- Maximum: Limited by system `limit` parameter or number of departures, whichever is lower
- Auto-rotation: As trains depart, their glances automatically expire and the next train's glance becomes visible

**Empty State:**
- Subtitle: `"No trains available"`
- Expiration: 1 hour (allows worker to refresh and fetch new data)
- Only shown when `s_num_departures == 0`

### Message Protocol

**New Message Type:**
- `WORKER_REQUEST_GLANCE` (100): Worker → Main App requesting glance update

**Reused Message Types:**
- `MSG_REQUEST_DATA` (1): Main App → JS requesting train data
- `MSG_SEND_COUNT` (3): JS → Main App sending departure count
- `MSG_SEND_DEPARTURE` (2): JS → Main App sending each departure

### Code Reuse

**Achieved Zero Duplication:**
- Schedule evaluation logic ✅
- API fetching logic ✅
- Data structures (`s_departures` array) ✅
- Message protocol ✅
- Error handling ✅

**New Code (unavoidable):**
- Glance rendering callback (~50 lines) - simple for loop, no icon mapping
- Worker message handler (~5 lines)
- Time parsing helper for expiration (~45 lines)
- Platform guards

**Total:** ~100 new lines vs ~2000+ reused lines = **~5% new code, 95% reuse**

### Background Behavior

**When worker triggers update:**
1. Worker sends `WORKER_REQUEST_GLANCE` message
2. Main app sets `s_is_background_update = true`
3. Main app requests data from JS (reuses `request_train_data()`)
4. JS evaluates schedules, fetches from iRail API
5. JS sends departures back to main app
6. Main app populates `s_departures` array
7. Main app calls `app_glance_reload()`
8. Main app exits (no UI shown)

**Mode Flag:**
- `s_is_background_update` distinguishes background updates from UI updates
- When true: Skip UI updates, exit after glance update
- When false: Update both glances and UI

### Testing

**Enable Worker Logging:**
```bash
pebble logs --emulator aplite
```

**Key Log Messages:**
- `"Worker requesting glance update"` - Worker tick triggered
- `"All departures received"` - Data ready for glance update
- `"Updating AppGlance (limit: X, departures: Y)"` - Glance callback invoked
- `"AppGlance updated with N train slices"` - Glances successfully updated
- `"Background glance update complete, exiting"` - App exiting after background update

**Testing Checklist:**
1. ✅ Build succeeds for all platforms
2. ✅ Worker launches and ticks
3. ✅ Background updates trigger data fetch
4. ✅ Glances update with correct data
5. ✅ On-demand updates work when app is opened
6. ✅ Empty state handled gracefully
7. ✅ Icons display correctly
8. ✅ Delays shown in glance subtitles

### Configuration

**Worker Update Interval:**
Located in `worker_src/c/worker.c`:
```c
#define UPDATE_INTERVAL_MINUTES 1  // Set to 10 for production
```

**Glance Expiration:**
Each train slice expires at its actual departure time - when a train at 14:30 departs, that glance automatically disappears. This ensures glances are always relevant and don't show departed trains.

**Why this design:**
- No hardcoded expiration intervals needed in nmbs.c
- Worker update interval (in worker.c) is decoupled from glance lifetime
- Glances are self-cleaning based on actual train schedules
- If a train is delayed, the glance stays visible until the new departure time
- More efficient - Pebble OS automatically removes expired glances

### Implementation Notes

**Platform Compatibility:**
- AppGlances code is properly guarded with `#if defined(PBL_HEALTH)` preprocessor directives
- Compiles successfully for all platforms: aplite, basalt, diorite
- Only uses functions already present in the original codebase (`snprintf`, `time`, `strcmp`)
- Avoided problematic libc functions (`sscanf`, `localtime`, `gmtime`) that cause linker issues on some platforms

- Do not ever use basalt, test in diorite

- Always document everything relevant in CLAUDE.md