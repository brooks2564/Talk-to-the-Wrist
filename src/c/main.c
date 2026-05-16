#include <pebble.h>

static Window *s_window;
static Layer *s_canvas;
static int s_pressed = -1;
static AppTimer *s_flash_timer = NULL;

static const uint32_t s_resource_ids[] = {
    RESOURCE_ID_PCM_DATA_YES,
    RESOURCE_ID_PCM_DATA_NO,
    RESOURCE_ID_PCM_DATA_SHUTUP,
    RESOURCE_ID_PCM_DATA_GETOUTTAHERE
};

static const GColor s_bg_normal[] = {
    { .argb = GColorBrightGreenARGB8 },
    { .argb = GColorRedARGB8 },
    { .argb = GColorChromeYellowARGB8 },
    { .argb = GColorVividVioletARGB8 }
};
static const GColor s_fg_normal[] = {
    { .argb = GColorBlackARGB8 },
    { .argb = GColorWhiteARGB8 },
    { .argb = GColorBlackARGB8 },
    { .argb = GColorWhiteARGB8 }
};
static const char *s_labels[] = { "YES", "NO", "SHUT UP", "GET OUTTA\nHERE" };

static GFont s_font_big;
static GFont s_font_small;

// ── Audio ─────────────────────────────────────────────────────────────────────

static void play_sound(int btn) {
    if (speaker_get_status() != SpeakerStatusIdle) {
        speaker_stop();
    }
    ResHandle h = resource_get_handle(s_resource_ids[btn]);
    size_t size = resource_size(h);
    uint8_t *buf = malloc(size);
    if (!buf) return;
    resource_load(h, buf, size);
    if (speaker_stream_open(SpeakerPcmFormat_8kHz_8bit, 85)) {
        const uint8_t *ptr = buf;
        size_t remaining = size;
        while (remaining > 0) {
            uint32_t w = speaker_stream_write(ptr, remaining);
            if (w == 0) break;
            ptr += w;
            remaining -= w;
        }
        speaker_stream_close();
    }
    free(buf);
}

// ── Flash timer ───────────────────────────────────────────────────────────────

static void flash_timer_cb(void *data) {
    s_flash_timer = NULL;
    s_pressed = -1;
    layer_mark_dirty(s_canvas);
}

// ── Touch ─────────────────────────────────────────────────────────────────────

static void touch_handler(const TouchEvent *event, void *context) {
    if (event->type == TouchEvent_Touchdown) {
        int col = (event->x >= 100) ? 1 : 0;
        int row = (event->y >= 114) ? 1 : 0;
        int btn = row * 2 + col;
        s_pressed = btn;
        layer_mark_dirty(s_canvas);
        play_sound(btn);
        if (s_flash_timer) app_timer_cancel(s_flash_timer);
        s_flash_timer = app_timer_register(250, flash_timer_cb, NULL);
    }
}

// ── Icons ─────────────────────────────────────────────────────────────────────

static void draw_check(GContext *ctx, int ox, int oy, GColor c) {
    graphics_context_set_stroke_color(ctx, c);
    graphics_context_set_stroke_width(ctx, 6);
    graphics_draw_line(ctx, GPoint(ox + 16, oy + 40), GPoint(ox + 35, oy + 60));
    graphics_draw_line(ctx, GPoint(ox + 35, oy + 60), GPoint(ox + 78, oy + 16));
}

static void draw_x(GContext *ctx, int ox, int oy, GColor c) {
    graphics_context_set_stroke_color(ctx, c);
    graphics_context_set_stroke_width(ctx, 6);
    graphics_draw_line(ctx, GPoint(ox + 16, oy + 16), GPoint(ox + 78, oy + 64));
    graphics_draw_line(ctx, GPoint(ox + 78, oy + 16), GPoint(ox + 16, oy + 64));
}

static void draw_shh(GContext *ctx, int ox, int oy, GColor bg, GColor fg) {
    // Filled circle (mouth)
    graphics_context_set_fill_color(ctx, fg);
    graphics_fill_circle(ctx, GPoint(ox + 48, oy + 42), 22);
    // Horizontal bar (finger over mouth)
    graphics_context_set_fill_color(ctx, bg);
    graphics_fill_rect(ctx, GRect(ox + 24, oy + 35, 48, 14), 5, GCornersAll);
    // Fingertip
    graphics_context_set_fill_color(ctx, fg);
    graphics_fill_circle(ctx, GPoint(ox + 48, oy + 26), 9);
}

static void draw_door(GContext *ctx, int ox, int oy, GColor c) {
    // Door frame
    graphics_context_set_stroke_color(ctx, c);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_rect(ctx, GRect(ox + 14, oy + 14, 34, 52));
    // Doorknob
    graphics_context_set_fill_color(ctx, c);
    graphics_fill_circle(ctx, GPoint(ox + 40, oy + 40), 4);
    // Exit arrow pointing right
    graphics_context_set_stroke_width(ctx, 5);
    graphics_draw_line(ctx, GPoint(ox + 54, oy + 40), GPoint(ox + 82, oy + 40));
    graphics_draw_line(ctx, GPoint(ox + 72, oy + 30), GPoint(ox + 82, oy + 40));
    graphics_draw_line(ctx, GPoint(ox + 72, oy + 50), GPoint(ox + 82, oy + 40));
}

// ── Canvas ────────────────────────────────────────────────────────────────────

static void canvas_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    int bw = bounds.size.w / 2;   // 100
    int bh = bounds.size.h / 2;   // 114

    for (int i = 0; i < 4; i++) {
        int col = i % 2;
        int row = i / 2;
        int ox = col * bw;
        int oy = row * bh;
        GRect r = GRect(ox, oy, bw, bh);

        GColor bg = s_bg_normal[i];
        GColor fg = s_fg_normal[i];
        bool pressed = (s_pressed == i);
        if (pressed) { GColor tmp = bg; bg = fg; fg = tmp; }

        // Background
        graphics_context_set_fill_color(ctx, bg);
        graphics_fill_rect(ctx, r, 0, GCornerNone);

        // Icon
        switch (i) {
            case 0: draw_check(ctx, ox, oy, fg); break;
            case 1: draw_x(ctx, ox, oy, fg); break;
            case 2: draw_shh(ctx, ox, oy, bg, fg); break;
            case 3: draw_door(ctx, ox, oy, fg); break;
        }

        // Label
        graphics_context_set_text_color(ctx, fg);
        GFont font = (i < 2) ? s_font_big : s_font_small;
        int text_y = (i == 3) ? (oy + 68) : (oy + 76);
        int text_h = bh - (text_y - oy) - 2;
        GRect text_rect = GRect(ox + 4, text_y, bw - 8, text_h);
        graphics_draw_text(ctx, s_labels[i], font, text_rect,
                           GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }

    // Divider lines
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, GPoint(bw, 0), GPoint(bw, bounds.size.h));
    graphics_draw_line(ctx, GPoint(0, bh), GPoint(bounds.size.w, bh));
}

// ── Window ────────────────────────────────────────────────────────────────────

static void window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);

    s_canvas = layer_create(bounds);
    layer_set_update_proc(s_canvas, canvas_update_proc);
    layer_add_child(root, s_canvas);

    s_font_big = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    s_font_small = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

    touch_service_subscribe(touch_handler, NULL);
}

static void window_unload(Window *window) {
    touch_service_unsubscribe();
    layer_destroy(s_canvas);
    if (s_flash_timer) { app_timer_cancel(s_flash_timer); s_flash_timer = NULL; }
    if (speaker_get_status() != SpeakerStatusIdle) speaker_stop();
}

// ── Main ──────────────────────────────────────────────────────────────────────

static void init(void) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){
        .load = window_load,
        .unload = window_unload
    });
    window_stack_push(s_window, true);
}

static void deinit(void) {
    window_destroy(s_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}
