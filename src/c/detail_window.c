#include "detail_window.h"
#include "state.h"
#include "utils.h"

// UI elements
static Window *s_detail_window = NULL;
static StatusBarLayer *s_detail_status_bar = NULL;
static ScrollLayer *s_detail_scroll_layer = NULL;
static Layer *s_detail_content_layer = NULL;

// Forward declarations
static void detail_window_load(Window *window);
static void detail_window_unload(Window *window);
static void detail_content_update_proc(Layer *layer, GContext *ctx);

// Custom drawing function for detail window content
static void detail_content_update_proc(Layer *layer, GContext *ctx) {
  if (!state_is_detail_received()) {
    // Show loading message
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx,
                       "Loading journey details...",
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(8, 40, layer_get_bounds(layer).size.w - 16, 60),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);
    return;
  }

  JourneyDetail *journey = state_get_journey_detail();

  // Draw each leg
  int16_t y_offset = 8;
  const int16_t margin = 8;
  const int16_t platform_box_size = 16;
  const int16_t line_height = 20;
  const int16_t leg_spacing = 8;

  for (uint8_t i = 0; i < journey->leg_count; i++) {
    JourneyLeg *leg = &journey->legs[i];

    // Abbreviate station names
    char depart_abbrev[32];
    char arrive_abbrev[32];
    abbreviate_station_name(leg->depart_station, depart_abbrev, sizeof(depart_abbrev));
    abbreviate_station_name(leg->arrive_station, arrive_abbrev, sizeof(arrive_abbrev));

    // Draw departure row - Time + delay
    static char depart_time_str[16];
    if (leg->depart_delay > 0) {
      snprintf(depart_time_str, sizeof(depart_time_str), "%s +%d", leg->depart_time, leg->depart_delay);
    } else {
      snprintf(depart_time_str, sizeof(depart_time_str), "%s", leg->depart_time);
    }

    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx,
                       depart_time_str,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(margin, y_offset, 80, line_height),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft,
                       NULL);

    // Platform box (departure)
    GRect depart_platform_box = GRect(layer_get_bounds(layer).size.w - margin - platform_box_size,
                                      y_offset + 2,
                                      platform_box_size,
                                      platform_box_size);

    if (leg->depart_platform_changed) {
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_round_rect(ctx, depart_platform_box, 2);
      graphics_context_set_text_color(ctx, GColorBlack);
    } else {
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, depart_platform_box, 2, GCornersAll);
      graphics_context_set_text_color(ctx, GColorWhite);
    }

    GRect platform_text_rect = depart_platform_box;
    platform_text_rect.origin.y -= 2;
    graphics_draw_text(ctx,
                       leg->depart_platform,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       platform_text_rect,
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter,
                       NULL);

    y_offset += line_height;

    // Departure station name
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx,
                       depart_abbrev,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(margin, y_offset, layer_get_bounds(layer).size.w - 2 * margin, line_height),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft,
                       NULL);

    y_offset += (line_height + 7);

    // Journey line - Draw dotted vertical line
    const int16_t line_x = margin + 2;
    for (int16_t dot_y = y_offset; dot_y < y_offset + line_height * 2; dot_y += 4) {
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, GRect(line_x, dot_y, 2, 2), 0, GCornerNone);
    }

    // Vehicle + direction text
    static char vehicle_line[64];
    snprintf(vehicle_line, sizeof(vehicle_line), "%s to %s", leg->vehicle, leg->direction);
    graphics_draw_text(ctx,
                       vehicle_line,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(margin + 10, y_offset, layer_get_bounds(layer).size.w - 2 * margin - 10, line_height),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft,
                       NULL);

    y_offset += line_height;

    // Stop count
    static char stop_line[32];
    snprintf(stop_line, sizeof(stop_line), "%d stop%s", leg->stop_count, leg->stop_count == 1 ? "" : "s");
    graphics_draw_text(ctx,
                       stop_line,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(margin + 10, y_offset, layer_get_bounds(layer).size.w - 2 * margin - 10, line_height),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft,
                       NULL);

    y_offset += line_height;

    // Draw arrival row - Time + delay
    static char arrive_time_str[16];
    if (leg->arrive_delay > 0) {
      snprintf(arrive_time_str, sizeof(arrive_time_str), "%s +%d", leg->arrive_time, leg->arrive_delay);
    } else {
      snprintf(arrive_time_str, sizeof(arrive_time_str), "%s", leg->arrive_time);
    }

    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx,
                       arrive_time_str,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(margin, y_offset, 80, line_height),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft,
                       NULL);

    // Platform box (arrival)
    GRect arrive_platform_box = GRect(layer_get_bounds(layer).size.w - margin - platform_box_size,
                                      y_offset + 2,
                                      platform_box_size,
                                      platform_box_size);

    if (leg->arrive_platform_changed) {
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_round_rect(ctx, arrive_platform_box, 2);
      graphics_context_set_text_color(ctx, GColorBlack);
    } else {
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, arrive_platform_box, 2, GCornersAll);
      graphics_context_set_text_color(ctx, GColorWhite);
    }

    platform_text_rect = arrive_platform_box;
    platform_text_rect.origin.y -= 2;
    graphics_draw_text(ctx,
                       leg->arrive_platform,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       platform_text_rect,
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter,
                       NULL);

    y_offset += line_height;

    // Arrival station name
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx,
                       arrive_abbrev,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(margin, y_offset, layer_get_bounds(layer).size.w - 2 * margin, line_height),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft,
                       NULL);

    y_offset += line_height + leg_spacing;
  }
}

// Update detail window content (triggers redraw)
void detail_window_update(void) {
  if (!s_detail_content_layer) return;

  layer_mark_dirty(s_detail_content_layer);

  if (s_detail_scroll_layer) {
    Layer *window_layer = window_get_root_layer(s_detail_window);
    GRect bounds = layer_get_bounds(window_layer);
    int16_t content_height = 24 + (state_get_journey_detail()->leg_count * 128);
    layer_set_frame(s_detail_content_layer, GRect(0, 0, bounds.size.w, content_height));
    scroll_layer_set_content_size(s_detail_scroll_layer, GSize(bounds.size.w, content_height));
  }
}

// Detail window lifecycle
static void detail_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create StatusBarLayer
  s_detail_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_detail_status_bar, GColorBlack, GColorWhite);
  layer_add_child(window_layer, status_bar_layer_get_layer(s_detail_status_bar));

  // Calculate bounds for ScrollLayer (below status bar)
  int16_t status_bar_height = STATUS_BAR_LAYER_HEIGHT;
  GRect scroll_bounds = bounds;
  scroll_bounds.origin.y = status_bar_height;
  scroll_bounds.size.h -= status_bar_height;

  // Create ScrollLayer
  s_detail_scroll_layer = scroll_layer_create(scroll_bounds);
  scroll_layer_set_click_config_onto_window(s_detail_scroll_layer, window);

  // Create custom Layer for detail content (large height for scrolling)
  s_detail_content_layer = layer_create(GRect(0, 0, scroll_bounds.size.w, 2000));
  layer_set_update_proc(s_detail_content_layer, detail_content_update_proc);

  // Add content Layer to ScrollLayer
  scroll_layer_add_child(s_detail_scroll_layer, s_detail_content_layer);

  // Add ScrollLayer to window
  layer_add_child(window_layer, scroll_layer_get_layer(s_detail_scroll_layer));

  // Set initial content - mark dirty AFTER adding to hierarchy to ensure first draw
  layer_mark_dirty(s_detail_content_layer);
}

static void detail_window_unload(Window *window) {
  layer_destroy(s_detail_content_layer);
  s_detail_content_layer = NULL;
  scroll_layer_destroy(s_detail_scroll_layer);
  s_detail_scroll_layer = NULL;
  status_bar_layer_destroy(s_detail_status_bar);
  s_detail_status_bar = NULL;
}

// Create and show detail window
void detail_window_show(void) {
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

// Destroy detail window
void detail_window_destroy(void) {
  if (s_detail_window) {
    window_destroy(s_detail_window);
    s_detail_window = NULL;
  }
}

// Get detail window instance
Window* detail_window_get_instance(void) {
  return s_detail_window;
}
