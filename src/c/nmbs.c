#include <pebble.h>

// Forward declarations
static void detail_window_load(Window *window);
static void detail_window_unload(Window *window);
static void request_train_data(void);
static void update_detail_window(void);

// UI elements
static Window *s_main_window;
static MenuLayer *s_menu_layer;
static StatusBarLayer *s_status_bar;
static Window *s_detail_window;
static ScrollLayer *s_detail_scroll_layer;
static TextLayer *s_detail_text_layer;

// Resources
static GBitmap *s_icon_switch;
static GBitmap *s_icon_switch_white;
static GBitmap *s_icon_airport;
static GBitmap *s_icon_airport_white;
static GBitmap *s_icon_start;
static GBitmap *s_icon_start_white;
static GBitmap *s_icon_finish;
static GBitmap *s_icon_finish_white;

// Marquee animation state
static AppTimer *s_marquee_timer = NULL;
static int16_t s_marquee_offset = 0;
static uint16_t s_selected_row = 0;
static int16_t s_marquee_max_offset = 0;

// Detail window state
static uint16_t s_selected_departure_index = 0;
static char s_detail_destination[32] = "";
static char s_detail_direction[32] = "";
static bool s_detail_received = false;

// Station data
typedef struct {
  char *name;
  char *code;  // Short code for display
} Station;

static Station s_stations[] = {
  {"Brussels-Central", "BXL-C"},
  {"Antwerp-Central", "ANT-C"},
  {"Ghent-Sint-Pieters", "GNT-SP"},
  {"Liège-Guillemins", "LGE-G"},
  {"Leuven", "LEU"}
};

#define NUM_STATIONS (sizeof(s_stations) / sizeof(Station))
static uint8_t s_from_station_index = 0;
static uint8_t s_to_station_index = 1;

// Message type constants (must match JavaScript)
#define MSG_REQUEST_DATA 1
#define MSG_SEND_DEPARTURE 2
#define MSG_SEND_COUNT 3
#define MSG_REQUEST_DETAILS 4
#define MSG_SEND_DETAIL 5

// Maximum number of departures we can store
#define MAX_DEPARTURES 11

// Train schedule data structure
typedef struct {
  char destination[32];
  char depart_time[8];
  char arrive_time[8];
  char platform[4];
  char train_type[8];
  char duration[8];
  int8_t depart_delay;  // Minutes of departure delay (0 = on time)
  int8_t arrive_delay;  // Minutes of arrival delay (0 = on time)
  bool is_direct;  // true = direct train, false = requires connection
  bool platform_changed;  // true = platform changed from original
} TrainDeparture;

// Dynamic train data (populated from API)
static TrainDeparture s_departures[MAX_DEPARTURES];
static uint8_t s_num_departures = 0;
static bool s_data_loading = false;

// Marquee timer callback
static void marquee_timer_callback(void *data) {
  // Slow scroll: 1 pixel per frame
  if (s_marquee_offset < s_marquee_max_offset) {
    s_marquee_offset += 1;

    // Redraw the menu layer
    layer_mark_dirty(menu_layer_get_layer(s_menu_layer));

    // Schedule next frame (80ms for slower scroll)
    s_marquee_timer = app_timer_register(80, marquee_timer_callback, NULL);
  } else {
    // Stop at the end - no looping
    s_marquee_timer = NULL;
  }
}

// MenuLayer Callbacks
static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *context) {
  return 2;  // Section 0: Station selectors, Section 1: Train departures
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer,
                                            uint16_t section_index,
                                            void *context) {
  if (section_index == 0) {
    return 2;  // "From" and "To" selectors
  } else {
    return s_data_loading ? 1 : s_num_departures;  // Train departures or loading indicator
  }
}

static int16_t menu_get_header_height_callback(MenuLayer *menu_layer,
                                                 uint16_t section_index,
                                                 void *context) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void menu_draw_header_callback(GContext *ctx,
                                        const Layer *cell_layer,
                                        uint16_t section_index,
                                        void *context) {
  GRect bounds = layer_get_bounds(cell_layer);

  // Draw darker background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw dotted border on top
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  for (int x = 0; x < bounds.size.w; x += 4) {
    graphics_draw_line(ctx, GPoint(x, 0), GPoint(x + 2, 0));
  }

  // Draw dotted border on bottom
  for (int x = 0; x < bounds.size.w; x += 4) {
    graphics_draw_line(ctx, GPoint(x, bounds.size.h - 1), GPoint(x + 2, bounds.size.h - 1));
  }

  // Draw header text
  const char *header_text = (section_index == 0) ? "Route" : "Connections";
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx,
                     header_text,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(4, -3, bounds.size.w - 8, bounds.size.h),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft,
                     NULL);
}

static void menu_draw_row_callback(GContext *ctx,
                                    const Layer *cell_layer,
                                    MenuIndex *cell_index,
                                    void *context) {
  GRect bounds = layer_get_bounds(cell_layer);

  // Section 0: Station selectors
  if (cell_index->section == 0) {
    bool selected = menu_cell_layer_is_highlighted(cell_layer);
    GColor text_color = selected ? GColorWhite : GColorBlack;

    // Draw icon on the left
    GBitmap *icon;
    const char *station_name;

    if (cell_index->row == 0) {
      // "From" station selector - use start icon
      icon = selected ? s_icon_start_white : s_icon_start;
      station_name = s_stations[s_from_station_index].name;
    } else {
      // "To" station selector - use finish icon
      icon = selected ? s_icon_finish_white : s_icon_finish;
      station_name = s_stations[s_to_station_index].name;
    }

    // Draw icon (16x16) with some padding
    if (icon) {
      GRect icon_rect = GRect(4, 4, 16, 16);
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, icon, icon_rect);
    }

    // Draw station name to the right of icon
    graphics_context_set_text_color(ctx, text_color);
    graphics_draw_text(ctx,
                       station_name,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(24, 0, bounds.size.w - 28, 20),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft,
                       NULL);

    return;
  }

  // Section 1: Train departures

  // Show loading indicator if data is being fetched
  if (s_data_loading) {
    bool selected = menu_cell_layer_is_highlighted(cell_layer);
    GColor text_color = selected ? GColorWhite : GColorBlack;
    graphics_context_set_text_color(ctx, text_color);
    graphics_draw_text(ctx,
                       "Loading trains...",
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(4, 10, bounds.size.w - 8, 24),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter,
                       NULL);
    return;
  }

  // No departures available
  if (s_num_departures == 0) {
    bool selected = menu_cell_layer_is_highlighted(cell_layer);
    GColor text_color = selected ? GColorWhite : GColorBlack;
    graphics_context_set_text_color(ctx, text_color);
    graphics_draw_text(ctx,
                       "No connections found",
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(4, 10, bounds.size.w - 8, 24),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter,
                       NULL);
    return;
  }

  TrainDeparture *departure = &s_departures[cell_index->row];


  // Check if this row is selected
  bool selected = menu_cell_layer_is_highlighted(cell_layer);
  GColor text_color = selected ? GColorWhite : GColorBlack;
  GColor platform_bg_color = selected ? GColorWhite : GColorBlack;
  GColor platform_text_color = selected ? GColorBlack : GColorWhite;

  // Define platform box dimensions (calculated early, drawn later)
  const int16_t platform_box_size = 24;
  const int16_t platform_box_margin = 4;

  // Calculate platform box position (right side of cell)
  GRect platform_box = GRect(
    bounds.size.w - platform_box_size - platform_box_margin,
    (bounds.size.h - platform_box_size) / 2,
    platform_box_size,
    platform_box_size
  );

  // Draw time range on the left (primary, bold, larger)
  const int16_t text_margin = 4;
  static char time_range[32];

  GRect time_rect = GRect(
    text_margin,
    0,
    bounds.size.w - platform_box_size - platform_box_margin - text_margin - 4,
    20
  );

  graphics_context_set_text_color(ctx, text_color);

  // Check if there are any delays
  bool has_delay = (departure->depart_delay > 0 || departure->arrive_delay > 0);
  GFont time_font;

  if (has_delay) {
    // Use smaller font and include delay indicators
    time_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    time_rect.origin.y += 2;  // Slightly lower for smaller font
    snprintf(time_range, sizeof(time_range), "%s+%d > %s+%d",
             departure->depart_time,
             departure->depart_delay,
             departure->arrive_time,
             departure->arrive_delay);
  } else {
    // Use larger font for on-time trains
    time_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    snprintf(time_range, sizeof(time_range), "%s > %s",
             departure->depart_time, departure->arrive_time);
  }

  graphics_draw_text(ctx,
                     time_range,
                     time_font,
                     time_rect,
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft,
                     NULL);

  // Draw train type in small box (below time, on the left)
  const int16_t train_type_box_width = 16;
  const int16_t train_type_box_height = 16;
  const int16_t train_type_y = 22;

  GRect train_type_box = GRect(
    text_margin,
    train_type_y + 1,
    train_type_box_width,
    train_type_box_height
  );

  // Calculate if we need extra space for connection icon
  int16_t icon_space = !departure->is_direct ? 18 : 0;  // 16px icon + 2px gap

  // Draw details (duration and destination) next to train type box (and icon if present)
  const int16_t details_x = text_margin + train_type_box_width + icon_space + 6;
  GRect details_rect = GRect(
    details_x,
    train_type_y,
    bounds.size.w - details_x - platform_box_size - platform_box_margin - 4,
    train_type_box_height
  );

  static char detail_text[80];
  snprintf(detail_text, sizeof(detail_text), "%s · %s",
           departure->duration, departure->destination);

  graphics_context_set_text_color(ctx, text_color);

  // Calculate text width to determine if scrolling is needed
  GFont detail_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GSize text_size = graphics_text_layout_get_content_size(
    detail_text,
    detail_font,
    GRect(0, 0, 500, train_type_box_height),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentLeft
  );

  bool needs_scroll = text_size.w > details_rect.size.w;

  // Draw details text first (before boxes to allow proper layering)
  if (selected && needs_scroll) {
    // Calculate how far we need to scroll (text width - visible width)
    s_marquee_max_offset = text_size.w - details_rect.size.w;

    // Offset the text rect for marquee effect
    GRect marquee_rect = details_rect;
    marquee_rect.origin.x -= s_marquee_offset;
    marquee_rect.size.w = text_size.w + 20;  // Just wide enough for the text

    graphics_draw_text(ctx,
                       detail_text,
                       detail_font,
                       marquee_rect,
                       GTextOverflowModeWordWrap,
                       GTextAlignmentLeft,
                       NULL);
  } else {
    graphics_draw_text(ctx,
                       detail_text,
                       detail_font,
                       details_rect,
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft,
                       NULL);
  }

  // Draw masking rectangles to hide overflow text (match the background color)
  GColor bg_color = selected ? GColorBlack : GColorWhite;

  // Left mask (covers train type box and connection icon if present)
  GRect left_mask = GRect(0, train_type_y, text_margin + train_type_box_width + icon_space + 2, train_type_box_height);
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_rect(ctx, left_mask, 0, GCornerNone);

  // Right mask (after details area, before platform box)
  GRect right_mask = GRect(
    details_rect.origin.x + details_rect.size.w,
    train_type_y,
    bounds.size.w - (details_rect.origin.x + details_rect.size.w),
    train_type_box_height
  );
  graphics_fill_rect(ctx, right_mask, 0, GCornerNone);

  // Check if this is an airport train (destination contains "Airport")
  bool is_airport = strstr(departure->destination, "Airport") != NULL;

  if (is_airport) {
    // Draw airport icon instead of train type box
    GRect icon_rect = GRect(
      train_type_box.origin.x,
      train_type_box.origin.y,
      16,
      16
    );

    GBitmap *icon = selected ? s_icon_airport_white : s_icon_airport;
    if (icon) {
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, icon, icon_rect);
    }
  } else {
    // Draw train type box background (on top of masks)
    graphics_context_set_fill_color(ctx, platform_bg_color);
    graphics_fill_rect(ctx, train_type_box, 2, GCornersAll);

    // Draw train type text
    graphics_context_set_text_color(ctx, platform_text_color);
    GRect train_type_text_rect = train_type_box;
    train_type_text_rect.origin.y -= 2;  // Adjust for vertical centering

    // Truncate train type to 2 characters max for tight box fit
    static char train_type_display[3];  // 2 chars + null terminator
    snprintf(train_type_display, sizeof(train_type_display), "%.2s", departure->train_type);

    graphics_draw_text(ctx,
                       train_type_display,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       train_type_text_rect,
                       GTextOverflowModeFill,
                       GTextAlignmentCenter,
                       NULL);
  }

  // Draw connection icon if this train requires a connection
  if (!departure->is_direct) {
    GRect icon_rect = GRect(
      train_type_box.origin.x + train_type_box.size.w + 2,
      train_type_box.origin.y,
      16,
      16
    );

    // Use white icon when selected, black icon otherwise
    GBitmap *icon = selected ? s_icon_switch_white : s_icon_switch;
    if (icon) {
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, icon, icon_rect);
    }
  }

  // Draw platform box background (drawn last to appear on top)
  if (departure->platform_changed) {
    // Platform changed - draw outline only
    graphics_context_set_stroke_color(ctx, platform_bg_color);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_round_rect(ctx, platform_box, 2);
  } else {
    // Normal platform - filled box
    graphics_context_set_fill_color(ctx, platform_bg_color);
    graphics_fill_rect(ctx, platform_box, 2, GCornersAll);
  }

  // Draw platform number (adjust rect for vertical centering)
  graphics_context_set_text_color(ctx, departure->platform_changed ? text_color : platform_text_color);
  GRect platform_text_rect = platform_box;
  platform_text_rect.origin.y -= 5;  // Lift upward to vertically center larger font

  graphics_draw_text(ctx,
                     departure->platform,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     platform_text_rect,
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter,
                     NULL);
}

static int16_t menu_get_cell_height_callback(MenuLayer *menu_layer,
                                              MenuIndex *cell_index,
                                              void *context) {
  // Section 0: Station selector rows are shorter
  if (cell_index->section == 0) {
    return PBL_IF_ROUND_ELSE(
      MENU_CELL_ROUND_FOCUSED_SHORT_CELL_HEIGHT,
      24  // Shorter height for station selectors
    );
  }

  // Section 1: Train departure rows
  return PBL_IF_ROUND_ELSE(
    menu_layer_is_index_selected(menu_layer, cell_index) ?
      MENU_CELL_ROUND_FOCUSED_SHORT_CELL_HEIGHT : MENU_CELL_ROUND_UNFOCUSED_TALL_CELL_HEIGHT,
    44  // Standard height for train rows
  );
}

static void menu_selection_changed_callback(MenuLayer *menu_layer,
                                            MenuIndex new_index,
                                            MenuIndex old_index,
                                            void *context) {
  // Cancel existing marquee timer
  if (s_marquee_timer) {
    app_timer_cancel(s_marquee_timer);
    s_marquee_timer = NULL;
  }

  // Reset marquee offset and update selected row
  s_marquee_offset = 0;
  s_selected_row = new_index.row;

  // Start marquee animation after a brief delay
  s_marquee_timer = app_timer_register(500, marquee_timer_callback, NULL);
}

static void menu_select_callback(MenuLayer *menu_layer,
                                  MenuIndex *cell_index,
                                  void *context) {
  // Section 0: Station selectors
  if (cell_index->section == 0) {
    if (cell_index->row == 0) {
      // "From" station selector
      s_from_station_index = (s_from_station_index + 1) % NUM_STATIONS;
      layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
      APP_LOG(APP_LOG_LEVEL_INFO, "From station changed to: %s", s_stations[s_from_station_index].name);
      // Request new data
      request_train_data();
    } else {
      // "To" station selector
      s_to_station_index = (s_to_station_index + 1) % NUM_STATIONS;
      layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
      APP_LOG(APP_LOG_LEVEL_INFO, "To station changed to: %s", s_stations[s_to_station_index].name);
      // Request new data
      request_train_data();
    }
    return;
  }

  // Section 1: Train row selected - show detail window
  s_selected_departure_index = cell_index->row;
  TrainDeparture *departure = &s_departures[s_selected_departure_index];
  APP_LOG(APP_LOG_LEVEL_INFO, "Selected train to %s", departure->destination);

  // Request fresh details from JavaScript
  s_detail_received = false;
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, MESSAGE_KEY_MESSAGE_TYPE, MSG_REQUEST_DETAILS);
  dict_write_uint8(iter, MESSAGE_KEY_DEPARTURE_INDEX, s_selected_departure_index);
  app_message_outbox_send();

  // Create detail window if not already created
  if (!s_detail_window) {
    s_detail_window = window_create();
    window_set_window_handlers(s_detail_window, (WindowHandlers) {
      .load = detail_window_load,
      .unload = detail_window_unload,
    });
  }

  // Push detail window onto stack
  const bool animated = true;
  window_stack_push(s_detail_window, animated);
  
}

// Update detail window content
static void update_detail_window(void) {
  if (!s_detail_text_layer) return;

  TrainDeparture *departure = &s_departures[s_selected_departure_index];
  static char detail_buffer[300];

  if (s_detail_received) {
    // Show full details with direction and destination
    snprintf(detail_buffer, sizeof(detail_buffer),
             "Direction: %s\n\n"
             "Destination: %s\n\n"
             "🚂 %s Train\n\n"
             "🕐 %s → %s\n"
             "%s%s\n\n"
             "🏁 Platform %s%s\n\n"
             "⏱ Duration: %s\n"
             "%s",
             s_detail_direction,
             s_detail_destination,
             departure->train_type,
             departure->depart_time,
             departure->arrive_time,
             (departure->depart_delay > 0 || departure->arrive_delay > 0) ?
               "⚠️ Delayed!" : "✅ On time",
             (departure->depart_delay > 0 || departure->arrive_delay > 0) ?
               (departure->depart_delay > 0 && departure->arrive_delay > 0 ?
                 " (both ways)" : "") : "",
             departure->platform,
             departure->platform_changed ? " ⚠️ CHANGED" : "",
             departure->duration,
             !departure->is_direct ?
               "🔄 Connection required" : "➡️ Direct train"
    );
  } else {
    // Show loading message while waiting for details
    snprintf(detail_buffer, sizeof(detail_buffer),
             "Loading connection details...\n\n"
             "🚂 %s Train\n\n"
             "🕐 %s → %s\n"
             "%s%s\n\n"
             "🏁 Platform %s%s\n\n"
             "⏱ Duration: %s",
             departure->train_type,
             departure->depart_time,
             departure->arrive_time,
             (departure->depart_delay > 0 || departure->arrive_delay > 0) ?
               "⚠️ Delayed!" : "✅ On time",
             (departure->depart_delay > 0 || departure->arrive_delay > 0) ?
               (departure->depart_delay > 0 && departure->arrive_delay > 0 ?
                 " (both ways)" : "") : "",
             departure->platform,
             departure->platform_changed ? " ⚠️ CHANGED" : "",
             departure->duration
    );
  }

  text_layer_set_text(s_detail_text_layer, detail_buffer);

  // Update scroll layer content size
  if (s_detail_scroll_layer) {
    Layer *window_layer = window_get_root_layer(s_detail_window);
    GRect bounds = layer_get_bounds(window_layer);
    GSize text_size = text_layer_get_content_size(s_detail_text_layer);
    text_layer_set_size(s_detail_text_layer, GSize(bounds.size.w - 16, text_size.h + 16));
    scroll_layer_set_content_size(s_detail_scroll_layer, GSize(bounds.size.w, text_size.h + 24));
  }
}

// Detail window lifecycle
static void detail_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create ScrollLayer
  s_detail_scroll_layer = scroll_layer_create(bounds);
  scroll_layer_set_click_config_onto_window(s_detail_scroll_layer, window);

  // Create TextLayer for detail content (large height for scrolling)
  s_detail_text_layer = text_layer_create(GRect(8, 8, bounds.size.w - 16, 2000));
  text_layer_set_text_color(s_detail_text_layer, GColorBlack);
  text_layer_set_background_color(s_detail_text_layer, GColorClear);
  text_layer_set_font(s_detail_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_overflow_mode(s_detail_text_layer, GTextOverflowModeWordWrap);

  // Add TextLayer to ScrollLayer
  scroll_layer_add_child(s_detail_scroll_layer, text_layer_get_layer(s_detail_text_layer));

  // Add ScrollLayer to window
  layer_add_child(window_layer, scroll_layer_get_layer(s_detail_scroll_layer));

  // Set initial content
  update_detail_window();
}

static void detail_window_unload(Window *window) {
  text_layer_destroy(s_detail_text_layer);
  scroll_layer_destroy(s_detail_scroll_layer);
}

// Main window lifecycle
static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create StatusBarLayer
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorBlack, GColorWhite);
  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));

  // Calculate bounds for MenuLayer (below status bar)
  int16_t status_bar_height = STATUS_BAR_LAYER_HEIGHT;
  GRect menu_bounds = bounds;
  menu_bounds.origin.y = status_bar_height;
  menu_bounds.size.h -= status_bar_height;

  // Create MenuLayer
  s_menu_layer = menu_layer_create(menu_bounds);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);

  // Set up MenuLayer callbacks
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_header_height = menu_get_header_height_callback,
    .draw_header = menu_draw_header_callback,
    .draw_row = menu_draw_row_callback,
    .get_cell_height = menu_get_cell_height_callback,
    .select_click = menu_select_callback,
    .selection_changed = menu_selection_changed_callback,
  });

  // Add MenuLayer to window
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
}

static void window_unload(Window *window) {
  // Cancel marquee timer if running
  if (s_marquee_timer) {
    app_timer_cancel(s_marquee_timer);
    s_marquee_timer = NULL;
  }

  // Destroy MenuLayer
  menu_layer_destroy(s_menu_layer);

  // Destroy StatusBarLayer
  status_bar_layer_destroy(s_status_bar);
}

// Request train data from JavaScript
static void request_train_data(void) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  dict_write_uint8(iter, MESSAGE_KEY_MESSAGE_TYPE, MSG_REQUEST_DATA);
  dict_write_cstring(iter, MESSAGE_KEY_FROM_STATION, s_stations[s_from_station_index].name);
  dict_write_cstring(iter, MESSAGE_KEY_TO_STATION, s_stations[s_to_station_index].name);

  app_message_outbox_send();

  s_data_loading = true;
  s_num_departures = 0;
  menu_layer_reload_data(s_menu_layer);

  APP_LOG(APP_LOG_LEVEL_INFO, "Requesting data: %s -> %s",
          s_stations[s_from_station_index].name,
          s_stations[s_to_station_index].name);
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

  if (message_type == MSG_SEND_COUNT) {
    // Received departure count
    Tuple *count_tuple = dict_find(iterator, MESSAGE_KEY_DATA_COUNT);
    if (count_tuple) {
      s_num_departures = count_tuple->value->uint8;
      APP_LOG(APP_LOG_LEVEL_INFO, "Expecting %d departures", s_num_departures);

      if (s_num_departures == 0) {
        s_data_loading = false;
        menu_layer_reload_data(s_menu_layer);
      }
    }
  } else if (message_type == MSG_SEND_DEPARTURE) {
    // Received departure data
    Tuple *index_tuple = dict_find(iterator, MESSAGE_KEY_DEPARTURE_INDEX);
    if (!index_tuple) return;

    uint8_t index = index_tuple->value->uint8;
    if (index >= MAX_DEPARTURES) return;

    TrainDeparture *dep = &s_departures[index];

    // Read all fields
    Tuple *dest = dict_find(iterator, MESSAGE_KEY_DESTINATION);
    Tuple *depart = dict_find(iterator, MESSAGE_KEY_DEPART_TIME);
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

    APP_LOG(APP_LOG_LEVEL_INFO, "Received departure %d: %s", index, dep->destination);

    // If this is the last departure, stop loading
    if (index == s_num_departures - 1) {
      s_data_loading = false;
      APP_LOG(APP_LOG_LEVEL_INFO, "All departures received");
      // Reload menu data to update row count and scrolling
      menu_layer_reload_data(s_menu_layer);
    } else {
      layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
    }
  } else if (message_type == MSG_SEND_DETAIL) {
    // Received connection detail data
    Tuple *dest = dict_find(iterator, MESSAGE_KEY_DETAIL_DESTINATION);
    Tuple *dir = dict_find(iterator, MESSAGE_KEY_DETAIL_DIRECTION);

    if (dest) {
      strncpy(s_detail_destination, dest->value->cstring, sizeof(s_detail_destination) - 1);
      s_detail_destination[sizeof(s_detail_destination) - 1] = '\0';
    }

    if (dir) {
      strncpy(s_detail_direction, dir->value->cstring, sizeof(s_detail_direction) - 1);
      s_detail_direction[sizeof(s_detail_direction) - 1] = '\0';
    }

    s_detail_received = true;
    APP_LOG(APP_LOG_LEVEL_INFO, "Received details: %s -> %s", s_detail_direction, s_detail_destination);

    // Update detail window if it's currently shown
    if (s_detail_window && window_stack_contains_window(s_detail_window)) {
      update_detail_window();
    }
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed: %d", (int)reason);
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

// App initialization
static void init(void) {
  // Load resources
  s_icon_switch = gbitmap_create_with_resource(RESOURCE_ID_ICON_SWITCH);
  s_icon_switch_white = gbitmap_create_with_resource(RESOURCE_ID_ICON_SWITCH_WHITE);
  s_icon_airport = gbitmap_create_with_resource(RESOURCE_ID_ICON_AIRPORT);
  s_icon_airport_white = gbitmap_create_with_resource(RESOURCE_ID_ICON_AIRPORT_WHITE);
  s_icon_start = gbitmap_create_with_resource(RESOURCE_ID_ICON_START);
  s_icon_start_white = gbitmap_create_with_resource(RESOURCE_ID_ICON_START_WHITE);
  s_icon_finish = gbitmap_create_with_resource(RESOURCE_ID_ICON_FINISH);
  s_icon_finish_white = gbitmap_create_with_resource(RESOURCE_ID_ICON_FINISH_WHITE);

  // Create main window
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  // Push window to stack
  const bool animated = true;
  window_stack_push(s_main_window, animated);

  // Register AppMessage callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Open AppMessage with appropriate buffer sizes
  app_message_open(512, 512);

  // Request initial train data
  request_train_data();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "NMBS Schedule App initialized");
}

static void deinit(void) {
  // Destroy resources
  gbitmap_destroy(s_icon_switch);
  gbitmap_destroy(s_icon_switch_white);
  gbitmap_destroy(s_icon_airport);
  gbitmap_destroy(s_icon_airport_white);
  gbitmap_destroy(s_icon_start);
  gbitmap_destroy(s_icon_start_white);
  gbitmap_destroy(s_icon_finish);
  gbitmap_destroy(s_icon_finish_white);

  // Destroy main window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
