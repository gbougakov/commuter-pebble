#include "menu_layer.h"
#include "state.h"
#include "api_handler.h"

// Icon resources (set by menu_layer_init)
static GBitmap *s_icon_switch = NULL;
static GBitmap *s_icon_switch_white = NULL;
static GBitmap *s_icon_airport = NULL;
static GBitmap *s_icon_airport_white = NULL;
static GBitmap *s_icon_start = NULL;
static GBitmap *s_icon_start_white = NULL;
static GBitmap *s_icon_finish = NULL;
static GBitmap *s_icon_finish_white = NULL;

// Marquee timer callback
static void marquee_timer_callback(void *data) {
  // Slow scroll: 1 pixel per frame
  if (state_get_marquee_offset() < state_get_marquee_max_offset()) {
    state_set_marquee_offset(state_get_marquee_offset() + 1);

    // Redraw the menu layer (we need to get menu layer from data passed in)
    MenuLayer *menu_layer = (MenuLayer *)data;
    layer_mark_dirty(menu_layer_get_layer(menu_layer));

    // Schedule next frame (80ms for slower scroll)
    state_set_marquee_timer(app_timer_register(80, marquee_timer_callback, menu_layer));
  } else {
    // Stop at the end - no looping
    state_set_marquee_timer(NULL);
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
    // Show 1 row for loading, error, or when no departures
    if (state_is_data_loading() || state_is_data_failed() || state_get_num_departures() == 0) {
      return 1;
    }
    return state_get_num_departures();
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
      station_name = (state_get_num_stations() > 0) ? state_get_stations()[state_get_from_station_index()].name : NULL;
    } else {
      // "To" station selector - use finish icon
      icon = selected ? s_icon_finish_white : s_icon_finish;
      station_name = (state_get_num_stations() > 0) ? state_get_stations()[state_get_to_station_index()].name : NULL;
    }

    // Draw icon (16x16) with some padding
    if (icon) {
      GRect icon_rect = GRect(4, 4, 16, 16);
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, icon, icon_rect);
    }

    // Draw station name or skeleton
    if (station_name) {
      // Draw actual station name
      graphics_context_set_text_color(ctx, text_color);
      graphics_draw_text(ctx,
                         station_name,
                         fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                         GRect(24, 0, bounds.size.w - 28, 20),
                         GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentLeft,
                         NULL);
    } else {
      // Draw skeleton: dithered rectangle placeholder
      GRect skeleton_rect = GRect(24, 4, bounds.size.w - 40, 16);

      // When selected, background is black so use white dither
      // When not selected, background is white so use black dither
      GColor dither_color = selected ? GColorWhite : GColorBlack;
      graphics_context_set_stroke_color(ctx, dither_color);

      // Create sparse dithered pattern (every 3rd pixel for less opacity)
      for (int y = skeleton_rect.origin.y; y < skeleton_rect.origin.y + skeleton_rect.size.h; y++) {
        for (int x = skeleton_rect.origin.x; x < skeleton_rect.origin.x + skeleton_rect.size.w; x++) {
          if ((x + y) % 3 == 0) {
            graphics_draw_pixel(ctx, GPoint(x, y));
          }
        }
      }
    }

    return;
  }

  // Section 1: Train departures

  // Show loading message if no stations have been received yet
  if (!state_are_stations_received()) {
    bool selected = menu_cell_layer_is_highlighted(cell_layer);
    GColor text_color = selected ? GColorWhite : GColorBlack;
    graphics_context_set_text_color(ctx, text_color);
    graphics_draw_text(ctx,
                       "Loading...",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(4, 12, bounds.size.w - 8, 24),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter,
                       NULL);
    return;
  }

  // Show loading indicator based on state machine
  if (state_is_data_loading()) {
    bool selected = menu_cell_layer_is_highlighted(cell_layer);
    GColor text_color = selected ? GColorWhite : GColorBlack;
    graphics_context_set_text_color(ctx, text_color);

    const char *loading_message;
    switch (state_get_load_state()) {
      case LOAD_STATE_CONNECTING:
        loading_message = "Connecting to phone...";
        break;
      case LOAD_STATE_FETCHING:
        loading_message = "Loading trains...";
        break;
      case LOAD_STATE_RECEIVING:
        loading_message = "Receiving trains...";
        break;
      case LOAD_STATE_ERROR:
        loading_message = "Connection failed";
        break;
      default:
        loading_message = "Loading...";
        break;
    }

    graphics_draw_text(ctx,
                       loading_message,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(4, 12, bounds.size.w - 8, 24),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter,
                       NULL);
    return;
  }

  // Show error if load failed
  if (state_is_data_failed()) {
    bool selected = menu_cell_layer_is_highlighted(cell_layer);
    GColor text_color = selected ? GColorWhite : GColorBlack;
    graphics_context_set_text_color(ctx, text_color);
    graphics_draw_text(ctx,
                       "Connection failed",
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(4, 10, bounds.size.w - 8, 24),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter,
                       NULL);
    return;
  }

  // No departures available
  if (state_get_num_departures() == 0) {
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

  TrainDeparture *departure = &state_get_departures()[cell_index->row];

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
    state_set_marquee_max_offset(text_size.w - details_rect.size.w);

    // Offset the text rect for marquee effect
    GRect marquee_rect = details_rect;
    marquee_rect.origin.x -= state_get_marquee_offset();
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
  AppTimer *timer = state_get_marquee_timer();
  if (timer) {
    app_timer_cancel(timer);
    state_set_marquee_timer(NULL);
  }

  // Reset marquee offset and update selected row
  state_set_marquee_offset(0);
  state_set_selected_row(new_index.row);

  // Start marquee animation after a brief delay
  state_set_marquee_timer(app_timer_register(500, marquee_timer_callback, menu_layer));
}

static void menu_select_callback(MenuLayer *menu_layer,
                                  MenuIndex *cell_index,
                                  void *context) {
  // Section 0: Station selectors
  if (cell_index->section == 0) {
    if (state_get_num_stations() == 0) {
      APP_LOG(APP_LOG_LEVEL_WARNING, "No stations loaded yet");
      return;
    }

    if (cell_index->row == 0) {
      // "From" station selector
      uint8_t new_index = (state_get_from_station_index() + 1) % state_get_num_stations();
      state_set_from_station_index(new_index);
      layer_mark_dirty(menu_layer_get_layer(menu_layer));
      APP_LOG(APP_LOG_LEVEL_INFO, "From station changed to: %s", state_get_stations()[new_index].name);
      // Request new data
      api_handler_request_train_data();
    } else {
      // "To" station selector
      uint8_t new_index = (state_get_to_station_index() + 1) % state_get_num_stations();
      state_set_to_station_index(new_index);
      layer_mark_dirty(menu_layer_get_layer(menu_layer));
      APP_LOG(APP_LOG_LEVEL_INFO, "To station changed to: %s", state_get_stations()[new_index].name);
      // Request new data
      api_handler_request_train_data();
    }
    return;
  }

  // Section 1: Train row selected - request details
  state_set_selected_departure_index(cell_index->row);
  api_handler_request_detail_data();
}

// Initialize menu layer
void menu_layer_init(MenuLayer *menu_layer, GBitmap *icon_switch, GBitmap *icon_switch_white,
                      GBitmap *icon_airport, GBitmap *icon_airport_white,
                      GBitmap *icon_start, GBitmap *icon_start_white,
                      GBitmap *icon_finish, GBitmap *icon_finish_white) {
  s_icon_switch = icon_switch;
  s_icon_switch_white = icon_switch_white;
  s_icon_airport = icon_airport;
  s_icon_airport_white = icon_airport_white;
  s_icon_start = icon_start;
  s_icon_start_white = icon_start_white;
  s_icon_finish = icon_finish;
  s_icon_finish_white = icon_finish_white;
}

// Get menu layer callbacks
MenuLayerCallbacks menu_layer_get_callbacks(void) {
  return (MenuLayerCallbacks) {
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_header_height = menu_get_header_height_callback,
    .draw_header = menu_draw_header_callback,
    .draw_row = menu_draw_row_callback,
    .get_cell_height = menu_get_cell_height_callback,
    .select_click = menu_select_callback,
    .selection_changed = menu_selection_changed_callback,
  };
}
