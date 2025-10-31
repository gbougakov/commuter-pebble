# CLAUDE.md

This file provides guidance to Claude Code when working with this repository.

## Project Overview

A Pebble smartwatch app displaying real-time NMBS (Belgian railway) train schedules with a custom MenuLayer UI. Built for Pebble SDK 3, targets all Pebble platforms (aplite, basalt, diorite, chalk, emery, flint).

## Build & Development

**Always use diorite emulator (never basalt) for testing.**

```bash
# Build
pebble build

# Install to emulator
pebble install --emulator diorite

# Build and install
pebble build && pebble install --emulator diorite

# VNC mode (visual testing)
pebble install --emulator diorite --vnc

# Screenshot
pebble screenshot --no-open screenshot.png

# View logs
pebble logs --emulator diorite
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

| Platform | RAM | Display | Notes |
|----------|-----|---------|-------|
| aplite | 24KB | B&W, rect | SDK 2.x |
| basalt/diorite | 64KB | Color, rect | **Use diorite for testing** |
| chalk | 64KB | Color, round | Use `PBL_IF_ROUND_ELSE()` |
| emery/flint | 128KB | Color, rect | Latest hardware |

**AppGlances**: Available on basalt, chalk, diorite, emery (platforms with `PBL_HEALTH` define). Not on aplite.

## Icon System

Icons adapt to menu row selection state. Each icon needs black (normal) and white (selected) variants.

**Generate variants:**
```bash
./scripts/generate_icons.sh <input_icon.png>  # Requires ImageMagick
```

**Current icons:**
- `switch.png` / `switch_white.png` - Connection indicator (16x16)
- `airport.png` / `airport_white.png` - Airport train indicator (16x16)

**Usage pattern:**
```c
GBitmap *icon = selected ? s_icon_name_white : s_icon_name;
```

Both variants must be added to `package.json` resources and loaded separately in C.

## iRail API Integration

Fetches real-time Belgian train data via PebbleKit JS. List view shows minimal data; detail view fetches full data on-demand.

### Message Protocol

**Message types (C ↔ JS):**
```c
#define MSG_REQUEST_DATA 1       // C → JS: Request connections
#define MSG_SEND_DEPARTURE 2     // JS → C: Send one departure
#define MSG_SEND_COUNT 3         // JS → C: Send total count first
#define MSG_REQUEST_DETAILS 4    // C → JS: Request full details
#define MSG_SEND_DETAIL 5        // JS → C: Send detailed info
#define MSG_SEND_STATION_COUNT 6 // JS → C: Config - station count
#define MSG_SEND_STATION 7       // JS → C: Config - station data
#define MSG_SET_ACTIVE_ROUTE 8   // JS → C: Config - auto-select route
#define WORKER_REQUEST_GLANCE 100 // Worker → C: Glance update
```

**Typical flow:**
1. C → JS: Request data (station IDs)
2. JS: Fetch from iRail API
3. JS → C: Send count, then departures sequentially (recursive callbacks)
4. User clicks train → C → JS: Request details (DEPARTURE_INDEX)
5. JS: Re-fetch API, match by vehicle ID + time
6. JS → C: Send direction + destination
7. C: Update detail window

### Station Handling

**Default stations (fallback):**
- Brussels-Central: `BE.NMBS.008813003`
- Antwerp-Central: `BE.NMBS.008821006`
- Ghent-Sint-Pieters: `BE.NMBS.008892007`
- Liège-Guillemins: `BE.NMBS.008841004`
- Leuven: `BE.NMBS.008833001`

User favorites (up to 6) configured via web interface. Station cache (~600 stations, ~50KB) fetched from iRail API and stored in localStorage.

### Data Persistence

**LocalStorage keys:**

| Key | Content | Size | Purpose |
|-----|---------|------|---------|
| `nmbs_station_cache` | Full station list from API | ~50KB | Offline names/IDs |
| `nmbs_favorite_stations` | Array of iRail IDs | ~200B | User's favorites (max 6) |
| `nmbs_smart_schedules` | Schedule rules | ~500B | Time-based automation |
| `nmbs_from_station` | iRail ID | ~30B | Last from station |
| `nmbs_to_station` | iRail ID | ~30B | Last to station |
| `nmbs_connections` | Connection identifiers | ~2KB | For detail fetching |

Data saved on station change and after each departure sent. Validates on load, resets to defaults on corruption.

### API Request Debouncing

Requests debounced with 500ms delay to prevent excessive API calls during rapid station switching.

### Direction vs Destination

**Critical distinction:**
- **Direction**: Train's final terminus (e.g., "Ostend")
- **Destination**: User's arrival station (e.g., "Ghent-Sint-Pieters")

**List view**: Shows direction only (where train is heading)
**Detail view**: Shows both direction AND destination

### On-Demand Detail Fetching

App stores connection identifiers (`{vehicle, departTime}`) and re-fetches full details only when user opens detail view.

**Why re-fetch:**
- Gets latest delay information
- Reduces initial load size (AppMessage ~1KB limit)
- Ensures fresh data

### Sequential Message Sending

Due to AppMessage queue limitations, departures sent one-at-a-time with recursive callbacks:
1. Send `MSG_SEND_COUNT` (tells C how many to expect)
2. Send `MSG_SEND_DEPARTURE[0]`, wait for success
3. Send `[1]`, wait, continue recursively

### Data Structure

Fixed-size arrays (not pointers) for embedded stability:

```c
typedef struct {
  char destination[32];
  char depart_time[8];       // "HH:MM" for display
  time_t depart_timestamp;   // Unix timestamp for glance expiration
  char arrive_time[8];
  char platform[4];
  char train_type[8];
  char duration[8];
  int8_t depart_delay;       // Minutes
  int8_t arrive_delay;       // Minutes
  bool is_direct;
  bool platform_changed;
} TrainDeparture;

static TrainDeparture s_departures[MAX_DEPARTURES];
static uint8_t s_num_departures = 0;
```

Data copied with `strncpy` bounds checking. When last departure received, reload menu.

### Detail Window Updates

**Critical:** Must regenerate text content, not just mark dirty.

**Pattern:**
1. Use `text_layer_set_text()` to update content (not just `layer_mark_dirty()`)
2. Recalculate scroll layer content size after text changes
3. Call `update_detail_window()` when `MSG_SEND_DETAIL` received

Shows loading state initially, full details after data arrives.

### Error Handling

All errors send `MSG_SEND_COUNT` with `DATA_COUNT: 0`:
- Same from/to station
- API request failure
- JSON parse error
- No connections found

### Debugging

```bash
pebble logs --emulator diorite
```

**Common issues:**
- **Detail stuck on "Loading"**: Ensure `update_detail_window()` called on `MSG_SEND_DETAIL`
- **Menu not updating**: Use `menu_layer_reload_data()` not `layer_mark_dirty()`

### Message Keys

All keys defined in `package.json` auto-generate `MESSAGE_KEY_*` constants in C.

## User Configuration

Web-based config interface (accessed via Pebble mobile app) for favorite stations (max 6) and smart route schedules.

### Architecture

**Data flow:**
1. iRail API → Station cache (~600 stations, ~50KB)
2. Config page → User selects favorites, defines schedules (using iRail IDs)
3. Watch app → Receives IDs, displays names, makes API requests

**Why iRail IDs:** Stable across localizations, API compatible, no ambiguity.

### Configuration Page (config.html)

**Must be hosted publicly** (GitHub Pages recommended).

**UI sections:**
1. Active Route Banner (green if active, gray if none)
2. Favorite Stations (searchable, max 6)
3. Smart Schedules
   - Simple: Template buttons (Morning Commute, Evening Return, Weekend)
   - Advanced: List with add/remove

**Returns:** `pebblejs://close#encodedJSON` with `{favoriteStations: [...], smartSchedules: [...]}`

### Schedule Evaluation

Runs on app launch, data request, config save. First matching schedule wins (order matters).

**Schedule structure:**
```javascript
{
  id: "morning-commute-1234567890",
  fromId: "BE.NMBS.008833001",
  toId: "BE.NMBS.008813003",
  days: [1, 2, 3, 4, 5],  // 0=Sun, 1=Mon, ..., 6=Sat
  startTime: "06:00",     // HH:MM
  endTime: "12:00",
  enabled: true
}
```

### C-Side Station Handling

```c
typedef struct {
  char name[64];
  char irail_id[32];
} Station;

#define MAX_FAVORITE_STATIONS 6
static Station s_stations[MAX_FAVORITE_STATIONS];
```

Fallback to 5 default stations if config fails.

### Configuration Flow

1. User opens Pebble app Settings → NMBS
2. Config page loads, fetches stations, displays favorites/schedules
3. User configures, saves
4. JS evaluates schedules, sends to C: station list + active route
5. Watch updates menu, requests fresh data

## VSCode IntelliSense

`.vscode/c_cpp_properties.json` configures Pebble SDK header paths: `~/Library/Application Support/Pebble SDK/SDKs/4.5/`

## AppGlances

Quick train schedule previews without opening app. Available on basalt, chalk, diorite, emery (not aplite).

### Architecture

**Hybrid updates:**
- **Background**: Worker ticks every 10 min → app fetches data → updates glances → exits
- **On-demand**: User opens app → updates glances + UI

**Code reuse:** ~95% reused (schedule eval, API fetch, data structures, message protocol)

### Glance Format

**Per-train slices:**
- On time: `"14:23 • Plat. 3 • Brussels-Central"`
- Delayed: `"14:23 (+5) • Plat. 3 • Brussels-Central"`
- Expiration: Each glance expires at train's departure time (self-cleaning)
- Empty: `"No trains available"` (expires in 1 hour)

### Configuration

**Worker interval:** `UPDATE_INTERVAL_MINUTES` in `worker_src/c/worker.c` (1 min testing, 10 min production)

**Glance expiration:** Automatic at departure time. If delayed, stays until new time.

---

## Code Organization

The C codebase is modularized for maintainability:

### File Structure

```
src/c/
├── nmbs.c              # Main entry point (140 lines)
├── types.h             # Shared data structures and constants
├── state.h/c           # Global state management
├── utils.h/c           # Utility functions (station abbreviation)
├── menu_layer.h/c      # MenuLayer UI rendering (~520 lines)
├── detail_window.h/c   # Detail window implementation (~270 lines)
├── api_handler.h/c     # AppMessage communication (~380 lines)
└── glances.h/c         # AppGlances support (~80 lines)
```

### Module Responsibilities

**nmbs.c** - App lifecycle
- Resource loading/unloading
- Main window setup
- Module initialization
- Entry point (`main()`)

**types.h** - Shared definitions
- Data structures (`Station`, `TrainDeparture`, `JourneyLeg`, `JourneyDetail`)
- Message type constants
- Loading state enum
- Maximum limits

**state.c/h** - Global state
- Station data (favorites, selections)
- Departure data array
- Loading state machine
- Request ID tracking
- Marquee animation state
- Getter/setter functions for encapsulation

**utils.c/h** - Helper functions
- `abbreviate_station_name()` - Shortens Belgian station names for display

**menu_layer.c/h** - Main UI
- MenuLayer callbacks (sections, rows, drawing)
- Station selector rendering
- Train departure rendering (times, delays, platforms, icons)
- Marquee scrolling animation
- Platform-specific layouts

**detail_window.c/h** - Detail view
- Journey detail window lifecycle
- Scrollable leg-by-leg display
- Platform boxes and timing
- Station abbreviation

**api_handler.c/h** - Backend communication
- AppMessage registration
- Train data requests
- Detail data requests
- Message parsing (departures, stations, config)
- Request ID validation (idempotency)
- Error handling

**glances.c/h** - Background updates
- AppGlance slice generation
- Worker message handling
- Platform guards (`#if defined(PBL_HEALTH)`)

### Inter-Module Communication

```
nmbs.c
  ├─> state.c (init, resource pointers)
  ├─> menu_layer.c (setup callbacks, icons)
  ├─> api_handler.c (init with menu ref)
  └─> glances.c (worker subscription)

menu_layer.c
  ├─> state.c (read stations, departures, marquee)
  └─> api_handler.c (request data on selection)

detail_window.c
  ├─> state.c (read journey detail)
  └─> utils.c (abbreviate stations)

api_handler.c
  ├─> state.c (update data, load state)
  ├─> detail_window.c (show/update window)
  └─> glances.c (update after data received)

glances.c
  ├─> state.c (read departures)
  └─> api_handler.c (request data on worker trigger)
```

### Benefits of Modular Structure

1. **Separation of concerns**: Each file has one clear responsibility
2. **Easier debugging**: Isolated logic is simpler to trace
3. **Testability**: Modules can be unit tested independently
4. **Maintainability**: Changes localized to specific files
5. **Code reuse**: Shared state/types prevent duplication
6. **Faster builds**: Incremental compilation on changes

### Adding New Features

- **New UI element**: Add to `menu_layer.c` or create new UI module
- **New API message**: Add to `api_handler.c` inbox handler
- **New data field**: Add to `types.h` struct, update state getters
- **New utility**: Add to `utils.c`

---

## JavaScript Code Organization

JavaScript split into 7 CommonJS modules (from 903 lines):

```
src/pkjs/
├── 00-constants.js      # API URLs, message types, storage keys
├── 01-storage.js        # LocalStorage (station cache, favorites, schedules)
├── 02-api.js            # iRail API requests (connections, stations, details)
├── 03-data-processor.js # Format times, durations, parse connections
├── 04-message-handler.js # AppMessage protocol, send to watch
├── 05-config-manager.js # Schedule eval, Pebble event listeners
└── index.js             # Entry point (requires config-manager)
```

**Module pattern:** CommonJS with `require()` and `module.exports`
**File naming:** 00-05 prefix ensures correct webpack load order
**Event listeners:** Registered at module level in 05-config-manager.js (where `Pebble` object is accessible)

## Documentation

**Always document relevant changes in this CLAUDE.md file.**