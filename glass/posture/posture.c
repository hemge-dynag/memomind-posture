#include "gm_plugin_lvgl_api.h"
#include "gm_plugin_libc.h"

/* Paired with PhoneSDK/examples/posture. The glasses continuously read the raw
 * IMU pitch angle and warn the wearer when they slouch (head tilted away from a
 * calibrated neutral posture) for longer than a configured delay. The phone
 * owns the settings (enable, angle threshold, hold time) and keeps a per-day
 * tally of the reminders. */

#define SETTINGS_CHANNEL UINT16_C(0x504F) /* 'PO': phone -> glasses */
#define EVENT_CHANNEL UINT16_C(0x5045)    /* 'PE': glasses -> phone */
#define PROTOCOL_VERSION 1U

#define COMMAND_CONFIG 1U
#define COMMAND_REQUEST_CALIBRATE 2U
#define COMMAND_STOP 3U

#define EVENT_CALIBRATED UINT8_C(1)
#define EVENT_ALERT UINT8_C(2)

#define MIN_CONFIG_PAYLOAD 5U /* version + command + enabled + threshold + seconds */

#define DEFAULT_THRESHOLD_DEG 15
#define MIN_THRESHOLD_DEG 5
#define MAX_THRESHOLD_DEG 45
#define DEFAULT_DURATION_SEC 5
#define MIN_DURATION_SEC 2
#define MAX_DURATION_SEC 30

#define SAMPLE_INTERVAL_MS 100U
/* How long the "Redresse-toi" reminder stays on screen. */
#define ALERT_DISPLAY_MS 4000U
/* Minimum quiet time between two reminders so it never nags continuously. */
#define ALERT_COOLDOWN_MS 30000U
/* Full width of the inclination gauge, in degrees on either side of neutral. */
#define GAUGE_RANGE_DEG 30

#define SCREEN_MARGIN 20
#define TITLE_HEIGHT 24
#define STATUS_HEIGHT 44
#define TRACK_HEIGHT 14
#define KNOB_WIDTH 18
#define KNOB_HEIGHT 26
#define HINT_HEIGHT 30

typedef struct {
    const gm_plugin_host_api_t *host;
    const gm_plugin_lvgl_api_t *ui;
    const gm_plugin_libc_extension_api_t *libc;
    gm_plugin_lvgl_obj_t *screen;
    gm_plugin_lvgl_obj_t *title_label;
    gm_plugin_lvgl_obj_t *status_label;
    gm_plugin_lvgl_obj_t *gauge_track;
    gm_plugin_lvgl_obj_t *gauge_center;
    gm_plugin_lvgl_obj_t *gauge_knob;
    gm_plugin_lvgl_obj_t *stats_label;
    gm_plugin_lvgl_obj_t *hint_label;

    gm_plugin_lvgl_coord_t gauge_x;
    gm_plugin_lvgl_coord_t gauge_y;
    gm_plugin_lvgl_coord_t gauge_width;

    bool monitoring;
    bool calibrated;
    bool calibration_pending;
    bool has_sample;
    bool alert_active;
    bool device_state_capable;

    int16_t baseline_pitch;
    int16_t filtered_pitch;
    int16_t threshold_deg;
    uint16_t duration_ms;
    uint32_t bad_elapsed_ms;
    uint32_t alert_elapsed_ms;
    uint32_t cooldown_ms;
    uint32_t sample_elapsed_ms;
    uint16_t alert_count;
    uint8_t language; /* 0=fr 1=en 2=es 3=it 4=de 5=zh-CN */
} posture_t;

/* Picks the string for the detected UI locale. */
#define L(fr_str, en_str, es_str, it_str, de_str, zh_str) \
    (po.language == 1U ? (en_str) : po.language == 2U ? (es_str) : \
     po.language == 3U ? (it_str) : po.language == 4U ? (de_str) : \
     po.language == 5U ? (zh_str) : (fr_str))

static posture_t po;

static uint8_t posture_detect_language(const char *locale)
{
    if (locale[0] == 'e' && locale[1] == 'n') return 1U;
    if (locale[0] == 'e' && locale[1] == 's') return 2U;
    if (locale[0] == 'i' && locale[1] == 't') return 3U;
    if (locale[0] == 'd' && locale[1] == 'e') return 4U;
    if (locale[0] == 'z' && locale[1] == 'h') return 5U;
    return 0U;
}

#define number gm_plugin_lvgl_style_number
#define color gm_plugin_lvgl_style_color

static int32_t clamp_i32(int32_t value, int32_t minimum, int32_t maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int32_t abs_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

static void set_style(gm_plugin_lvgl_obj_t *object,
                       gm_plugin_lvgl_style_prop_t property,
                       gm_plugin_lvgl_style_value_t value)
{
    po.ui->style_set(object, property, value, GM_PLUGIN_LVGL_SELECTOR_MAIN);
}

static void style_panel(gm_plugin_lvgl_obj_t *object, uint8_t fill,
                         uint8_t border, uint8_t radius)
{
    set_style(object, GM_PLUGIN_LVGL_STYLE_BG_COLOR, color(fill));
    set_style(object, GM_PLUGIN_LVGL_STYLE_BG_OPA, number(255));
    set_style(object, GM_PLUGIN_LVGL_STYLE_BORDER_COLOR, color(0xA0));
    set_style(object, GM_PLUGIN_LVGL_STYLE_BORDER_OPA, number(255));
    set_style(object, GM_PLUGIN_LVGL_STYLE_BORDER_WIDTH, number(border));
    set_style(object, GM_PLUGIN_LVGL_STYLE_RADIUS, number(radius));
    po.ui->obj_clear_flag(object, GM_PLUGIN_LVGL_FLAG_SCROLLABLE);
}

static gm_plugin_lvgl_obj_t *make_label(gm_plugin_lvgl_obj_t *parent,
                                         const char *text, uint8_t shade)
{
    gm_plugin_lvgl_obj_t *label = po.ui->label_create(parent);
    if (label == 0) return 0;
    po.ui->label_set_text(label, text);
    po.ui->label_set_long_mode(label, GM_PLUGIN_LVGL_LABEL_DOT);
    set_style(label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR, color(shade));
    set_style(label, GM_PLUGIN_LVGL_STYLE_TEXT_ALIGN,
              number(GM_PLUGIN_LVGL_TEXT_ALIGN_CENTER));
    return label;
}

/* Current signed tilt from the calibrated neutral posture, in degrees. Positive
 * means the head is lowered relative to neutral (the slouching direction); the
 * detector reacts to either sign so leaning backwards is caught too. */
static int32_t posture_delta(void)
{
    return (int32_t)po.filtered_pitch - (int32_t)po.baseline_pitch;
}

static bool posture_is_bad(void)
{
    return abs_i32(posture_delta()) > (int32_t)po.threshold_deg;
}

static void position_knob(void)
{
    int32_t delta = clamp_i32(posture_delta(), -GAUGE_RANGE_DEG, GAUGE_RANGE_DEG);
    int32_t travel = (int32_t)po.gauge_width - KNOB_WIDTH;
    int32_t center = (int32_t)po.gauge_x + travel / 2;
    int32_t x = center + delta * (travel / 2) / GAUGE_RANGE_DEG;
    x = clamp_i32(x, po.gauge_x, (int32_t)po.gauge_x + travel);
    po.ui->obj_set_pos(po.gauge_knob, (gm_plugin_lvgl_coord_t)x,
                        (gm_plugin_lvgl_coord_t)(po.gauge_y + TRACK_HEIGHT / 2
                                            - KNOB_HEIGHT / 2));
}

static void refresh_display(void)
{
    char text[40];

    if (!po.monitoring) {
        po.ui->label_set_text(po.status_label,
            L("En attente du telephone...", "Waiting for the phone...", "Esperando al teléfono...", "In attesa del telefono...", "Warte auf das Telefon...", "正在等待手机..."));
        set_style(po.status_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR, color(0x90));
        po.ui->label_set_text(po.hint_label,
            L("Active le suivi depuis le telephone", "Enable tracking on the phone", "Activa el seguimiento en el teléfono", "Attiva il monitoraggio sul telefono", "Aktiviere die Überwachung am Telefon", "请在手机上启用追踪"));
        po.ui->label_set_text(po.stats_label, "");
        position_knob();
        return;
    }

    if (!po.calibrated) {
        po.ui->label_set_text(po.status_label,
            L("Calibration requise", "Calibration needed", "Calibración necesaria", "Calibrazione necessaria", "Kalibrierung nötig", "需要校准"));
        set_style(po.status_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR, color(0xD0));
        po.ui->label_set_text(po.hint_label,
            L("Tiens-toi droit puis appuie sur le bouton",
              "Sit up straight then press the button",
              "Siéntate derecho y pulsa el botón",
              "Siediti dritto e premi il pulsante",
              "Sitz gerade und drücke die Taste",
              "坐直后按下按钮"));
        po.ui->label_set_text(po.stats_label, "");
        position_knob();
        return;
    }

    if (po.alert_active) {
        po.ui->label_set_text(po.status_label, L("REDRESSE-TOI !", "SIT UP STRAIGHT!", "¡SIÉNTATE DERECHO!", "STAI DRITTO!", "SITZ GERADE!", "坐直！"));
        set_style(po.status_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR, color(0xFF));
    } else if (posture_is_bad()) {
        po.ui->label_set_text(po.status_label, L("Corrige ta posture", "Fix your posture", "Corrige tu postura", "Correggi la postura", "Korrigiere deine Haltung", "纠正姿势"));
        set_style(po.status_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR, color(0xD0));
    } else {
        po.ui->label_set_text(po.status_label, L("Bonne posture", "Good posture", "Buena postura", "Buona postura", "Gute Haltung", "姿势良好"));
        set_style(po.status_label, GM_PLUGIN_LVGL_STYLE_TEXT_COLOR, color(0xF0));
    }

    po.libc->snprintf(text, sizeof(text), L("Rappels : %u", "Reminders: %u", "Avisos: %u", "Avvisi: %u", "Hinweise: %u", "提醒：%u"),
                       (unsigned int)po.alert_count);
    po.ui->label_set_text(po.stats_label, text);
    po.ui->label_set_text(po.hint_label,
        L("Bouton : calibrer la posture", "Button: calibrate posture", "Botón: calibrar postura", "Pulsante: calibra postura", "Taste: Haltung kalibrieren", "按钮：校准姿势"));
    position_knob();
}

static void send_calibrated_event(void)
{
    uint8_t payload[4];
    payload[0] = PROTOCOL_VERSION;
    payload[1] = EVENT_CALIBRATED;
    payload[2] = (uint8_t)((uint16_t)po.baseline_pitch & 0xFFU);
    payload[3] = (uint8_t)(((uint16_t)po.baseline_pitch >> 8) & 0xFFU);
    (void)po.host->bt_send(EVENT_CHANNEL, payload, sizeof(payload));
}

static void send_alert_event(void)
{
    uint8_t payload[6];
    uint16_t pitch = (uint16_t)po.filtered_pitch;
    payload[0] = PROTOCOL_VERSION;
    payload[1] = EVENT_ALERT;
    payload[2] = (uint8_t)(po.alert_count & 0xFFU);
    payload[3] = (uint8_t)((po.alert_count >> 8) & 0xFFU);
    payload[4] = (uint8_t)(pitch & 0xFFU);
    payload[5] = (uint8_t)((pitch >> 8) & 0xFFU);
    (void)po.host->bt_send(EVENT_CHANNEL, payload, sizeof(payload));
}

/* Stores the current (filtered) pitch as the neutral reference and tells the
 * phone so it can show the calibrated value. */
static void apply_calibration(void)
{
    po.baseline_pitch = po.filtered_pitch;
    po.calibrated = true;
    po.calibration_pending = false;
    po.bad_elapsed_ms = 0U;
    po.alert_active = false;
    send_calibrated_event();
    refresh_display();
}

static void trigger_alert(void)
{
    if (po.alert_count < 0xFFFFU) po.alert_count++;
    po.alert_active = true;
    po.alert_elapsed_ms = 0U;
    po.bad_elapsed_ms = 0U;
    po.cooldown_ms = 0U;
    send_alert_event();
    refresh_display();
}

static void process_sample(const gm_plugin_imu_sample_t *sample)
{
    int16_t raw = sample->pitch_degrees;
    bool bad;

    /* Light exponential smoothing so gauge jitter and single-frame noise do not
     * toggle the posture state. */
    po.filtered_pitch = (int16_t)(((int32_t)po.filtered_pitch * 3 + raw) / 4);
    po.has_sample = true;

    if (po.calibration_pending) {
        apply_calibration();
        return;
    }

    if (po.sample_elapsed_ms >= SAMPLE_INTERVAL_MS) {
        po.sample_elapsed_ms = 0U;
        bad = po.monitoring && posture_is_bad();
        if (bad) {
            po.bad_elapsed_ms += SAMPLE_INTERVAL_MS;
            if (!po.alert_active && po.bad_elapsed_ms >= po.duration_ms &&
                po.cooldown_ms >= ALERT_COOLDOWN_MS) {
                trigger_alert();
            }
        } else {
            po.bad_elapsed_ms = 0U;
        }
        refresh_display();
    }
}

static void handle_settings_message(const uint8_t *data, uint32_t length)
{
    if (length < MIN_CONFIG_PAYLOAD || data[0] != PROTOCOL_VERSION) return;

    switch (data[1]) {
    case COMMAND_CONFIG:
        po.monitoring = data[2] != 0U;
        po.threshold_deg = (int16_t)clamp_i32((int32_t)data[3],
                                              MIN_THRESHOLD_DEG, MAX_THRESHOLD_DEG);
        po.duration_ms = (uint16_t)(clamp_i32((int32_t)data[4],
                                              MIN_DURATION_SEC, MAX_DURATION_SEC)
                                    * 1000);
        po.bad_elapsed_ms = 0U;
        refresh_display();
        break;
    case COMMAND_REQUEST_CALIBRATE:
        if (po.has_sample)
            apply_calibration();
        else
            po.calibration_pending = true;
        break;
    case COMMAND_STOP:
        po.monitoring = false;
        po.alert_active = false;
        po.bad_elapsed_ms = 0U;
        refresh_display();
        break;
    default:
        break;
    }
}

static gm_plugin_result_t posture_start(void *context)
{
    gm_plugin_display_info_t display;
    gm_plugin_lvgl_obj_t *root;
    char locale[GM_PLUGIN_LOCALE_TAG_MAX];
    (void)context;

    po.language = 0U;
    if (po.host->locale_get != 0 &&
        po.host->locale_get(locale) == GM_PLUGIN_OK)
        po.language = posture_detect_language(locale);

    if (po.host->display_get_info(&display) != GM_PLUGIN_OK ||
        display.width <= SCREEN_MARGIN * 2U ||
        display.height < TITLE_HEIGHT + STATUS_HEIGHT + HINT_HEIGHT + 80)
        return GM_PLUGIN_ESTATE;

    root = po.ui->root_get();
    if (root == 0) return GM_PLUGIN_ESTATE;
    po.ui->obj_clean(root);

    po.screen = po.ui->obj_create(root);
    if (po.screen == 0) return GM_PLUGIN_ENOMEM;
    po.ui->obj_set_size(po.screen, display.width, display.height);
    po.ui->obj_align(po.screen, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, 0);
    style_panel(po.screen, 0x00, 0, 0);

    po.title_label = make_label(po.screen, "Posture", 0x80);
    if (po.title_label == 0) goto no_memory;
    po.ui->obj_set_size(po.title_label, display.width - SCREEN_MARGIN * 2U, TITLE_HEIGHT);
    po.ui->obj_align(po.title_label, GM_PLUGIN_LVGL_ALIGN_TOP_MID, 0, 8);

    po.status_label = make_label(po.screen, "", 0xF0);
    if (po.status_label == 0) goto no_memory;
    {
        gm_plugin_lvgl_style_value_t large_font = {0};
        large_font.ptr = po.ui->font_large;
        set_style(po.status_label, GM_PLUGIN_LVGL_STYLE_TEXT_FONT, large_font);
    }
    po.ui->obj_set_size(po.status_label, display.width - SCREEN_MARGIN * 2U, STATUS_HEIGHT);
    po.ui->obj_align(po.status_label, GM_PLUGIN_LVGL_ALIGN_CENTER, 0, -18);

    po.gauge_x = SCREEN_MARGIN;
    po.gauge_width = (gm_plugin_lvgl_coord_t)(display.width - SCREEN_MARGIN * 2U);
    po.gauge_y = (gm_plugin_lvgl_coord_t)((int32_t)display.height / 2 + 24);

    po.gauge_track = po.ui->obj_create(po.screen);
    if (po.gauge_track == 0) goto no_memory;
    po.ui->obj_set_size(po.gauge_track, po.gauge_width, TRACK_HEIGHT);
    po.ui->obj_set_pos(po.gauge_track, po.gauge_x, po.gauge_y);
    style_panel(po.gauge_track, 0x18, 1, TRACK_HEIGHT / 2);

    /* A small mark in the middle of the track shows the calibrated neutral. */
    po.gauge_center = po.ui->obj_create(po.screen);
    if (po.gauge_center == 0) goto no_memory;
    po.ui->obj_set_size(po.gauge_center, 2, TRACK_HEIGHT + 6);
    po.ui->obj_set_pos(po.gauge_center,
                        (gm_plugin_lvgl_coord_t)((int32_t)po.gauge_x + po.gauge_width / 2),
                        (gm_plugin_lvgl_coord_t)(po.gauge_y - 3));
    style_panel(po.gauge_center, 0x60, 0, 0);

    po.gauge_knob = po.ui->obj_create(po.screen);
    if (po.gauge_knob == 0) goto no_memory;
    po.ui->obj_set_size(po.gauge_knob, KNOB_WIDTH, KNOB_HEIGHT);
    style_panel(po.gauge_knob, 0xE0, 1, KNOB_WIDTH / 2);

    po.stats_label = make_label(po.screen, "", 0x90);
    if (po.stats_label == 0) goto no_memory;
    po.ui->obj_set_size(po.stats_label, display.width - SCREEN_MARGIN * 2U, 24);
    po.ui->obj_align(po.stats_label, GM_PLUGIN_LVGL_ALIGN_CENTER, 0,
                      TRACK_HEIGHT / 2 + KNOB_HEIGHT);

    po.hint_label = make_label(po.screen, "", 0x70);
    if (po.hint_label == 0) goto no_memory;
    po.ui->obj_set_size(po.hint_label, display.width - SCREEN_MARGIN * 2U, HINT_HEIGHT);
    po.ui->obj_align(po.hint_label, GM_PLUGIN_LVGL_ALIGN_BOTTOM_MID, 0, -8);

    po.monitoring = false;
    po.calibrated = false;
    po.calibration_pending = false;
    po.has_sample = false;
    po.alert_active = false;
    po.baseline_pitch = 0;
    po.filtered_pitch = 0;
    po.threshold_deg = DEFAULT_THRESHOLD_DEG;
    po.duration_ms = DEFAULT_DURATION_SEC * 1000U;
    po.bad_elapsed_ms = 0U;
    po.alert_elapsed_ms = 0U;
    po.cooldown_ms = ALERT_COOLDOWN_MS;
    po.sample_elapsed_ms = SAMPLE_INTERVAL_MS;
    po.alert_count = 0U;

    refresh_display();

    if (po.host->imu_enable(GM_PLUGIN_IMU_ENABLE_RAW) != GM_PLUGIN_OK)
        goto no_memory;
    return GM_PLUGIN_OK;

no_memory:
    po.ui->obj_clean(root);
    po.screen = 0;
    po.title_label = 0;
    po.status_label = 0;
    po.gauge_track = 0;
    po.gauge_center = 0;
    po.gauge_knob = 0;
    po.stats_label = 0;
    po.hint_label = 0;
    return GM_PLUGIN_ENOMEM;
}

static void posture_loop(void *context, uint32_t elapsed_ms)
{
    gm_plugin_imu_sample_t sample;
    bool worn = true;
    (void)context;

    /* Guard against long stalls (e.g. after a resume) so timers do not jump. */
    if (elapsed_ms > 500U) elapsed_ms = 500U;

    if (!po.alert_active) {
        if (po.cooldown_ms < ALERT_COOLDOWN_MS) {
            uint32_t remaining = ALERT_COOLDOWN_MS - po.cooldown_ms;
            po.cooldown_ms = elapsed_ms >= remaining
                ? ALERT_COOLDOWN_MS : po.cooldown_ms + elapsed_ms;
        }
    }

    if (po.alert_active) {
        po.alert_elapsed_ms += elapsed_ms;
        if (po.alert_elapsed_ms >= ALERT_DISPLAY_MS) {
            po.alert_active = false;
            po.alert_elapsed_ms = 0U;
            refresh_display();
        }
    }

    if (po.sample_elapsed_ms < SAMPLE_INTERVAL_MS) {
        uint32_t remaining = SAMPLE_INTERVAL_MS - po.sample_elapsed_ms;
        po.sample_elapsed_ms = elapsed_ms >= remaining
            ? SAMPLE_INTERVAL_MS : po.sample_elapsed_ms + elapsed_ms;
    }

    if (po.device_state_capable && po.host->wearing != 0 && !po.host->wearing())
        worn = false;

    if (po.sample_elapsed_ms >= SAMPLE_INTERVAL_MS &&
        po.host->imu_read(&sample) == GM_PLUGIN_OK) {
        if (worn || po.calibration_pending)
            process_sample(&sample);
        else {
            /* Not worn: no reminder, and do not accumulate "bad posture" time. */
            po.filtered_pitch = (int16_t)(((int32_t)po.filtered_pitch * 3 +
                                            sample.pitch_degrees) / 4);
            po.bad_elapsed_ms = 0U;
            po.sample_elapsed_ms = 0U;
        }
    }
}

static bool posture_event(void *context, const gm_plugin_event_t *event)
{
    (void)context;
    if (event == 0) return false;

    if (event->type == GM_PLUGIN_EVENT_CONNECTION) {
        po.host->log("posture: phone connected=%u",
                      (unsigned int)event->data.connection.connected);
        if (!event->data.connection.connected && po.monitoring) {
            po.monitoring = false;
            po.alert_active = false;
            refresh_display();
        }
        return true;
    }

    if (event->type == GM_PLUGIN_EVENT_BT_MESSAGE) {
        if (event->data.bt.channel != SETTINGS_CHANNEL || event->data.bt.data == 0)
            return false;
        handle_settings_message(event->data.bt.data, event->data.bt.length);
        return true;
    }

    if (event->type == GM_PLUGIN_EVENT_BUTTON &&
        event->data.button.button == GM_PLUGIN_BUTTON_PRIMARY &&
        event->data.button.action == GM_PLUGIN_BUTTON_ACTION_SINGLE) {
        if (po.has_sample)
            apply_calibration();
        else
            po.calibration_pending = true;
        return true;
    }
    return false;
}

static void posture_stop(void *context)
{
    gm_plugin_lvgl_obj_t *root;
    (void)context;
    root = po.ui->root_get();
    (void)po.host->imu_enable(GM_PLUGIN_IMU_ENABLE_NONE);
    if (root != 0) po.ui->obj_clean(root);
    po.screen = 0;
    po.title_label = 0;
    po.status_label = 0;
    po.gauge_track = 0;
    po.gauge_center = 0;
    po.gauge_knob = 0;
    po.stats_label = 0;
    po.hint_label = 0;
    po.monitoring = false;
    po.alert_active = false;
    po.has_sample = false;
}

gm_plugin_result_t gm_plugin_entry(const gm_plugin_host_api_t *host,
                                    gm_plugin_descriptor_t *plugin)
{
    const gm_plugin_capabilities_t required =
        GM_PLUGIN_CAP_BLUETOOTH | GM_PLUGIN_CAP_BUTTON | GM_PLUGIN_CAP_IMU_RAW;

    if (host == 0 || plugin == 0 || host->log == 0 || host->bt_send == 0 ||
        host->display_get_info == 0 || host->graphics.lvgl == 0 ||
        host->imu_enable == 0 || host->imu_read == 0 ||
        !GM_PLUGIN_VERSION_COMPATIBLE(host->abi_version,
                                      GM_PLUGIN_ABI_MIN_VERSION) ||
        host->struct_size < GM_PLUGIN_HOST_API_MIN_SIZE ||
        plugin->struct_size < GM_PLUGIN_DESCRIPTOR_MIN_SIZE ||
        (host->capabilities & required) != required)
        return GM_PLUGIN_ENOTSUP;

    po.host = host;
    po.ui = host->graphics.lvgl;
    if (gm_plugin_libc_get(host, &po.libc) != GM_PLUGIN_OK)
        return GM_PLUGIN_ENOTSUP;
    if (po.ui->struct_size < GM_PLUGIN_LVGL_API_MIN_SIZE ||
        !GM_PLUGIN_VERSION_COMPATIBLE(po.ui->api_version,
                                      GM_PLUGIN_LVGL_API_MIN_VERSION))
        return GM_PLUGIN_EVERSION;

    po.device_state_capable =
        (host->capabilities & GM_PLUGIN_CAP_DEVICE_STATE) != 0U;

    plugin->abi_version = GM_PLUGIN_ABI_MIN_VERSION;
    plugin->context = &po;
    plugin->on_start = posture_start;
    plugin->on_loop = posture_loop;
    plugin->on_event = posture_event;
    plugin->on_stop = posture_stop;
    return GM_PLUGIN_OK;
}
