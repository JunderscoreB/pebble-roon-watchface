/*
 * Pebble Roon Remote Watchface
 * Copyright (c) 2026 J_B
 *
 * Released under the MIT License.
 *
 * AI Disclosure: Portions of this file were generated and optimized with the assistance of generative AI.
 * Co-Authored-By: Google Gemini <noreply@google.com>
 */

#include <pebble.h>
#include <stdlib.h>

#define KEY_COMMAND 0
#define KEY_ZONE_NAME 1
#define KEY_TRACK 2
#define KEY_ARTIST 3
#define KEY_IS_PLAYING 4
#define KEY_VOLUME_VAL 5
#define KEY_IS_FIXED 6
#define KEY_ERROR 7
#define KEY_FONT_SIZE 8
#define KEY_SCROLL_TEXT 9
#define KEY_ENABLE_TOUCH 12
#define KEY_RESPECT_QUIET_TIME 16
#define KEY_THEME 17

#define PERSIST_KEY_FONT 0
#define PERSIST_KEY_SCROLL 1
#define PERSIST_KEY_TOUCH 4
#define PERSIST_KEY_QUIET_TIME 5
#define PERSIST_KEY_THEME 6

#define ENABLE_VOLUME 1

// Timing configurations
#define GLOW_FRAME_INTERVAL_MS 60
#define GESTURE_MODE_TIMEOUT_MS 5000
#define VOLUME_MODE_TIMEOUT_MS 5000

// Scale dynamically for Gabbro (260x260), Chalk (180x180), Emery (200x228), and Rect (144x168)
#define RECT_SCALE_Y(val) (((val) * bounds.size.h) / 168)
#define RECT_SCALE_H(val) (((val) * bounds.size.h) / 168)
#define ROUND_SCALE_Y(val) (((val) * bounds.size.h) / 180)
#define ROUND_SCALE_H(val) (((val) * bounds.size.h) / 180)

#define NATIVE_Y(rect_y, round_y) PBL_IF_ROUND_ELSE(ROUND_SCALE_Y(round_y), RECT_SCALE_Y(rect_y))
#define NATIVE_H(rect_h, round_h) PBL_IF_ROUND_ELSE(ROUND_SCALE_H(round_h), RECT_SCALE_H(rect_h))

typedef enum {
  MODE_TRACK,
  MODE_ZONE,
  MODE_ERROR
} AppMode;

// --- STATE VARIABLES ---
static AppMode s_mode = MODE_TRACK;
static Window *s_window;
static bool s_window_loaded = false;

static int s_font_size = 1;
static bool s_enable_scroll = false;
static bool s_enable_touch = true;
static bool s_respect_quiet_time = true;
static bool s_light_mode = false;

static TextLayer *s_time_layer = NULL;
static TextLayer *s_date_layer = NULL;
static BitmapLayer *s_logo_layer = NULL;
static GBitmap *s_logo_bitmap = NULL;
static TextLayer *s_track_layer = NULL;
static TextLayer *s_artist_layer = NULL;
static Layer *s_zone_layer = NULL;
static Layer *s_status_layer = NULL;
static Layer *s_glow_layer = NULL;

static GFont s_time_font = NULL;
static GFont s_zone_font = NULL;
static GPath *s_play_path = NULL;
static const GPoint s_large_play_points[] = {{-6, -12}, {-6, 12}, {12, 0}};
static const GPathInfo s_large_play_info = { .num_points = 3, .points = (GPoint *)s_large_play_points };
static const GPoint s_normal_play_points[] = {{-4, -8}, {-4, 8}, {8, 0}};
static const GPathInfo s_normal_play_info = { .num_points = 3, .points = (GPoint *)s_normal_play_points };
static const GPoint s_small_play_points[] = {{-3, -6}, {-3, 6}, {6, 0}};
static const GPathInfo s_small_play_info = { .num_points = 3, .points = (GPoint *)s_small_play_points };

// Synchronized Marquee Variables
static PropertyAnimation *s_marquee_track_anim = NULL;
static PropertyAnimation *s_marquee_artist_anim = NULL;
static Animation *s_marquee_spawn_anim = NULL;

#if ENABLE_VOLUME
static TextLayer *s_vol_layer = NULL;
static char s_vol_buf[32];
static int s_volume = -1;
static AppTimer *s_vol_flash_timer = NULL;
static bool s_is_flashing_vol = false;
static AppTimer *s_vol_ignore_timer = NULL;
static bool s_ignore_vol_updates = false;
#endif

static AppTimer *s_playpause_delay_timer = NULL;
static AppTimer *s_zone_revert_timer = NULL;
static AppTimer *s_touch_hold_timer = NULL;
static AppTimer *s_vol_ready_timer = NULL;
static AppTimer *s_gesture_mode_timer = NULL;
static AppTimer *s_play_ignore_timer = NULL;
static AppTimer *s_glow_pulse_timer = NULL;

static bool s_touch_held = false;
static bool s_volume_mode_ready = false;
static bool s_volume_mode_active = false;
static bool s_gesture_mode_active = false;
static bool s_ignore_play_updates = false;
static bool s_is_playing = false;
static bool s_is_fixed = false;
static bool s_app_in_focus = true;
static bool s_is_touching = false;

static int16_t s_touch_start_x = -1;
static int16_t s_touch_start_y = -1;
static int16_t s_touch_current_x = -1;
static int16_t s_touch_current_y = -1;
static int16_t s_last_vol_y = -1;
static uint8_t s_glow_step = 0;

static const GColor GLOW_PALETTE[][2] = {
  { GColorCobaltBlue,     GColorMidnightGreen },
  { GColorVividCerulean,  GColorCobaltBlue },
  { GColorPictonBlue,     GColorVividCerulean },
  { GColorCyan,           GColorPictonBlue },
  { GColorElectricBlue,   GColorCyan },
  { GColorWhite,          GColorElectricBlue },
  { GColorElectricBlue,   GColorCyan },
  { GColorCyan,           GColorPictonBlue },
  { GColorPictonBlue,     GColorVividCerulean },
  { GColorVividCerulean,  GColorCobaltBlue },
};
#define GLOW_PALETTE_STEPS (sizeof(GLOW_PALETTE) / sizeof(GLOW_PALETTE[0]))

static char s_time_buf[16] = "";
static char s_date_buf[64] = "";
static char s_track_buf[128] = "";
static char s_artist_buf[128] = "";
static char s_zone_buf[64] = "";

// --- FORWARD DECLARATIONS ---
static void update_ui(void);
static void end_gesture_mode_cb(void *data);
static void extend_gesture_mode(void);
static void accel_tap_handler(AccelAxisType axis, int32_t direction);
static void touch_handler(const TouchEvent *event, void *context);
#if ENABLE_VOLUME
static void lock_volume_updates(void);
#endif

// --- UTILITY ---
static int get_tuple_int(Tuple *t) {
  if (!t) return -1;
  switch (t->length) {
    case 1: return t->value->int8;
    case 2: return t->value->int16;
    case 4: return t->value->int32;
    default: return t->value->int32;
  }
}

static void focus_handler(bool in_focus) {
  s_app_in_focus = in_focus;
}

static void apply_theme() {
  if (!s_window_loaded) return;

  GColor bg_color = s_light_mode ? GColorWhite : GColorBlack;
  GColor fg_color = s_light_mode ? GColorBlack : GColorWhite;

  window_set_background_color(s_window, bg_color);
  text_layer_set_text_color(s_time_layer, fg_color);
  if (s_date_layer) text_layer_set_text_color(s_date_layer, fg_color);
  text_layer_set_text_color(s_track_layer, fg_color);
  text_layer_set_text_color(s_artist_layer, fg_color);

  if (s_logo_bitmap) {
    gbitmap_destroy(s_logo_bitmap);
    s_logo_bitmap = NULL;
  }

  s_logo_bitmap = gbitmap_create_with_resource(s_light_mode ? RESOURCE_ID_ROON_LOGO_TINY_LIGHT : RESOURCE_ID_ROON_LOGO_TINY_DARK);
  if (s_logo_layer && s_logo_bitmap) {
    bitmap_layer_set_bitmap(s_logo_layer, s_logo_bitmap);
  }

  #if ENABLE_VOLUME
  if (s_vol_layer) {
    text_layer_set_background_color(s_vol_layer, bg_color);
    text_layer_set_text_color(s_vol_layer, fg_color);
  }
  #endif

  if (s_zone_layer) layer_mark_dirty(s_zone_layer);
  if (s_status_layer) layer_mark_dirty(s_status_layer);
  if (s_glow_layer) layer_mark_dirty(s_glow_layer);
}

// --- HARDWARE SUBSCRIPTIONS ---
static void manage_hardware_subscriptions(void) {
  bool quiet_time_enforced = s_respect_quiet_time && quiet_time_is_active();
  bool should_enable = s_enable_touch && !quiet_time_enforced;

  if (should_enable) {
    // Only subscribe to the accelerometer at rest.
    accel_tap_service_subscribe(accel_tap_handler);
  } else {
    // Cut all hardware listeners immediately and drop the UI when quiet time is enforced.
    accel_tap_service_unsubscribe();
    if (s_gesture_mode_active) {
      end_gesture_mode_cb(NULL);
    }
  }
}

// --- BLUETOOTH CONNECTION HANDLER ---
static void bluetooth_callback(bool connected) {
  if (!connected) {
    if (s_mode != MODE_ERROR) {
      s_mode = MODE_ERROR;
      update_ui();
    }
  } else {
    if (s_window_loaded) {
      DictionaryIterator *iter;
      if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
        dict_write_cstring(iter, KEY_COMMAND, "status");
        app_message_outbox_send();
      }
    }
  }
}

// --- NATIVE OUTBOX DISPATCHER ---
static void send_command(char *cmd) {
  if (!s_window_loaded) return;

  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);

  if (result == APP_MSG_OK) {
    dict_write_cstring(iter, KEY_COMMAND, cmd);
    result = app_message_outbox_send();

    if (result != APP_MSG_OK) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox dispatch failed! Kernel Code: %d", (int)result);
    }
  }
}

#if ENABLE_VOLUME
static void vol_ignore_cb(void *data) {
  s_ignore_vol_updates = false;
  s_vol_ignore_timer = NULL;
}

static void lock_volume_updates(void) {
  s_ignore_vol_updates = true;
  if (s_vol_ignore_timer) app_timer_cancel(s_vol_ignore_timer);
  s_vol_ignore_timer = app_timer_register(1500, vol_ignore_cb, NULL);
}

static void vol_flash_cb(void *data) {
  s_is_flashing_vol = false;
  s_vol_flash_timer = NULL;

  end_gesture_mode_cb(NULL);
  update_ui();
}

static void flash_volume_ms(int ms) {
  s_is_flashing_vol = true;
  if (s_vol_flash_timer) {
    app_timer_cancel(s_vol_flash_timer);
    s_vol_flash_timer = NULL;
  }

  if (!s_is_touching) {
    s_vol_flash_timer = app_timer_register(ms, vol_flash_cb, NULL);
  }

  if (s_gesture_mode_active && !s_is_touching) {
    extend_gesture_mode();
  }

  update_ui();
}
#endif

static void safe_set_text(TextLayer *layer, char *text) {
  if (s_window_loaded && layer && text) text_layer_set_text(layer, text);
}

static void apply_fonts() {
  if (!s_track_layer || !s_artist_layer) return;

  if (s_play_path) {
    gpath_destroy(s_play_path);
    s_play_path = NULL;
  }

  if (s_font_size == 2) {
    text_layer_set_font(s_track_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
    s_zone_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    s_play_path = gpath_create(&s_large_play_info);
  } else if (s_font_size == 1) {
    text_layer_set_font(s_track_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
    s_zone_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    s_play_path = gpath_create(&s_normal_play_info);
  } else {
    text_layer_set_font(s_track_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    s_zone_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    s_play_path = gpath_create(&s_small_play_info);
  }

  if (s_zone_layer) layer_mark_dirty(s_zone_layer);
}

static void stop_marquee() {
  if (s_marquee_spawn_anim) {
    animation_unschedule(s_marquee_spawn_anim);
    animation_destroy(s_marquee_spawn_anim);
    s_marquee_spawn_anim = NULL;
  } else {
    if (s_marquee_track_anim) animation_unschedule(property_animation_get_animation(s_marquee_track_anim));
    if (s_marquee_artist_anim) animation_unschedule(property_animation_get_animation(s_marquee_artist_anim));
  }

  if (s_marquee_track_anim) { property_animation_destroy(s_marquee_track_anim); s_marquee_track_anim = NULL; }
  if (s_marquee_artist_anim) { property_animation_destroy(s_marquee_artist_anim); s_marquee_artist_anim = NULL; }

  if (s_track_layer && s_window_loaded) {
    Layer *root = window_get_root_layer(s_window);
    GRect bounds = layer_get_bounds(root);

    layer_set_frame(text_layer_get_layer(s_track_layer), GRect(0, (bounds.size.h / 2) + NATIVE_Y(12, 14), bounds.size.w, NATIVE_H(32, 32)));
    text_layer_set_text_alignment(s_track_layer, GTextAlignmentCenter);

    layer_set_frame(text_layer_get_layer(s_artist_layer), GRect(0, (bounds.size.h / 2) + NATIVE_Y(36, 40), bounds.size.w, NATIVE_H(28, 28)));
    text_layer_set_text_alignment(s_artist_layer, GTextAlignmentCenter);
  }
}

static void start_marquee() {
  stop_marquee();
  if (!s_window_loaded || !s_track_layer || !s_artist_layer) return;

  bool is_no_core = (strcmp(s_track_buf, "No Core") == 0);
  // Force marquee for "No Core" prompt even if scroll is disabled in settings
  if (!is_no_core) {
    if (!s_enable_scroll || !s_is_playing) return;
  }

  const char* track_text = text_layer_get_text(s_track_layer);
  const char* artist_text = text_layer_get_text(s_artist_layer);

  if (!track_text || strlen(track_text) == 0) return;

  GFont font_track;
  GFont font_artist;
  if (s_font_size == 2) {
    font_track = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    font_artist = fonts_get_system_font(FONT_KEY_GOTHIC_28);
  } else if (s_font_size == 1) {
    font_track = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    font_artist = fonts_get_system_font(FONT_KEY_GOTHIC_24);
  } else {
    font_track = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    font_artist = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  }

  Layer *root = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root);

  GSize track_size = graphics_text_layout_get_content_size(track_text, font_track, GRect(0, 0, 2000, 60), GTextOverflowModeWordWrap, GTextAlignmentLeft);
  GSize artist_size = graphics_text_layout_get_content_size(artist_text, font_artist, GRect(0, 0, 2000, 60), GTextOverflowModeWordWrap, GTextAlignmentLeft);

  bool scroll_track = (track_size.w > bounds.size.w);
  bool scroll_artist = (artist_size.w > bounds.size.w);

  if (!scroll_track && !scroll_artist) return;

  int max_w = track_size.w > artist_size.w ? track_size.w : artist_size.w;

  GRect start_track = GRect(bounds.size.w, (bounds.size.h / 2) + NATIVE_Y(12, 14), max_w + 20, NATIVE_H(32, 32));
  GRect end_track = GRect(-max_w - 20, (bounds.size.h / 2) + NATIVE_Y(12, 14), max_w + 20, NATIVE_H(32, 32));

  GRect start_artist = GRect(bounds.size.w, (bounds.size.h / 2) + NATIVE_Y(36, 40), max_w + 20, NATIVE_H(28, 28));
  GRect end_artist = GRect(-max_w - 20, (bounds.size.h / 2) + NATIVE_Y(36, 40), max_w + 20, NATIVE_H(28, 28));

  Animation *anim_t = NULL;
  Animation *anim_a = NULL;

  if (scroll_track) {
    text_layer_set_text_alignment(s_track_layer, GTextAlignmentLeft);
    s_marquee_track_anim = property_animation_create_layer_frame(text_layer_get_layer(s_track_layer), &start_track, &end_track);
    anim_t = property_animation_get_animation(s_marquee_track_anim);
  }
  if (scroll_artist) {
    text_layer_set_text_alignment(s_artist_layer, GTextAlignmentLeft);
    s_marquee_artist_anim = property_animation_create_layer_frame(text_layer_get_layer(s_artist_layer), &start_artist, &end_artist);
    anim_a = property_animation_get_animation(s_marquee_artist_anim);
  }

  int duration = (bounds.size.w + max_w + 40) * 20;

  if (anim_t && anim_a) {
    animation_set_duration(anim_t, duration);
    animation_set_duration(anim_a, duration);
    s_marquee_spawn_anim = animation_spawn_create(anim_t, anim_a, NULL);
    animation_set_curve(s_marquee_spawn_anim, AnimationCurveLinear);
    animation_set_play_count(s_marquee_spawn_anim, ANIMATION_PLAY_COUNT_INFINITE);
    animation_schedule(s_marquee_spawn_anim);
  } else if (anim_t) {
    animation_set_duration(anim_t, duration);
    animation_set_curve(anim_t, AnimationCurveLinear);
    animation_set_play_count(anim_t, ANIMATION_PLAY_COUNT_INFINITE);
    animation_schedule(anim_t);
  } else if (anim_a) {
    animation_set_duration(anim_a, duration);
    animation_set_curve(anim_a, AnimationCurveLinear);
    animation_set_play_count(anim_a, ANIMATION_PLAY_COUNT_INFINITE);
    animation_schedule(anim_a);
  }
}

static void update_ui() {
  if (!s_window_loaded) return;

  Layer *root = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root);

  bool is_error_resting = (s_mode == MODE_ERROR && !s_gesture_mode_active);
  bool is_quiet = (s_respect_quiet_time && quiet_time_is_active());
  bool hide_music = is_error_resting || is_quiet;

  if (hide_music) {
    stop_marquee();

    layer_set_hidden(text_layer_get_layer(s_track_layer), true);
    layer_set_hidden(text_layer_get_layer(s_artist_layer), true);
    layer_set_hidden(s_zone_layer, true);

    if (s_status_layer) layer_set_hidden(s_status_layer, true);
    layer_set_hidden(text_layer_get_layer(s_date_layer), false);

    int time_h = NATIVE_H(42, 42);
    int date_h = (bounds.size.w < 180) ? 36 : 18;
    int total_h = time_h + date_h;
    int offset_y = (bounds.size.h - total_h) / 2;

    layer_set_frame(bitmap_layer_get_layer(s_logo_layer), GRect(0, NATIVE_Y(4, 10), bounds.size.w, NATIVE_H(20, 20)));
    layer_set_frame(text_layer_get_layer(s_time_layer), GRect(0, offset_y - 6, bounds.size.w, time_h));
    layer_set_frame(text_layer_get_layer(s_date_layer), GRect(0, offset_y + time_h - 10, bounds.size.w, date_h));
  } else {
    layer_set_hidden(text_layer_get_layer(s_date_layer), true);

    layer_set_hidden(text_layer_get_layer(s_track_layer), false);
    layer_set_hidden(text_layer_get_layer(s_artist_layer), false);
    layer_set_hidden(s_zone_layer, (s_mode == MODE_ERROR));
    if (s_status_layer) {
      layer_set_hidden(s_status_layer, false);
      layer_mark_dirty(s_status_layer);
    }

    layer_set_frame(bitmap_layer_get_layer(s_logo_layer), GRect(0, NATIVE_Y(4, 10), bounds.size.w, NATIVE_H(20, 20)));
    layer_set_frame(text_layer_get_layer(s_time_layer), GRect(0, NATIVE_Y(24, 30), bounds.size.w, NATIVE_H(42, 42)));
  }

  if (s_mode == MODE_ERROR) {
    safe_set_text(s_track_layer, "Bridge Not Found");
    safe_set_text(s_artist_layer, "Tap to retry");
  } else {
    safe_set_text(s_track_layer, s_track_buf);
    if (strcmp(s_track_buf, "No Core") == 0) {
      safe_set_text(s_artist_layer, "Is the extension enabled?");
    } else {
      safe_set_text(s_artist_layer, s_artist_buf);
    }
  }

  layer_mark_dirty(s_zone_layer);

  #if ENABLE_VOLUME
  if (s_vol_layer) {
    if (s_is_flashing_vol) {
      if (s_is_fixed && s_volume == -1) {
        snprintf(s_vol_buf, sizeof(s_vol_buf), "Fixed Vol");
      } else if (s_volume == -1) {
        snprintf(s_vol_buf, sizeof(s_vol_buf), "Vol: --");
      } else {
        snprintf(s_vol_buf, sizeof(s_vol_buf), "Vol: %d", s_volume);
      }

      text_layer_set_text(s_vol_layer, s_vol_buf);
      layer_set_hidden(text_layer_get_layer(s_vol_layer), false);
    } else {
      layer_set_hidden(text_layer_get_layer(s_vol_layer), true);
    }
  }
  #endif
}

static void zone_revert_callback(void *data) {
  s_zone_revert_timer = NULL;
  if (s_mode == MODE_ZONE) { s_mode = MODE_TRACK; update_ui(); }
}

static void reset_zone_timer() {
  if (s_zone_revert_timer) app_timer_cancel(s_zone_revert_timer);
  s_zone_revert_timer = app_timer_register(8000, zone_revert_callback, NULL);
}

static void cancel_zone_timer() {
  if (s_zone_revert_timer) { app_timer_cancel(s_zone_revert_timer); s_zone_revert_timer = NULL; }
}

// --- OPTIMISTIC PLAY/PAUSE UI TRIGGER ---
static void play_ignore_cb(void *data) {
  s_ignore_play_updates = false;
  s_play_ignore_timer = NULL;
}

static void lock_play_updates() {
  s_ignore_play_updates = true;
  if (s_play_ignore_timer) app_timer_cancel(s_play_ignore_timer);
  s_play_ignore_timer = app_timer_register(2000, play_ignore_cb, NULL);
}

static void send_playpause_cb(void *data) {
  s_playpause_delay_timer = NULL;
  send_command("playpause");
}

static void trigger_optimistic_playpause() {
  s_is_playing = !s_is_playing;
  lock_play_updates();
  if (s_status_layer) layer_mark_dirty(s_status_layer);
  vibes_short_pulse();
  if (s_playpause_delay_timer) app_timer_cancel(s_playpause_delay_timer);
  s_playpause_delay_timer = app_timer_register(100, send_playpause_cb, NULL);
}

// --- VISUAL GLOW AND TOUCH FEEDBACK ---
static void glow_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_gesture_mode_active && !s_is_touching) return;

  GRect bounds = layer_get_bounds(layer);
  GColor fallback_color = s_light_mode ? GColorBlack : GColorWhite;
  (void)fallback_color; // Explicitly suppress unused variable warning on color platforms

  GColor outer_color = COLOR_FALLBACK(GLOW_PALETTE[s_glow_step][0], fallback_color);
  GColor inner_color = COLOR_FALLBACK(GLOW_PALETTE[s_glow_step][1], fallback_color);

  graphics_context_set_stroke_width(ctx, 1);

  #if PBL_ROUND
  GPoint center = grect_center_point(&bounds);
  uint16_t radius = (bounds.size.w / 2) - 3;
  graphics_context_set_stroke_color(ctx, outer_color);
  graphics_draw_circle(ctx, center, radius);
  graphics_context_set_stroke_color(ctx, inner_color);
  graphics_draw_circle(ctx, center, radius - 1);
  #else
  GRect glow_bounds = grect_inset(bounds, GEdgeInsets(2));
  graphics_context_set_stroke_color(ctx, outer_color);
  graphics_draw_round_rect(ctx, glow_bounds, 4);

  GRect inner_bounds = grect_inset(glow_bounds, GEdgeInsets(1));
  graphics_context_set_stroke_color(ctx, inner_color);
  graphics_draw_round_rect(ctx, inner_bounds, 3);
  #endif
}

static void glow_pulse_timer_cb(void *data) {
  if (!s_gesture_mode_active) {
    s_glow_pulse_timer = NULL;
    return;
  }
  s_glow_step = (s_glow_step + 1) % GLOW_PALETTE_STEPS;
  layer_mark_dirty(s_glow_layer);
  s_glow_pulse_timer = app_timer_register(GLOW_FRAME_INTERVAL_MS, glow_pulse_timer_cb, NULL);
}

// --- TOUCH ENGINE DISPATCHER & GESTURE WAKEUP ---

static void end_gesture_mode_cb(void *data) {
  s_gesture_mode_timer = NULL;
  s_gesture_mode_active = false;
  s_is_touching = false;

  if (touch_service_is_enabled()) {
    touch_service_unsubscribe();
  }

  layer_set_hidden(s_glow_layer, true);
  if (s_glow_pulse_timer) {
    app_timer_cancel(s_glow_pulse_timer);
    s_glow_pulse_timer = NULL;
  }

  update_ui();
}

static void extend_gesture_mode(void) {
  if (!s_is_touching) {
    if (s_gesture_mode_timer) {
      app_timer_reschedule(s_gesture_mode_timer, GESTURE_MODE_TIMEOUT_MS);
    } else {
      s_gesture_mode_timer = app_timer_register(GESTURE_MODE_TIMEOUT_MS, end_gesture_mode_cb, NULL);
    }
  }
}

static void enter_gesture_mode(void) {
  if (!s_gesture_mode_active) {
    light_enable_interaction();

    s_gesture_mode_active = true;
    vibes_short_pulse();

    if (s_enable_touch && touch_service_is_enabled()) {
      touch_service_subscribe(touch_handler, NULL);
    }

    if (s_glow_layer) {
      layer_set_hidden(s_glow_layer, false);
      layer_mark_dirty(s_glow_layer);
    }

    if (!s_glow_pulse_timer) {
      s_glow_pulse_timer = app_timer_register(GLOW_FRAME_INTERVAL_MS, glow_pulse_timer_cb, NULL);
    }
    update_ui();
  }
  extend_gesture_mode();
}

static void touch_hold_cb(void *data) {
  s_touch_hold_timer = NULL;
  s_touch_held = true;

  vibes_long_pulse();
  stop_marquee();
  safe_set_text(s_track_layer, "Pausing All...");
  safe_set_text(s_artist_layer, "");

  s_track_buf[0] = '\0';
  s_artist_buf[0] = '\0';

  send_command("pause_all");
}

static void vol_drag_ready_cb(void *data) {
  s_vol_ready_timer = NULL;
  s_volume_mode_ready = true;
}

static void touch_handler(const TouchEvent *event, void *context) {
  if (s_mode == MODE_ERROR || !s_app_in_focus) return;

  if (!s_gesture_mode_active) return;

  Layer *root_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(root_layer);

  if (event->type == TouchEvent_Touchdown) {
    light_enable_interaction();

    s_is_touching = true;
    s_touch_start_x = event->x;
    s_touch_start_y = event->y;
    s_touch_current_x = event->x;
    s_touch_current_y = event->y;
    s_last_vol_y = event->y;

    s_touch_held = false;
    s_volume_mode_ready = false;
    s_volume_mode_active = false;

    if (s_gesture_mode_timer) { app_timer_cancel(s_gesture_mode_timer); s_gesture_mode_timer = NULL; }
    if (s_vol_flash_timer) { app_timer_cancel(s_vol_flash_timer); s_vol_flash_timer = NULL; }

    if (s_touch_hold_timer) app_timer_cancel(s_touch_hold_timer);
    s_touch_hold_timer = app_timer_register(1500, touch_hold_cb, NULL);

    if (s_vol_ready_timer) app_timer_cancel(s_vol_ready_timer);
    s_vol_ready_timer = app_timer_register(400, vol_drag_ready_cb, NULL);

  } else if (event->type == TouchEvent_PositionUpdate) {
    s_is_touching = true;
    s_touch_current_x = event->x;
    s_touch_current_y = event->y;

    int16_t raw_dx = s_touch_current_x - s_touch_start_x;
    int16_t raw_dy = s_touch_current_y - s_touch_start_y;
    int16_t dx = abs(raw_dx);
    int16_t dy = abs(raw_dy);

    if (s_volume_mode_ready && dy > 15 && dy > dx * 2) {
      if (!s_volume_mode_active) {
        s_volume_mode_active = true;
        if (s_touch_hold_timer) { app_timer_cancel(s_touch_hold_timer); s_touch_hold_timer = NULL; }
        vibes_double_pulse();
      }

      int16_t step_dy = s_touch_current_y - s_last_vol_y;
      if (step_dy <= -10 || step_dy >= 10) {
        #if ENABLE_VOLUME
        if (!s_is_fixed) {
          if (step_dy <= -10) {
            if (s_volume != -1) { s_volume += 2; if (s_volume > 100) s_volume = 100; }
            send_command("vol_up");
            lock_volume_updates();
          } else {
            if (s_volume != -1) { s_volume -= 2; if (s_volume < 0) s_volume = 0; }
            send_command("vol_down");
            lock_volume_updates();
          }
          flash_volume_ms(3000);
        } else {
          flash_volume_ms(1000);
        }
        #endif
        s_last_vol_y = s_touch_current_y;
      }
    } else if (dx > 15 || dy > 15) {
      if (s_touch_hold_timer) { app_timer_cancel(s_touch_hold_timer); s_touch_hold_timer = NULL; }
      if (s_vol_ready_timer) { app_timer_cancel(s_vol_ready_timer); s_vol_ready_timer = NULL; }
    }

  } else if (event->type == TouchEvent_Liftoff) {
    s_is_touching = false;

    if (s_touch_hold_timer) { app_timer_cancel(s_touch_hold_timer); s_touch_hold_timer = NULL; }
    if (s_vol_ready_timer) { app_timer_cancel(s_vol_ready_timer); s_vol_ready_timer = NULL; }

    if (s_touch_held || s_volume_mode_active) {
      s_volume_mode_active = false;
      s_touch_start_x = -1;
      s_touch_start_y = -1;

      if (s_gesture_mode_active) {
        extend_gesture_mode();
        if (s_is_flashing_vol && !s_vol_flash_timer) {
          s_vol_flash_timer = app_timer_register(VOLUME_MODE_TIMEOUT_MS, vol_flash_cb, NULL);
        }
      }
      return;
    }

    if (s_touch_start_x != -1) {
      int16_t delta_x = s_touch_current_x - s_touch_start_x;
      int16_t delta_y = s_touch_current_y - s_touch_start_y;
      int16_t abs_dx = abs(delta_x);
      int16_t abs_dy = abs(delta_y);

      if (s_is_flashing_vol) {
        if (!s_is_fixed && abs_dy > 20 && abs_dy > abs_dx) {
          vibes_short_pulse();
          if (delta_y < 0) {
            if (s_volume != -1) { s_volume += 2; if (s_volume > 100) s_volume = 100; }
            send_command("vol_up");
            lock_volume_updates();
          } else {
            if (s_volume != -1) { s_volume -= 2; if (s_volume < 0) s_volume = 0; }
            send_command("vol_down");
            lock_volume_updates();
          }
          flash_volume_ms(3000);
        }
      }
      else if (abs_dx > 30 && abs_dx > abs_dy) {
        if (s_mode == MODE_TRACK) {
          vibes_short_pulse();
          send_command(delta_x > 0 ? "previous" : "next");
        }
      }
      else if (abs_dy > 20 && abs_dy > abs_dx) {
        vibes_short_pulse();
        reset_zone_timer();
        stop_marquee();
        send_command(delta_y > 0 ? "prev_zone" : "next_zone");
      }
      else if (abs_dx < 10 && abs_dy < 10) {
        if (s_touch_current_y < NATIVE_Y(50, 60)) {
          vibes_double_pulse();
          end_gesture_mode_cb(NULL);
        } else {
          if (s_mode == MODE_TRACK) {
            trigger_optimistic_playpause();
          } else if (s_mode == MODE_ZONE) {
            reset_zone_timer();
          }
        }
      }
    }

    s_touch_start_x = -1;
    s_touch_start_y = -1;

    if (s_gesture_mode_active) {
      extend_gesture_mode();
      if (s_is_flashing_vol && !s_vol_flash_timer) {
        s_vol_flash_timer = app_timer_register(VOLUME_MODE_TIMEOUT_MS, vol_flash_cb, NULL);
      }
    }
  }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_mode == MODE_ERROR || !s_app_in_focus) return;

  if (axis == ACCEL_AXIS_Z) {
    if (!s_gesture_mode_active) {
      enter_gesture_mode();
    } else {
      vibes_double_pulse();
      end_gesture_mode_cb(NULL);
    }
  }
}

static void zone_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_window_loaded || s_mode == MODE_ERROR) return;

  GRect bounds = layer_get_bounds(layer);

  if (!s_zone_font) return;
  if (strlen(s_zone_buf) == 0) return;

  GSize text_size = graphics_text_layout_get_content_size(s_zone_buf, s_zone_font, GRect(0, 0, bounds.size.w - 12, bounds.size.h), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);

  int padding_x = 6;
  int padding_y_total = 4;

  int box_width = text_size.w + (padding_x * 2);
  if (box_width > bounds.size.w) box_width = bounds.size.w;

  int box_height = text_size.h + padding_y_total;
  if (box_height > bounds.size.h) box_height = bounds.size.h;

  int box_x = (bounds.size.w - box_width) / 2;
  int box_y = (bounds.size.h - box_height) / 2;

  GRect box_rect = GRect(box_x, box_y, box_width, box_height);
  GRect text_rect = GRect(box_x + padding_x, box_y - 2, text_size.w, box_height + 4);

  GColor bg_color = s_light_mode ? GColorWhite : GColorBlack;
  GColor fg_color = s_light_mode ? GColorBlack : GColorWhite;

  if (s_mode == MODE_ZONE) {
    graphics_context_set_fill_color(ctx, fg_color);
    graphics_fill_rect(ctx, box_rect, 3, GCornersAll);
    graphics_context_set_text_color(ctx, bg_color);
  } else {
    graphics_context_set_stroke_color(ctx, fg_color);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_round_rect(ctx, box_rect, 3);
    graphics_context_set_text_color(ctx, fg_color);
  }

  graphics_draw_text(ctx, s_zone_buf, s_zone_font, text_rect, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void status_layer_update_proc(Layer *layer, GContext *ctx) {
  if (!s_window_loaded || s_mode == MODE_ERROR) return;
  GRect bounds = layer_get_bounds(layer);

  GColor fg_color = s_light_mode ? GColorBlack : GColorWhite;

  if (s_mode == MODE_TRACK) {
    graphics_context_set_fill_color(ctx, fg_color);
    if (s_is_playing) {
      if (s_font_size == 2) {
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 - 8, bounds.size.h/2 - 12, 6, 24), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 + 2, bounds.size.h/2 - 12, 6, 24), 0, GCornerNone);
      } else if (s_font_size == 1) {
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 - 6, bounds.size.h/2 - 8, 4, 16), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 + 2, bounds.size.h/2 - 8, 4, 16), 0, GCornerNone);
      } else {
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 - 4, bounds.size.h/2 - 6, 3, 12), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(bounds.size.w/2 + 1, bounds.size.h/2 - 6, 3, 12), 0, GCornerNone);
      }
    } else {
      if (s_play_path) {
        gpath_move_to(s_play_path, GPoint(bounds.size.w/2, bounds.size.h/2));
        gpath_draw_filled(ctx, s_play_path);
      }
    }
  } else {
    graphics_context_set_text_color(ctx, fg_color);
    const char* mode_text = "";

    if (s_mode == MODE_ZONE) mode_text = "Select Zone";

    graphics_draw_text(ctx, mode_text, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(0, -2, bounds.size.w, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

// --- LOGGING & MESSAGE HANDLERS ---
static void outbox_sent_handler(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "AppMessage cleared link layer successfully.");
}

static void outbox_failed_handler(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Upstream AppMessage transmission dropped. Reason code: %d", (int)reason);
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  if (!s_window_loaded) return;
  Tuple *t;

  if ((t = dict_find(iterator, KEY_FONT_SIZE))) {
    int requested_size = get_tuple_int(t);
    if (s_font_size != requested_size) {
      s_font_size = requested_size;
      persist_write_int(PERSIST_KEY_FONT, s_font_size);
      apply_fonts();
      start_marquee();
      update_ui();
    }
  }

  if ((t = dict_find(iterator, KEY_SCROLL_TEXT))) {
    bool requested_scroll = (get_tuple_int(t) == 1);
    if (s_enable_scroll != requested_scroll) {
      s_enable_scroll = requested_scroll;
      persist_write_bool(PERSIST_KEY_SCROLL, s_enable_scroll);
      if (s_enable_scroll) {
        start_marquee();
      } else {
        stop_marquee();
      }
    }
  }

  if ((t = dict_find(iterator, KEY_ENABLE_TOUCH))) {
    bool requested_touch = (get_tuple_int(t) == 1);
    if (s_enable_touch != requested_touch) {
      s_enable_touch = requested_touch;
      persist_write_bool(PERSIST_KEY_TOUCH, s_enable_touch);
      manage_hardware_subscriptions();
    }
  }

  if ((t = dict_find(iterator, KEY_RESPECT_QUIET_TIME))) {
    bool requested_respect = (get_tuple_int(t) == 1);
    if (s_respect_quiet_time != requested_respect) {
      s_respect_quiet_time = requested_respect;
      persist_write_bool(PERSIST_KEY_QUIET_TIME, s_respect_quiet_time);
      manage_hardware_subscriptions();
      update_ui();
    }
  }

  if ((t = dict_find(iterator, KEY_THEME))) {
    bool requested_theme = (get_tuple_int(t) == 1);
    if (s_light_mode != requested_theme) {
      s_light_mode = requested_theme;
      persist_write_bool(PERSIST_KEY_THEME, s_light_mode);
      apply_theme();
    }
  }

  if ((t = dict_find(iterator, KEY_ERROR))) {
    if (get_tuple_int(t) == 1) {
      if (s_mode != MODE_ERROR) {
        s_mode = MODE_ERROR;
        update_ui();
      }
      return;
    } else {
      if (s_mode == MODE_ERROR) {
        s_mode = MODE_TRACK;
        update_ui();
      }
    }
  }

  if (s_mode == MODE_ERROR) return;

  if ((t = dict_find(iterator, KEY_ZONE_NAME))) {
    snprintf(s_zone_buf, sizeof(s_zone_buf), "%s", t->value->cstring);
    if (s_zone_layer) layer_mark_dirty(s_zone_layer);
  }

  if ((t = dict_find(iterator, KEY_TRACK))) {
    if (strcmp(s_track_buf, t->value->cstring) != 0) {
      snprintf(s_track_buf, sizeof(s_track_buf), "%s", t->value->cstring);
      safe_set_text(s_track_layer, s_track_buf);
      start_marquee();

      if (strcmp(s_track_buf, "No Core") == 0) {
        safe_set_text(s_artist_layer, "Is the extension enabled?");
      } else {
        safe_set_text(s_artist_layer, s_artist_buf);
      }
    }
  }

  if ((t = dict_find(iterator, KEY_ARTIST))) {
    if (strcmp(s_artist_buf, t->value->cstring) != 0) {
      snprintf(s_artist_buf, sizeof(s_artist_buf), "%s", t->value->cstring);
      if (strcmp(s_track_buf, "No Core") == 0) {
        safe_set_text(s_artist_layer, "Is the extension enabled?");
      } else {
        safe_set_text(s_artist_layer, s_artist_buf);
      }
    }
  }

  if ((t = dict_find(iterator, KEY_IS_PLAYING))) {
    if (!s_ignore_play_updates) {
      bool is_playing = (get_tuple_int(t) == 1);
      if (s_is_playing != is_playing) {
        s_is_playing = is_playing;
        if (s_status_layer) layer_mark_dirty(s_status_layer);

        // Instantly halt infinite text animation loops if playback is paused
        if (s_is_playing) {
          start_marquee();
        } else {
          stop_marquee();
          safe_set_text(s_track_layer, s_track_buf);
          safe_set_text(s_artist_layer, s_artist_buf);
        }
      }
    }
  }

  #if ENABLE_VOLUME
  if ((t = dict_find(iterator, KEY_VOLUME_VAL))) {
    int received_vol = get_tuple_int(t);
    if (!s_ignore_vol_updates) {
      s_volume = received_vol;
      if (s_is_flashing_vol) update_ui();
    }
  }
  #endif

  if ((t = dict_find(iterator, KEY_IS_FIXED))) s_is_fixed = (get_tuple_int(t) == 1);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (clock_is_24h_style()) {
    strftime(s_time_buf, sizeof(s_time_buf), "%H:%M", tick_time);
  } else {
    char temp_buf[8];
    strftime(temp_buf, sizeof(temp_buf), "%I:%M", tick_time);
    int start_idx = (temp_buf[0] == '0') ? 1 : 0;
    snprintf(s_time_buf, sizeof(s_time_buf), "%s %s", &temp_buf[start_idx], tick_time->tm_hour < 12 ? "am" : "pm");
  }

  if (s_time_layer) {
    text_layer_set_text(s_time_layer, s_time_buf);
  }

  strftime(s_date_buf, sizeof(s_date_buf), "%a, %b %d", tick_time);
  if (s_date_layer) {
    text_layer_set_text(s_date_layer, s_date_buf);
  }

  manage_hardware_subscriptions();
  update_ui();
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_logo_layer = bitmap_layer_create(GRect(0, NATIVE_Y(4, 10), bounds.size.w, NATIVE_H(20, 20)));
  bitmap_layer_set_background_color(s_logo_layer, GColorClear);
  bitmap_layer_set_compositing_mode(s_logo_layer, GCompOpSet);
  bitmap_layer_set_alignment(s_logo_layer, GAlignCenter);
  layer_add_child(root, bitmap_layer_get_layer(s_logo_layer));

  s_time_layer = text_layer_create(GRect(0, NATIVE_Y(24, 30), bounds.size.w, NATIVE_H(42, 42)));
  text_layer_set_background_color(s_time_layer, GColorClear);
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_COMFORTAA_BOLD_42));
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  s_date_layer = text_layer_create(GRect(0, 0, bounds.size.w, 18));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_set_hidden(text_layer_get_layer(s_date_layer), true);
  layer_add_child(root, text_layer_get_layer(s_date_layer));

  s_status_layer = layer_create(GRect(0, (bounds.size.h / 2) - 16, bounds.size.w, 32));
  layer_set_update_proc(s_status_layer, status_layer_update_proc);
  layer_add_child(root, s_status_layer);

  s_track_layer = text_layer_create(GRect(0, (bounds.size.h / 2) + NATIVE_Y(12, 14), bounds.size.w, NATIVE_H(32, 32)));
  text_layer_set_text(s_track_layer, "Loading...");
  text_layer_set_text_alignment(s_track_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_track_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_background_color(s_track_layer, GColorClear);
  layer_add_child(root, text_layer_get_layer(s_track_layer));

  s_artist_layer = text_layer_create(GRect(0, (bounds.size.h / 2) + NATIVE_Y(36, 40), bounds.size.w, NATIVE_H(28, 28)));
  text_layer_set_text_alignment(s_artist_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_artist_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_background_color(s_artist_layer, GColorClear);
  layer_add_child(root, text_layer_get_layer(s_artist_layer));

  s_zone_layer = layer_create(GRect(0, bounds.size.h - NATIVE_H(26, 32), bounds.size.w, NATIVE_H(26, 26)));
  layer_set_update_proc(s_zone_layer, zone_layer_update_proc);
  layer_add_child(root, s_zone_layer);

  s_glow_layer = layer_create(bounds);
  layer_set_update_proc(s_glow_layer, glow_layer_update_proc);
  layer_set_hidden(s_glow_layer, true);
  layer_add_child(root, s_glow_layer);

  #if ENABLE_VOLUME
  s_vol_layer = text_layer_create(GRect(0, NATIVE_Y(45, 50), bounds.size.w, NATIVE_H(80, 80)));
  text_layer_set_text(s_vol_layer, "Vol: --");
  text_layer_set_font(s_vol_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_vol_layer, GTextAlignmentCenter);
  layer_set_hidden(text_layer_get_layer(s_vol_layer), true);
  layer_add_child(root, text_layer_get_layer(s_vol_layer));
  #endif

  apply_fonts();
  s_window_loaded = true;
  apply_theme();

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  tick_handler(t, MINUTE_UNIT);
}

static void window_unload(Window *window) {
  s_window_loaded = false;

  if (s_play_path) { gpath_destroy(s_play_path); s_play_path = NULL; }
  if (s_playpause_delay_timer) app_timer_cancel(s_playpause_delay_timer);
  if (s_play_ignore_timer) app_timer_cancel(s_play_ignore_timer);
  if (s_touch_hold_timer) app_timer_cancel(s_touch_hold_timer);
  if (s_vol_ready_timer) app_timer_cancel(s_vol_ready_timer);
  if (s_gesture_mode_timer) app_timer_cancel(s_gesture_mode_timer);
  if (s_glow_pulse_timer) app_timer_cancel(s_glow_pulse_timer);

  stop_marquee();
  cancel_zone_timer();

  #if ENABLE_VOLUME
  if (s_vol_flash_timer) app_timer_cancel(s_vol_flash_timer);
  if (s_vol_ignore_timer) app_timer_cancel(s_vol_ignore_timer);
  text_layer_destroy(s_vol_layer);
  s_vol_layer = NULL;
  #endif

  layer_destroy(s_glow_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_track_layer);
  text_layer_destroy(s_artist_layer);
  layer_destroy(s_zone_layer);
  layer_destroy(s_status_layer);
  bitmap_layer_destroy(s_logo_layer);

  if (s_time_font) {
    fonts_unload_custom_font(s_time_font);
  }
  if (s_logo_bitmap) {
    gbitmap_destroy(s_logo_bitmap);
  }
}

static void init(void) {
  if (persist_exists(PERSIST_KEY_FONT)) s_font_size = persist_read_int(PERSIST_KEY_FONT);
  if (persist_exists(PERSIST_KEY_SCROLL)) s_enable_scroll = persist_read_bool(PERSIST_KEY_SCROLL);
  if (persist_exists(PERSIST_KEY_TOUCH)) s_enable_touch = persist_read_bool(PERSIST_KEY_TOUCH);
  if (persist_exists(PERSIST_KEY_QUIET_TIME)) s_respect_quiet_time = persist_read_bool(PERSIST_KEY_QUIET_TIME);
  if (persist_exists(PERSIST_KEY_THEME)) s_light_mode = persist_read_bool(PERSIST_KEY_THEME);

  s_window = window_create();

  app_focus_service_subscribe_handlers((AppFocusHandlers){
    .will_focus = NULL,
    .did_focus = focus_handler
  });

  manage_hardware_subscriptions();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  connection_service_subscribe((ConnectionHandlers) { .pebble_app_connection_handler = bluetooth_callback });

  window_set_window_handlers(s_window, (WindowHandlers) { .load = window_load, .unload = window_unload });

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_handler);
  app_message_register_outbox_sent(outbox_sent_handler);
  app_message_register_outbox_failed(outbox_failed_handler);

  app_message_open(1024, 256);

  window_stack_push(s_window, true);
}

static void deinit(void) {
  accel_tap_service_unsubscribe();
  if (touch_service_is_enabled()) {
    touch_service_unsubscribe();
  }

  app_focus_service_unsubscribe();
  tick_timer_service_unsubscribe();

  connection_service_unsubscribe();
  window_destroy(s_window);
  app_message_deregister_callbacks();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
