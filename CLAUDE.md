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
// Store connection identifiers for detail requests
var currentFromStation = '';
var currentToStation = '';
var connectionIdentifiers = []; // Array of {vehicle, departTime}

// When sending departure list:
connectionIdentifiers[index] = {
  vehicle: conn.departure.vehicle || '',  // e.g., "BE.NMBS.IC1832"
  departTime: departTime                   // Unix timestamp
};
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
  char destination[32];  // Fixed-size array
  char depart_time[8];
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

## VSCode IntelliSense
The `.vscode/c_cpp_properties.json` configures paths to Pebble SDK headers for autocomplete. SDK location: `~/Library/Application Support/Pebble SDK/SDKs/4.5/`

- Do not ever use basalt, test in aplite

- Always document everything relevant in CLAUDE.md