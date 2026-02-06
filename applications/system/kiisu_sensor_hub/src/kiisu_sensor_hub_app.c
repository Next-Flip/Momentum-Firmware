#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <gui/elements.h>
#include <furi_hal.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <stm32wbxx_ll_gpio.h>
#include <math.h>
#include <notification/notification_messages.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923f
#endif
#include "drivers/i2c_bus.h"
#include "drivers/lsm303.h"
#include "drivers/gxhtc3c.h"
#include "drivers/kiisu_light_adc.h"

// Widget types
typedef enum {
    WidgetTypeAccelerometer,
    WidgetTypeMagnetometer,
    WidgetTypeTemperature,
    WidgetTypeHumidity,
    WidgetTypeLight,
    WidgetTypeCount
} WidgetType;


// App states
typedef enum {
    AppStateWidgetMenu,
    AppStateWidgetDetail,
    AppStateCalibrating
} AppState;

// Calibration mode (which sensor is being calibrated within the shared flow)
typedef enum {
    CalibModeMagnetometer = 0,
    CalibModeAccelerometer = 1,
} CalibMode;

// Widget structure
typedef struct {
    WidgetType type;
    char* title;
    bool enabled;
    bool ok;
    void* data;
    void (*draw_summary)(Canvas* canvas, int x, int y, int width, int height, void* data);
    void (*draw_detail)(Canvas* canvas, void* data);
} Widget;

// App structure
typedef struct {
    Gui* gui;
    ViewPort* vp;
    FuriMessageQueue* input_queue;
    I2cBus i2c;
    Lsm303 lsm;
    Gxhtc3c th;
    
    // Sensor data
    Lsm303Sample lsm_sample;
    Gxhtc3cSample th_sample;
    KiisuLightAdcSample light_sample;
    
    // UI state
    AppState state;
    int selected_widget;
    Widget widgets[WidgetTypeCount];
    
    // Polling timing
    uint32_t last_lsm_poll_ms;
    uint32_t last_th_poll_ms;
    uint32_t last_light_poll_ms;

    // Little Kiisu physics state (position in pixels, velocity px/s, angle rad, angular velocity rad/s)
    // Legacy single-sprite fields (unused now)
    float img_x;
    float img_y;
    float img_vx;
    float img_vy;
    float img_angle;
    float img_av;
    // New multi-sprite physics sim for accelerometer detail page 0
    int accel_detail_page;       // 0 = gravity sim, 1 = raw data, 2 = level tool
    int sim_count;               // number of sprites (5)
    int sim_w, sim_h;            // sprite width/height in pixels
    float sim_r;                 // collision radius (circle approx)
    float sim_x[5], sim_y[5];
    float sim_vx[5], sim_vy[5];
    float sim_ang[5], sim_av[5];
    uint32_t last_physics_tick;  // last tick for sim integration

    // Homescreen kitty (inside accelerometer box on widget menu)
    bool home_has_kitty;         // enable a single kitty on homescreen
    float home_x, home_y;        // center position in pixels
    float home_vx, home_vy;      // velocity px/s
    float home_ang, home_av;     // angle rad, angular velocity rad/s
    float home_r;                // collision radius
    uint32_t home_last_tick;     // last tick for homescreen kitty integration

    // Axis mapping hard-locked to +Z rotation (no fields needed)

    // Calibration data
    float mag_min_x, mag_max_x;
    float mag_min_y, mag_max_y;
    float mag_min_z, mag_max_z;
    // Computed calibration constants (from last completed calibration)
    bool mag_calibrated;           // true if calibration constants below are valid
    float mag_offs[3];             // hard-iron offsets [uT]
    float mag_scale[3];            // soft-iron diagonal scale factors (unitless)
    bool mag_hw_offsets_applied;   // if true, offsets are already applied in HW registers
    // Simple IIR smoothing for magnetometer (helps noise/LPF equivalence)
    float mag_filt[3];
    float mag_iir_alpha;           // 0..1, fraction of new sample
    // Leveling (flat surface) baseline
    bool leveled;
    float level_ax, level_ay, level_az; // captured accel when flat
    uint32_t calibration_start_time;
    uint8_t calib_step;            // 0=flat wait, 1=flat avg, 2=fig8 wait, 3=fig8 run, 4=compute, 5=complete
    uint32_t calib_duration_ms;    // duration for current timed phase
    CalibMode calib_mode;          // which sensor the flow is acting upon

    // Orientation: rotation of sensor frame to device frame (0, 90, 180, 270 degrees)
    uint8_t orientation_rot; // 0..3 multiples of 90°
    // PCB mounting quirk: swap X and Y from the sensor
    bool sensor_swap_xy;
    
    // Magnetometer detail view paging: 0 = Compass, 1 = Raw values
    int mag_detail_page;
    
    // Compass heading data
    float heading_deg;          // instantaneous heading (0..360, 0 = North)
    float heading_deg_smooth;   // smoothed heading
    float head_vec_x;           // smoothing vector x = cos(heading)
    float head_vec_y;           // smoothing vector y = sin(heading)
    // Heading configuration
    float mag_declination_deg;   // local magnetic declination (deg), +East; adds to heading
    float heading_tilt_limit_deg; // do not update heading smoothing when tilt exceeds this
    // Compass snap state (cardinal lock)
    bool compass_snapped;       // true when within snap threshold
    int compass_snap_idx;       // 0=N,1=E,2=S,3=W when snapped
    // Orientation details for sim responsiveness
    float acc_pitch;            // radians
    float acc_roll;             // radians
    float yaw_rate_dps;         // yaw rate deg/s (from heading change)
    float pitch_rate_dps;       // pitch rate deg/s
    float roll_rate_dps;        // roll rate deg/s
    float last_yaw_deg;         // for rate calc
    float last_pitch;           // radians
    float last_roll;            // radians
    uint32_t last_orient_tick;  // ms tick for rates

    // Notification
    NotificationApp* notification;

    // Level averaging during calibration step 2
    float level_sum_ax, level_sum_ay, level_sum_az;
    uint32_t level_samples;
} App;

// --- Home-screen navigation mapping (by visual layout) ---
// Directions: stay in place when no logical neighbor.
// Index by WidgetType: Accelerometer(0), Magnetometer(1), Temperature(2), Humidity(3), Light(4)
static const int NAV_RIGHT[WidgetTypeCount] = {
    /* Accel */      WidgetTypeTemperature,
    /* Magnet */     WidgetTypeTemperature,
    /* Temp */       WidgetTypeTemperature,
    /* Humidity */   WidgetTypeMagnetometer,
    /* Light */      WidgetTypeHumidity,
};
static const int NAV_LEFT[WidgetTypeCount] = {
    /* Accel */      WidgetTypeHumidity,
    /* Magnet */     WidgetTypeHumidity,
    /* Temp */       WidgetTypeMagnetometer,
    /* Humidity */   WidgetTypeLight,
    /* Light */      WidgetTypeLight,
};
static const int NAV_UP[WidgetTypeCount] = {
    /* Accel */      WidgetTypeAccelerometer,
    /* Magnet */     WidgetTypeAccelerometer,
    /* Temp */       WidgetTypeTemperature,
    /* Humidity */   WidgetTypeAccelerometer,
    /* Light */      WidgetTypeAccelerometer,
};
static const int NAV_DOWN[WidgetTypeCount] = {
    /* Accel */      WidgetTypeMagnetometer,
    /* Magnet */     WidgetTypeMagnetometer,
    /* Temp */       WidgetTypeTemperature,
    /* Humidity */   WidgetTypeHumidity,
    /* Light */      WidgetTypeLight,
};

// Forward declarations for widget drawing functions
static void draw_accelerometer_summary(Canvas* canvas, int x, int y, int width, int height, void* data);
static void draw_magnetometer_summary(Canvas* canvas, int x, int y, int width, int height, void* data);
static void draw_temperature_summary(Canvas* canvas, int x, int y, int width, int height, void* data);
static void draw_humidity_summary(Canvas* canvas, int x, int y, int width, int height, void* data);
static void draw_light_summary(Canvas* canvas, int x, int y, int width, int height, void* data);

static void draw_accelerometer_detail(Canvas* canvas, void* data);
static void draw_magnetometer_detail(Canvas* canvas, void* data);
static void draw_temperature_detail(Canvas* canvas, void* data);
static void draw_humidity_detail(Canvas* canvas, void* data);
static void draw_light_detail(Canvas* canvas, void* data);

static void am_apply(const App* app, float x, float y, float z, float* xo, float* yo, float* zo);

// --- Utility: draw a rotated monochrome XBM bitmap around its center ---
// Note: continuous angle rotation using per-pixel transform; small sprite so fast enough.
static inline bool xbm_bit_is_set(const uint8_t* bits, int w, int x, int y) {
    int bytes_per_row = (w + 7) / 8;
    int idx = y * bytes_per_row + (x >> 3);
    uint8_t mask = 1u << (x & 7); // XBM LSB-first
    return (bits[idx] & mask) != 0;
}

static void canvas_draw_xbm_rotated(Canvas* canvas, int cx, int cy, int w, int h, const uint8_t* bits, float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    // center offsets so that rotation is about the visual center
    float ox0 = (w - 1) * 0.5f;
    float oy0 = (h - 1) * 0.5f;
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            if(!xbm_bit_is_set(bits, w, x, y)) continue;
            float rx = (float)x - ox0;
            float ry = (float)y - oy0;
            int dx = cx + (int)roundf(c * rx - s * ry);
            int dy = cy + (int)roundf(s * rx + c * ry);
            if(dx >= 0 && dx < 128 && dy >= 0 && dy < 64) {
                canvas_draw_dot(canvas, dx, dy);
            }
        }
    }
}

// Initialize widgets
static void init_widgets(App* app) {
    // Accelerometer widget
    app->widgets[WidgetTypeAccelerometer].type = WidgetTypeAccelerometer;
    app->widgets[WidgetTypeAccelerometer].title = "Accelerometer";
    app->widgets[WidgetTypeAccelerometer].enabled = true;
    app->widgets[WidgetTypeAccelerometer].ok = false;
    // pass the whole app so detail view can access both accel sample and compass heading
    app->widgets[WidgetTypeAccelerometer].data = app;
    app->widgets[WidgetTypeAccelerometer].draw_summary = draw_accelerometer_summary;
    app->widgets[WidgetTypeAccelerometer].draw_detail = draw_accelerometer_detail;
    
    // Magnetometer widget
    app->widgets[WidgetTypeMagnetometer].type = WidgetTypeMagnetometer;
    app->widgets[WidgetTypeMagnetometer].title = "Magnetometer";
    app->widgets[WidgetTypeMagnetometer].enabled = true;
    app->widgets[WidgetTypeMagnetometer].ok = false;
    app->widgets[WidgetTypeMagnetometer].data = app; // Pass the whole app context
    app->widgets[WidgetTypeMagnetometer].draw_summary = draw_magnetometer_summary;
    app->widgets[WidgetTypeMagnetometer].draw_detail = draw_magnetometer_detail;
    
    // Temperature widget
    app->widgets[WidgetTypeTemperature].type = WidgetTypeTemperature;
    app->widgets[WidgetTypeTemperature].title = "Temperature";
    app->widgets[WidgetTypeTemperature].enabled = true;
    app->widgets[WidgetTypeTemperature].ok = false;
    app->widgets[WidgetTypeTemperature].data = app; // need both SHTC3 and LSM303 temps
    app->widgets[WidgetTypeTemperature].draw_summary = draw_temperature_summary;
    app->widgets[WidgetTypeTemperature].draw_detail = draw_temperature_detail;

    // Humidity widget
    app->widgets[WidgetTypeHumidity].type = WidgetTypeHumidity;
    app->widgets[WidgetTypeHumidity].title = "Humidity";
    app->widgets[WidgetTypeHumidity].enabled = true;
    app->widgets[WidgetTypeHumidity].ok = false;
    app->widgets[WidgetTypeHumidity].data = &app->th_sample;
    app->widgets[WidgetTypeHumidity].draw_summary = draw_humidity_summary;
    app->widgets[WidgetTypeHumidity].draw_detail = draw_humidity_detail;
    
    // Light widget
    app->widgets[WidgetTypeLight].type = WidgetTypeLight;
    app->widgets[WidgetTypeLight].title = "Light";
    app->widgets[WidgetTypeLight].enabled = true;
    app->widgets[WidgetTypeLight].ok = false;
    app->widgets[WidgetTypeLight].data = &app->light_sample;
    app->widgets[WidgetTypeLight].draw_summary = draw_light_summary;
    app->widgets[WidgetTypeLight].draw_detail = draw_light_detail;
}

// Compass calculation logic
static void update_compass(App* app) {
    if(!app->lsm_sample.ok) {
        return;
    }

    // Read raw sensor frame vectors
    float ax = app->lsm_sample.ax;
    float ay = app->lsm_sample.ay;
    float mx = app->lsm_sample.mx;
    float my = app->lsm_sample.my;
    float mz = app->lsm_sample.mz;
    // Apply requested axis mapping consistently to both accel and mag
    float axm, aym, azm; am_apply(app, ax, ay, app->lsm_sample.az, &axm, &aym, &azm);
    float mxm, mym, mzm; am_apply(app, mx, my, mz, &mxm, &mym, &mzm);
    ax = axm; ay = aym; mx = mxm; my = mym; mz = mzm;

    // During calibration, find min/max values
    if(app->state == AppStateCalibrating) {
        if(mx < app->mag_min_x) app->mag_min_x = mx;
        if(mx > app->mag_max_x) app->mag_max_x = mx;
        if(my < app->mag_min_y) app->mag_min_y = my;
        if(my > app->mag_max_y) app->mag_max_y = my;
        if(mz < app->mag_min_z) app->mag_min_z = mz;
        if(mz > app->mag_max_z) app->mag_max_z = mz;
    }

    // Apply optional sensor X/Y swap due to PCB orientation
    if(app->sensor_swap_xy) {
        float tx = ax; ax = ay; ay = tx;
        tx = mx; mx = my; my = tx;
    }

    // Apply a light IIR smoothing to magnetometer; helps noise and approximates LPF
    // y = (1-alpha)*y_prev + alpha*x
    float a = app->mag_iir_alpha;
    app->mag_filt[0] = (1.0f - a) * app->mag_filt[0] + a * mx;
    app->mag_filt[1] = (1.0f - a) * app->mag_filt[1] + a * my;
    app->mag_filt[2] = (1.0f - a) * app->mag_filt[2] + a * mz;
    mx = app->mag_filt[0];
    my = app->mag_filt[1];
    mz = app->mag_filt[2];
    // Maintain filter state during calibration and normal run

    // Apply calibration (hard-iron offset and soft-iron diagonal scale)
    // Avoid double-subtracting if offsets already programmed into HW.
    float cx = mx - (app->mag_hw_offsets_applied ? 0.0f : app->mag_offs[0]);
    float cy = my - (app->mag_hw_offsets_applied ? 0.0f : app->mag_offs[1]);
    float cz = mz - (app->mag_hw_offsets_applied ? 0.0f : app->mag_offs[2]);
    cx *= app->mag_scale[0];
    cy *= app->mag_scale[1];
    cz *= app->mag_scale[2];

    // Tilt compensation using projection approach (robust around g variations):
    // a_n = normalize(accel)
    // m_h = m - (m·a_n) a_n (remove vertical component)
    // heading = atan2(m_h_y, m_h_x) then rotate to North
    float an_norm = sqrtf(ax*ax + ay*ay + app->lsm_sample.az*app->lsm_sample.az);
    if(an_norm < 1e-6f) an_norm = 1e-6f;
    float anx = ax / an_norm;
    float any = ay / an_norm;
    float anz = app->lsm_sample.az / an_norm;
    // Use calibrated mag vector (cx,cy,cz)
    float mdota = cx*anx + cy*any + cz*anz;
    float mhx = cx - mdota*anx;
    float mhy = cy - mdota*any;
    // No need for mhz for heading
    // Heading in radians, 0 = North, CW positive after transforms below
    float heading = atan2f(mhy, mhx); // 0 at +X; rotate to North below
    // Convert frame: assume +X points East, so heading_x = atan2(my2, mx2) gives 0=East.
    // Adjust so 0=North (add -90deg): North = +Y.
    heading -= (float)M_PI_2; // shift so 0 is North

    // Apply device orientation rotation (multiples of 90deg)
    heading -= (float)app->orientation_rot * (float)M_PI_2;

    // Add declination (convert degrees to radians)
    heading += app->mag_declination_deg * (float)M_PI / 180.0f;
    // Normalize to [-pi, pi]
    while(heading > (float)M_PI) heading -= 2.0f * (float)M_PI;
    while(heading < -(float)M_PI) heading += 2.0f * (float)M_PI;

    // Smooth heading on the unit circle to avoid wrap jitter
    float hx = cosf(heading);
    float hy = sinf(heading);
    // Adaptive smoothing and tilt gating: if the device is highly tilted, gate updates
    float tilt_deg = acosf(fmaxf(-1.0f, fminf(1.0f, anz))) * 180.0f / (float)M_PI; // angle from gravity
    // Adjust alpha based on accel magnitude to reduce noise when motion detected
    float gmag = sqrtf(app->lsm_sample.ax*app->lsm_sample.ax + app->lsm_sample.ay*app->lsm_sample.ay + app->lsm_sample.az*app->lsm_sample.az);
    float a_dyn = a;
    if(gmag > 1.2f || gmag < 0.8f) a_dyn = fminf(0.35f, a + 0.1f); // more responsive during motion
    bool allow_update = tilt_deg <= app->heading_tilt_limit_deg;
    float blend = allow_update ? a_dyn : 0.0f;
    app->head_vec_x = (1.0f - blend) * app->head_vec_x + blend * hx;
    app->head_vec_y = (1.0f - blend) * app->head_vec_y + blend * hy;
    float hnorm = sqrtf(app->head_vec_x * app->head_vec_x + app->head_vec_y * app->head_vec_y);
    if(hnorm > 1e-6f) { app->head_vec_x /= hnorm; app->head_vec_y /= hnorm; }
    float heading_s = atan2f(app->head_vec_y, app->head_vec_x);

    // Convert to degrees [0,360)
    float hd = heading * 180.0f / (float)M_PI;
    float hds = heading_s * 180.0f / (float)M_PI;
    if(hd < 0) hd += 360.0f;
    if(hds < 0) hds += 360.0f;
    app->heading_deg = hd;
    app->heading_deg_smooth = hds;
}

// Draw calibration screen
static void draw_calibrating_screen(Canvas* canvas, App* app) {
    // User-provided bitmap templates
    static const uint8_t image_Little_Kiisu_0_bits[] = {0xfa,0xff,0xff,0xff,0xff,0xff,0x0f,0x04,0x00,0x00,0x9d,0x3e,0x00,0x13,0xc2,0xff,0x1f,0x9d,0x3e,0x00,0x20,0xc2,0xff,0x1f,0x9d,0x3e,0x30,0x2c,0xc3,0xff,0x1f,0x9f,0x3e,0x30,0x2c,0xc2,0xff,0x1f,0x80,0x3e,0x86,0x2d,0xc2,0xff,0x1f,0x80,0x3e,0x86,0x2d,0xc2,0xff,0x1f,0x90,0x3e,0x30,0x2c,0xc2,0xff,0x1f,0x92,0x3f,0x32,0x2c,0xc2,0xff,0x1f,0x00,0x00,0x00,0x2c,0xc2,0xff,0x1f,0x00,0x00,0x00,0x2c,0xc2,0xff,0x1f,0x00,0x00,0x00,0x20,0x1d,0x00,0x00,0x40,0x8a,0x75,0x20,0x20,0x00,0x00,0x40,0x00,0x74,0x21,0x20,0x55,0x2b,0x00,0x9f,0x71,0x3c,0x20,0x55,0x29,0x00,0x9f,0x05,0x20,0x20,0x53,0x2b,0x1c,0x1f,0x40,0x38,0x30,0x55,0x2a,0x1c,0x9f,0x40,0x20,0x20,0x55,0x3b,0x1c,0x1f,0x08,0x3c,0x20,0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x00,0x00,0x18,0x08,0x00,0x20,0x20,0x05,0x00,0x00,0x08,0xf0,0x21,0x1c,0x01,0x40,0x02,0x01,0xf8,0x23,0x02,0x00,0x40,0x08,0x00,0x0c,0x26,0xee,0xaa,0x00,0x00,0x00,0xfe,0x2f,0xee,0x00,0x00,0x00,0x00,0xfe,0x2f,0xee,0x00,0x00,0x00,0x00,0xfe,0x2f,0x02,0x00,0x00,0x00,0x00,0x00,0xfe,0x2f,0x56,0x55,0x55,0x55,0x55,0xfe,0x2f,0x06,0x00,0x00,0x00,0x00,0x0c,0x26,0x02,0xc1,0x06,0x19,0x24,0xf8,0x23,0x62,0xc8,0x20,0x98,0x84,0xf0,0x21,0x04,0x00,0x00,0x00,0x00,0x00,0x10,0xf8,0xff,0xff,0xff,0xff,0xff,0x0f};
    static const uint8_t image_Screenshot_2025_08_20_200213_0_bits[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x0f,0x00,0x00,0xe0,0x0f,0x00,0x00,0x00,0x1c,0x70,0x00,0x00,0x1c,0x70,0x00,0x00,0x00,0x1c,0x70,0x00,0x00,0x1c,0x70,0x00,0x00,0x00,0x03,0x80,0x00,0x00,0x03,0x80,0x00,0x00,0x80,0x00,0x00,0x03,0x80,0x00,0x00,0x03,0x00,0x80,0x00,0x00,0x03,0x80,0x00,0x00,0x03,0x00,0x60,0x00,0x00,0x04,0x60,0x00,0x00,0x04,0x00,0x60,0x00,0x00,0x18,0x10,0x00,0x00,0x04,0x00,0x60,0x00,0x00,0x18,0x10,0x00,0x00,0x04,0x00,0x10,0x00,0x00,0x20,0x08,0x00,0x00,0x18,0x00,0x10,0x00,0x00,0xc0,0x06,0x00,0x00,0x18,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x10,0x00,0x00,0x00,0x01,0x00,0x00,0x18,0x00,0x10,0x00,0x00,0xc0,0x06,0x00,0x00,0x18,0x00,0x10,0x00,0x00,0x20,0x08,0x00,0x00,0x18,0x00,0x10,0x00,0x00,0x20,0x08,0x00,0x00,0x18,0x00,0x60,0x00,0x00,0x18,0x10,0x00,0x00,0x04,0x00,0x60,0x00,0x00,0x04,0x60,0x00,0x00,0x04,0x00,0x60,0x00,0x00,0x04,0x60,0x00,0x00,0x04,0x00,0x80,0x00,0x00,0x03,0x80,0x00,0x00,0x03,0x00,0x00,0x03,0x80,0x00,0x00,0x03,0x80,0x00,0x00,0x00,0x03,0x80,0x00,0x00,0x03,0x80,0x00,0x00,0x00,0x1c,0x70,0x00,0x00,0x1c,0x70,0x00,0x00,0x00,0xe0,0x0f,0x00,0x00,0xe0,0x0f,0x00,0x00,0x00,0xe0,0x0f,0x00,0x00,0xe0,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

    canvas_clear(canvas);

    if(app->calib_step == 0 || app->calib_step == 1) {
        // Flat template
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 8, 7, "Place Kiisu on a flat surface");
        canvas_draw_str(canvas, 21, 15, "Press OK to continue");
        canvas_draw_xbm(canvas, 4, 17, 54, 34, image_Little_Kiisu_0_bits);
        canvas_draw_str(canvas, 80, 26, "Time left:");
        canvas_set_font(canvas, FontBigNumbers);
        // Show countdown only during averaging (step 1)
        if(app->calib_step == 1) {
            uint32_t elapsed = furi_get_tick() - app->calibration_start_time;
            uint32_t remaining_ms = (elapsed >= app->calib_duration_ms) ? 0 : (app->calib_duration_ms - elapsed);
            uint32_t remaining_s = (remaining_ms + 999) / 1000;
            char tbuf[12];
            snprintf(tbuf, sizeof(tbuf), "%lu", (unsigned long)remaining_s);
            canvas_draw_str(canvas, 88, 43, tbuf);
        } else {
        // Show the actual planned duration for the averaging phase
        uint32_t planned_s = (app->calib_duration_ms + 999) / 1000;
        char tbuf[12];
        snprintf(tbuf, sizeof(tbuf), "%lu", (unsigned long)planned_s);
        canvas_draw_str(canvas, 88, 43, tbuf);
        }
        elements_button_left(canvas, "Back");
        if(app->calib_step == 0) elements_button_center(canvas, "OK");
    } else if(app->calib_step == 2 || app->calib_step == 3) {
        // Figure-eight template
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 10, 7, "Move Kiisu in a figure eight");
        canvas_draw_str(canvas, 20, 15, "Press OK to continue");
        canvas_draw_xbm(canvas, 1, 17, 65, 32, image_Screenshot_2025_08_20_200213_0_bits);
        canvas_draw_str(canvas, 80, 26, "Time left:");
        canvas_set_font(canvas, FontBigNumbers);
        if(app->calib_step == 3) {
            uint32_t elapsed = furi_get_tick() - app->calibration_start_time;
            uint32_t remaining_ms = (elapsed >= app->calib_duration_ms) ? 0 : (app->calib_duration_ms - elapsed);
            uint32_t remaining_s = (remaining_ms + 999) / 1000;
            char tbuf[12];
            snprintf(tbuf, sizeof(tbuf), "%lu", (unsigned long)remaining_s);
            canvas_draw_str(canvas, 88, 43, tbuf);
        } else {
        // Show the planned duration for the figure-eight run
        uint32_t planned_s = (app->calib_duration_ms + 999) / 1000;
        char tbuf[12];
        snprintf(tbuf, sizeof(tbuf), "%lu", (unsigned long)planned_s);
        canvas_draw_str(canvas, 88, 43, tbuf);
        }
        elements_button_left(canvas, "Back");
        if(app->calib_step == 2) elements_button_center(canvas, "OK");
    } else if(app->calib_step == 5) {
        // Completion page
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 20, "Calibration complete");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 6, 34, "Everything's done.");
        canvas_draw_str(canvas, 6, 46, "Press BACK to return.");
        elements_button_left(canvas, "Back");
    }
}

// Widget drawing functions - Summary views
static void draw_accelerometer_summary(Canvas* canvas, int x, int y, int width, int height, void* data) {
    UNUSED(width);
    UNUSED(height);
    // data is App*
    App* app = (App*)data;
    Lsm303Sample* sample = &app->lsm_sample;
    canvas_set_font(canvas, FontSecondary);

    if(sample->ok) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fg", (double)sample->ax);
        canvas_draw_str(canvas, x + 6, y + 16, buf);
        snprintf(buf, sizeof(buf), "%.1fg", (double)sample->ay);
        canvas_draw_str(canvas, x + 6, y + 24, buf);
    } else {
        canvas_draw_str(canvas, x + 6, y + 18, "No data");
    }
}

static void draw_magnetometer_summary(Canvas* canvas, int x, int y, int width, int height, void* data) {
    UNUSED(width);
    UNUSED(height);
    App* app = (App*)data;
    canvas_set_font(canvas, FontSecondary);
    if(app->widgets[WidgetTypeMagnetometer].ok) {
        // Show wind direction (cardinal) based on smoothed heading
        float hd = app->heading_deg_smooth;
        while(hd < 0.0f) hd += 360.0f;
        while(hd >= 360.0f) hd -= 360.0f;
        static const char* labels8[8] = {"N","NE","E","SE","S","SW","W","NW"};
        int idx = (int)lroundf(hd / 45.0f) & 7;
        const char* dir = labels8[idx];
        canvas_draw_str(canvas, x + 6, y + 18, dir);
    } else {
        canvas_draw_str(canvas, x + 6, y + 18, "No data");
    }
}

static void draw_temperature_summary(Canvas* canvas, int x, int y, int width, int height, void* data) {
    UNUSED(width);
    UNUSED(height);
    App* app = (App*)data;
    canvas_set_font(canvas, FontSecondary);
    // Approximate temperature using both sensors when available:
    // - GXHTC3C (ambient) preferred; LSM303 die temp included as simple average if both OK
    bool have_gx = app->th_sample.ok;
    bool have_st = app->lsm_sample.ok; // LSM303 temp_c
    if(have_gx || have_st) {
        float t;
        if(have_gx && have_st) {
            t = 0.5f * (app->th_sample.temperature_c + app->lsm_sample.temp_c);
        } else if(have_gx) {
            t = app->th_sample.temperature_c;
        } else {
            t = app->lsm_sample.temp_c;
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0fC", (double)t);
        canvas_draw_str(canvas, x + 6, y + 18, buf);
    } else {
        canvas_draw_str(canvas, x + 6, y + 18, "--C");
    }
}

static void draw_humidity_summary(Canvas* canvas, int x, int y, int width, int height, void* data) {
    UNUSED(width);
    UNUSED(height);
    Gxhtc3cSample* sample = (Gxhtc3cSample*)data;
    canvas_set_font(canvas, FontSecondary);
    if(sample->ok) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f%%", (double)sample->humidity_rh);
        canvas_draw_str(canvas, x + 6, y + 18, buf);
    } else {
        canvas_draw_str(canvas, x + 6, y + 18, "--");
    }
}

static void draw_light_summary(Canvas* canvas, int x, int y, int width, int height, void* data) {
    UNUSED(width);
    UNUSED(height);
    KiisuLightAdcSample* sample = (KiisuLightAdcSample*)data;
    canvas_set_font(canvas, FontSecondary);
    if(sample->ok) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f%%", (double)sample->percent);
        canvas_draw_str(canvas, x + 6, y + 18, buf);
    } else {
        canvas_draw_str(canvas, x + 6, y + 18, "No data");
    }
}

// Widget drawing functions - Detail views



static void draw_accelerometer_detail(Canvas* canvas, void* data) {
    App* app = (App*)data;
    Lsm303Sample* sample = &app->lsm_sample;
    canvas_clear(canvas);
    // Title removed per request; use full screen for content

    // Pages: 0 = Kitties (gravity sim), 1 = Level, 2 = Raw data (Calib)
    if(app->accel_detail_page == 0) {
        // Physics integration step
        uint32_t now = furi_get_tick();
    float dt = (now - app->last_physics_tick) / 1000.0f;
    if(dt < 0.0f) dt = 0.0f;
    if(dt > 0.05f) dt = 0.05f; // clamp for stability
        app->last_physics_tick = now;

        // Compute gravity from accelerometer sample (screen coordinates: +X right, +Y down).
        // Use X,Y components and map sensor axes considering optional swap and orientation.
        float ax = 0.0f, ay = 0.0f;
        if(sample->ok) {
            float saxr = sample->ax, sayr = sample->ay, sazr = sample->az;
            if(app->sensor_swap_xy){ float t=saxr; saxr=sayr; sayr=t; }
            float sax, say, saz; am_apply(app, saxr, sayr, sazr, &sax, &say, &saz);
            // Subtract baseline (neutral) if leveled to avoid constant drift when held level
            if(app->leveled) {
                sax -= app->level_ax;
                say -= app->level_ay;
            }
            // Rotate according to screen orientation multiples of 90°
            float rx = sax, ry = say;
            switch(app->orientation_rot & 3){
                case 1: { float tx=rx; rx=ry; ry=-tx; } break;   // 90°
                case 2: { rx=-rx; ry=-ry; } break;               // 180°
                case 3: { float tx=rx; rx=-ry; ry=tx; } break;   // 270°
                default: break;
            }
            // Scale g (~9.81 m/s^2) to px/s^2.
            const float gscale = 80.0f;
            ax = rx * gscale;
            ay = ry * gscale;
            // Add a small swirl force from yaw rate to emulate spin coupling
            float swirl = app->yaw_rate_dps * 0.03f; // px/s^2 per deg/s
            ax += -ry * swirl * 0.2f;
            ay +=  rx * swirl * 0.2f;
        }

        // Bounds of play area below the title line
    const float minx = 0.0f + app->sim_r;
    const float miny = 0.0f + app->sim_r;
        const float maxx = 128.0f - app->sim_r;
        const float maxy = 64.0f - app->sim_r;

        // Integrate sprites
        for(int i=0;i<app->sim_count;i++){
            // velocity
            app->sim_vx[i] += ax * dt;
            app->sim_vy[i] += ay * dt;
            // simple damping to avoid runaway
            app->sim_vx[i] *= 0.995f;
            app->sim_vy[i] *= 0.995f;
            // static friction: if nearly level and slow, damp harder
            if(fabsf(ax) < 3.0f && fabsf(ay) < 3.0f) {
                if(fabsf(app->sim_vx[i]) < 3.0f) app->sim_vx[i] *= 0.9f;
                if(fabsf(app->sim_vy[i]) < 3.0f) app->sim_vy[i] *= 0.9f;
            }
            // position
            app->sim_x[i] += app->sim_vx[i] * dt;
            app->sim_y[i] += app->sim_vy[i] * dt;
            // angular: base spin plus yaw coupling
            float yaw_spin = app->yaw_rate_dps * (float)M_PI / 180.0f * 0.5f;
            app->sim_ang[i] += (app->sim_av[i] + yaw_spin) * dt;
        }

        // Wall collisions (elastic, with restitution)
    const float e = 0.75f;
        for(int i=0;i<app->sim_count;i++){
            if(app->sim_x[i] < minx){ app->sim_x[i] = minx; app->sim_vx[i] = -app->sim_vx[i]*e; }
            if(app->sim_x[i] > maxx){ app->sim_x[i] = maxx; app->sim_vx[i] = -app->sim_vx[i]*e; }
            if(app->sim_y[i] < miny){ app->sim_y[i] = miny; app->sim_vy[i] = -app->sim_vy[i]*e; }
            if(app->sim_y[i] > maxy){ app->sim_y[i] = maxy; app->sim_vy[i] = -app->sim_vy[i]*e; }
        }

        // Pairwise collisions (circle approx)
        for(int i=0;i<app->sim_count;i++){
            for(int j=i+1;j<app->sim_count;j++){
                float dx = app->sim_x[j]-app->sim_x[i];
                float dy = app->sim_y[j]-app->sim_y[i];
                float r2 = dx*dx+dy*dy;
                float minr = 2.0f*app->sim_r - 0.5f; // slight overlap tolerance
        if(r2 > 0.0001f && r2 < (minr*minr)){
                    float d = sqrtf(r2);
                    float nx = dx/d, ny = dy/d;
                    float pen = (minr - d) * 0.5f;
                    // separate
                    app->sim_x[i] -= nx*pen; app->sim_y[i] -= ny*pen;
                    app->sim_x[j] += nx*pen; app->sim_y[j] += ny*pen;
                    // relative velocity along normal
                    float rvx = app->sim_vx[j]-app->sim_vx[i];
                    float rvy = app->sim_vy[j]-app->sim_vy[i];
                    float rel = rvx*nx + rvy*ny;
                    if(rel < 0.0f){
                        float jimp = -(1.0f+e)*rel*0.5f; // equal mass
                        float jx = jimp*nx, jy = jimp*ny;
                        app->sim_vx[i] -= jx; app->sim_vy[i] -= jy;
                        app->sim_vx[j] += jx; app->sim_vy[j] += jy;
            // add a tiny spin exchange based on tangential component for looseness
            float tvx = rvx - rel*nx;
            float tvy = rvy - rel*ny;
            float tmag = tvx*nx + tvy*ny; // sign proxy
            app->sim_av[i] -= 0.05f * tmag;
            app->sim_av[j] += 0.05f * tmag;
                    }
                }
            }
        }

        // Render: draw 5 kitty sprites centered at (x,y) with simple rotation hint (small line)
        // Kitty bitmap 16x15 from user-provided data
        static const uint8_t image_kitty_0_bits[] = {0x01,0x08,0x03,0x0c,0x07,0x0e,0x0f,0x0f,0xff,0x0f,0xff,0x0f,0xf3,0x0c,0xf3,0x0c,0xf3,0x0c,0xff,0x0f,0x0f,0x0f,0x9f,0x0f,0xff,0x0f,0xfe,0x07,0xfc,0x03};
        for(int i=0;i<app->sim_count;i++){
            // draw rotated kitty around its center for a “loose” feel
            float ang = app->sim_ang[i];
            int cx = (int)roundf(app->sim_x[i]);
            int cy = (int)roundf(app->sim_y[i]);
            // keep within screen bounds by clamping center a bit
            if(cx < 0) cx = 0;
            if(cx > 127) cx = 127;
            if(cy < 0) cy = 0;
            if(cy > 63) cy = 63;
            canvas_draw_xbm_rotated(canvas, cx, cy, app->sim_w, app->sim_h, image_kitty_0_bits, ang);
        }

    canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, "Back");
    // Calibration is available only on Raw page
    elements_button_right(canvas, "Next");
    } else if(app->accel_detail_page == 1) {
        // Level page (auto-selects bullseye vs bar depending on gravity alignment)
        float rx = 0.0f, ry = 0.0f, rz = 1.0f;
        if(sample->ok) {
            float saxr = sample->ax, sayr = sample->ay, sazr = sample->az;
            if(app->sensor_swap_xy){ float t=saxr; saxr=sayr; sayr=t; }
            float sax, say, saz; am_apply(app, saxr, sayr, sazr, &sax, &say, &saz);
            rx = sax; ry = say; rz = saz;
            switch(app->orientation_rot & 3){
                case 1: { float tx=rx; rx=ry; ry=-tx; } break;
                case 2: { rx=-rx; ry=-ry; } break;
                case 3: { float tx=rx; rx=-ry; ry=tx; } break;
                default: break;
            }
            float g = sqrtf(rx*rx + ry*ry + rz*rz);
            if(g > 1e-3f){ rx/=g; ry/=g; rz/=g; }
        }

        bool flat_mode = fabsf(rz) > 0.85f; // near face-up/down
        if(flat_mode) {
            // Bullseye
            int cx = 64, cy = 38; // center
            int r = 22;
            canvas_draw_circle(canvas, cx, cy, r);
            canvas_draw_circle(canvas, cx, cy, r/2);
            canvas_draw_line(canvas, cx - r, cy, cx + r, cy);
            canvas_draw_line(canvas, cx, cy - r, cx, cy + r);

            float thx = atan2f(rx, rz);
            float thy = atan2f(ry, rz);
            float th_mag = sqrtf(thx*thx + thy*thy);
            float th_deg = th_mag * 180.0f / (float)M_PI;
            // Snap bubble to exact center within +/-3 degrees
            int bx, by;
            if(th_deg <= 3.0f) {
                bx = cx; by = cy; th_deg = 0.0f;
            } else {
                const float th_sat = 15.0f * (float)M_PI / 180.0f;
                float k = (float)(r - 4) / tanf(th_sat);
                bx = cx - (int)roundf(tanf(thx) * k);
                by = cy - (int)roundf(tanf(thy) * k);
                float dx = (float)bx - cx, dy = (float)by - cy;
                float d = sqrtf(dx*dx + dy*dy);
                float maxd = (float)r - 4.0f;
                if(d > maxd && d > 1e-3f){ bx = cx + (int)roundf(dx * (maxd/d)); by = cy + (int)roundf(dy * (maxd/d)); }
            }
            canvas_draw_disc(canvas, bx, by, 3);

            // Degree number: move to vertical middle at left side between edge and circle
            char nbuf[16];
            snprintf(nbuf, sizeof(nbuf), "%.0f", (double)th_deg);
            canvas_set_font(canvas, FontBigNumbers);
            int w = canvas_string_width(canvas, nbuf);
            int left_edge = 2;
            int right_edge = cx - r - 2;
            if(right_edge < left_edge + w) right_edge = left_edge + w; // avoid negative space
            int tx = left_edge + ((right_edge - left_edge) - w) / 2;
            int ty = cy + 6; // baseline around vertical middle
            canvas_draw_str(canvas, tx, ty, nbuf);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, tx + w + 2, ty, "\xC2\xB0");

            elements_button_left(canvas, "Back");
            elements_button_right(canvas, "Next");
        } else {
            // Upright level: choose horizontal or vertical bar depending on flip (dominant axis)
            bool vertical_bar = fabsf(rx) > fabsf(ry);
            if(!vertical_bar) {
                // Horizontal bar
                // Angle relative to level using screen-frame gravity: 0 when ry is +1 and rx is 0
                float angle = atan2f(rx, ry) * 180.0f / (float)M_PI;
                // Compute deviation from nearest multiple of 180 for snapping
                float dev0 = fabsf(angle);
                float dev180 = fabsf(fabsf(angle) - 180.0f);
                float dev = fminf(dev0, dev180);
                // Map angle for tan() to nearest 0 reference (keeps bubble smooth near 180)
                float angle_map = angle;
                if(fabsf(angle_map) > 90.0f) angle_map = angle_map - copysignf(180.0f, angle_map);

                int bx = 8, by = 28, bw = 112, bh = 12;
                canvas_draw_rframe(canvas, bx, by, bw, bh, 3);
                canvas_draw_line(canvas, bx + bw/2, by, bx + bw/2, by + bh);
                for(int i=1;i<=3;i++){
                    int txi = bx + (bw/2) + i*(bw/8);
                    int tyi = bx + (bw/2) - i*(bw/8);
                    canvas_draw_line(canvas, txi, by+2, txi, by+bh-2);
                    canvas_draw_line(canvas, tyi, by+2, tyi, by+bh-2);
                }
                // Snap bubble to center within +/-3 degrees; else flow freely via tan mapping
                int cxp;
                if(dev <= 3.0f) {
                    angle_map = 0.0f;
                    cxp = bx + bw/2;
                } else {
                    const float th_sat = 15.0f * (float)M_PI / 180.0f;
                    float k = (float)((bw/2) - 6) / tanf(th_sat);
                    cxp = bx + bw/2 - (int)roundf(tanf(angle_map * (float)M_PI / 180.0f) * k);
                    if(cxp < bx + 6) cxp = bx + 6;
                    if(cxp > bx + bw - 6) cxp = bx + bw - 6;
                }
                canvas_draw_disc(canvas, cxp, by + bh/2, 4);

                char nbuf[16];
                float adeg = fminf(dev0, dev180); // show the nearest deviation
                if(dev <= 3.0f) adeg = 0.0f; // snap displayed number too
                // Choose sign based on angle_map (direction of correction); no sign when 0
                const char* sign = (adeg == 0.0f) ? "" : (angle_map < 0 ? "-" : "");
                snprintf(nbuf, sizeof(nbuf), "%s%.0f", sign, (double)adeg);
                canvas_set_font(canvas, FontBigNumbers);
                int w = canvas_string_width(canvas, nbuf);
                int nx = 64 - w/2;
                // Move number higher to avoid overlap with bubble
                int ny = 14;
                if(adeg <= 3.0f) {
                    canvas_invert_color(canvas);
                    canvas_draw_box(canvas, nx - 3, ny - 12, w + 14, 20);
                    canvas_invert_color(canvas);
                }
                canvas_draw_str(canvas, nx, ny, nbuf);
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str(canvas, nx + w + 2, ny, "\xC2\xB0");

                elements_button_left(canvas, "Back");
                elements_button_right(canvas, "Next");
            } else {
                // Vertical bar (device flipped 90° either way)
                // Angle relative to vertical x-axis: 0 when rx is ±1 and ry is 0
                float angle = atan2f(-ry, rx) * 180.0f / (float)M_PI;
                // Deviation from nearest multiple of 180
                float dev0 = fabsf(angle);
                float dev180 = fabsf(fabsf(angle) - 180.0f);
                float dev = fminf(dev0, dev180);
                // Map to nearest 0 reference for tan
                float angle_map = angle;
                if(fabsf(angle_map) > 90.0f) angle_map = angle_map - copysignf(180.0f, angle_map);

                int bx = 58, by = 8, bw = 12, bh = 48;
                canvas_draw_rframe(canvas, bx, by, bw, bh, 3);
                // Center marker
                canvas_draw_line(canvas, bx, by + bh/2, bx + bw, by + bh/2);
                // Ticks
                for(int i=1;i<=3;i++){
                    int tyi = by + (bh/2) - i*(bh/8);
                    int tyi2 = by + (bh/2) + i*(bh/8);
                    canvas_draw_line(canvas, bx+2, tyi, bx+bw-2, tyi);
                    canvas_draw_line(canvas, bx+2, tyi2, bx+bw-2, tyi2);
                }
                // Snap bubble to center within +/-3 degrees; else flow freely via tan mapping
                int cyp;
                if(dev <= 3.0f) {
                    angle_map = 0.0f;
                    cyp = by + bh/2;
                } else {
                    const float th_sat = 15.0f * (float)M_PI / 180.0f;
                    float k = (float)((bh/2) - 6) / tanf(th_sat);
                    cyp = by + bh/2 - (int)roundf(tanf(angle_map * (float)M_PI / 180.0f) * k);
                    if(cyp < by + 6) cyp = by + 6;
                    if(cyp > by + bh - 6) cyp = by + bh - 6;
                }
                canvas_draw_disc(canvas, bx + bw/2, cyp, 4);

                // Number placed on the side (left or right of the bar), vertically centered
                char nbuf[16];
                float adeg = fminf(dev0, dev180);
                if(dev <= 3.0f) adeg = 0.0f; // snap displayed number too
                const char* sign = (adeg == 0.0f) ? "" : (angle_map < 0 ? "-" : "");
                snprintf(nbuf, sizeof(nbuf), "%s%.0f", sign, (double)adeg);
                canvas_set_font(canvas, FontBigNumbers);
                int w = canvas_string_width(canvas, nbuf);
                int left_space_l = 2;
                int left_space_r = bx - 2;
                int right_space_l = bx + bw + 2;
                int right_space_r = 126;
                // Swap left/right selection so vertical-right shows correct side
                bool place_right = (rx < 0.0f); // choose side based on which way is up
                // Rotate text to match device orientation so it's readable when flipped
                CanvasDirection dir = place_right ? CanvasDirectionTopToBottom : CanvasDirectionBottomToTop;
                canvas_set_font_direction(canvas, dir);
                int nx = place_right
                    ? (right_space_l + ((right_space_r - right_space_l) - 12) / 2) // 12px approx thickness
                    : (left_space_l + ((left_space_r - left_space_l) - 12) / 2);
                // Center vertically by distributing string "length" (w) around the center line
                int ny = (dir == CanvasDirectionTopToBottom)
                    ? (by + bh/2 - w/2)
                    : (by + bh/2 + w/2);
                if(adeg <= 3.0f) {
                    canvas_invert_color(canvas);
                    canvas_draw_box(canvas, nx - 3, ny - 12, w + 14, 20);
                    canvas_invert_color(canvas);
                }
                canvas_draw_str(canvas, nx, ny, nbuf);
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str(canvas, nx + w + 2, ny, "\xC2\xB0");
                // Restore default
                canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);

                elements_button_left(canvas, "Back");
                elements_button_right(canvas, "Next");
            }
        }
    } else {
        // Raw data page (calibration available here)
        canvas_set_font(canvas, FontSecondary);
        char buf[64];
        if(sample->ok) {
            // Place lines higher to avoid overlap with bottom button labels
            snprintf(buf, sizeof(buf), "X: %.2fg", (double)sample->ax);
            canvas_draw_str(canvas, 2, 14, buf);
            snprintf(buf, sizeof(buf), "Y: %.2fg", (double)sample->ay);
            canvas_draw_str(canvas, 2, 26, buf);
            snprintf(buf, sizeof(buf), "Z: %.2fg", (double)sample->az);
            canvas_draw_str(canvas, 2, 38, buf);
            // Overlaid compact orientation on the right side
            char obuf[32];
            snprintf(obuf, sizeof(obuf), "P:%+.0f", (double)(app->acc_pitch * 180.0f / (float)M_PI));
            int w = canvas_string_width(canvas, obuf);
            canvas_draw_str(canvas, 128 - w - 2, 14, obuf);
            snprintf(obuf, sizeof(obuf), "R:%+.0f", (double)(app->acc_roll * 180.0f / (float)M_PI));
            w = canvas_string_width(canvas, obuf);
            canvas_draw_str(canvas, 128 - w - 2, 26, obuf);
            snprintf(obuf, sizeof(obuf), "Y:%+.0f", (double)(app->heading_deg_smooth));
            w = canvas_string_width(canvas, obuf);
            canvas_draw_str(canvas, 128 - w - 2, 38, obuf);
            // Mapping is fixed; no need to display combos anymore
        } else {
            canvas_draw_str(canvas, 2, 30, "No data available");
        }
        elements_button_left(canvas, "Back");
        // Show Calib on raw page per request
        elements_button_center(canvas, "Calib");
        elements_button_right(canvas, "Next");
    }
}

static void draw_magnetometer_detail(Canvas* canvas, void* data) {
    App* app = (App*)data;
    canvas_clear(canvas);
    // No title line on compass page to free vertical space

    if(!app->widgets[WidgetTypeMagnetometer].ok) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "No data available");
    elements_button_left(canvas, "Back");
    // No Calib button on compass page
    elements_button_right(canvas, "Next");
        return;
    }
    // Page 0: Compass, Page 1: Raw values
    if(app->mag_detail_page == 0) {
    // Compass page (full circle dial centered at bottom; only upper half is visible)
        float hd = app->heading_deg_smooth;
        while(hd < 0.0f) hd += 360.0f;
        while(hd >= 360.0f) hd -= 360.0f;
        float rot = -hd * (float)M_PI / 180.0f; // dial rotation so needle stays static

        // Dial geometry: center at bottom center, radius large enough to span
        int cx = 64;
        int cy = 63; // bottom edge
    int r = 43;  // 15px smaller to avoid overlap with top numbers

    // Circle outline (only upper half will be visible on screen)
    canvas_draw_circle(canvas, cx, cy, r);

    // Draw rotating ticks around the full circle; lower half will be off-screen
    // Ticks: every 5 deg small, every 15 deg medium, every 45 deg long
    for(int d = 0; d < 360; d += 5) {
            float phi = (d * (float)M_PI / 180.0f) + rot; // rotated on dial
            int len = (d % 45 == 0) ? 6 : ((d % 15 == 0) ? 4 : 2);
            int x0 = cx + (int)roundf(sinf(phi) * (r - len));
            int y0 = cy - (int)roundf(cosf(phi) * (r - len));
            int x1 = cx + (int)roundf(sinf(phi) * r);
            int y1 = cy - (int)roundf(cosf(phi) * r);
            if(d % 5 == 0) canvas_draw_line(canvas, x0, y0, x1, y1);
        }

        // 8-wind labels N, NE, E, SE, S, SW, W, NW positioned on the dial and rotated
        const char* labels8[8] = {"N","NE","E","SE","S","SW","W","NW"};
        canvas_set_font(canvas, FontSecondary);
    int rl = r - 12; // label radius slightly inside ticks
        for(int i = 0; i < 8; i++) {
            float theta = (i * 45.0f) * (float)M_PI / 180.0f; // world angle
            float phi = theta + rot;
            int lx = cx + (int)roundf(sinf(phi) * rl);
            int ly = cy - (int)roundf(cosf(phi) * rl);
            // Only draw if within visible half (above or at center line)
            if(ly <= cy) {
                canvas_draw_str_aligned(canvas, lx, ly, AlignCenter, AlignCenter, labels8[i]);
            }
        }

        // Static needle pointing up from bottom center
        int nlen = r - 10;
        canvas_draw_line(canvas, cx, cy, cx, cy - nlen);
        // Small tip triangle or thicker head could be added; keep it simple

    // Angle and orientation combined in one centered line, e.g., "180 NE"
    int idx8 = (int)lroundf(hd / 45.0f) & 7;
    const char* otext = labels8[idx8];
    char num_only[8];
    snprintf(num_only, sizeof(num_only), "%.0f", (double)hd);
    // Measure widths in their respective fonts
    canvas_set_font(canvas, FontBigNumbers);
    int w_num = canvas_string_width(canvas, num_only);
    canvas_set_font(canvas, FontSecondary);
    char orient_with_space[6];
    snprintf(orient_with_space, sizeof(orient_with_space), " %s", otext);
    int w_txt = canvas_string_width(canvas, orient_with_space);
    int total_w = w_num + w_txt;
    int nx = (128 - total_w) / 2;
    int ny = 14; // top area
    // Draw number
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, nx, ny, num_only);
    // Draw orientation immediately after number
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, nx + w_num, ny, orient_with_space);

        // Calibration status at bottom (centered)
        char st[24];
        snprintf(st, sizeof(st), "%s", app->mag_calibrated ? "Cal:OK" : "Cal:--");
        int sw = canvas_string_width(canvas, st);
        int sx = (128 - sw) / 2;
        canvas_draw_str(canvas, sx, 62, st);

    // Compass page: no Calib button per request; keep navigation only
    elements_button_left(canvas, "Back");
    elements_button_right(canvas, "Next");
    } else {
        // Raw values page (calibration available here)
        canvas_set_font(canvas, FontSecondary);
        char buf[64];
    float rx = app->lsm_sample.mx, ry = app->lsm_sample.my, rz = app->lsm_sample.mz;
    float rmx, rmy, rmz; am_apply(app, rx, ry, rz, &rmx, &rmy, &rmz);
    if(app->sensor_swap_xy) { float t = rmx; rmx = rmy; rmy = t; }
    snprintf(buf, sizeof(buf), "Raw uT X:%+.1f Y:%+.1f Z:%+.1f", (double)rmx, (double)rmy, (double)rmz);
        canvas_draw_str(canvas, 2, 12, buf);

        // Show current min/max collected (useful during calibration)
        snprintf(buf, sizeof(buf), "Min  X:%+.1f Y:%+.1f Z:%+.1f", (double)app->mag_min_x, (double)app->mag_min_y, (double)app->mag_min_z);
        canvas_draw_str(canvas, 2, 24, buf);
        snprintf(buf, sizeof(buf), "Max  X:%+.1f Y:%+.1f Z:%+.1f", (double)app->mag_max_x, (double)app->mag_max_y, (double)app->mag_max_z);
        canvas_draw_str(canvas, 2, 36, buf);

    // Indicate calibration state only (SwapXY removed)
    snprintf(buf, sizeof(buf), "%s", app->mag_calibrated ? "Cal:OK" : "Cal:--");
        int w = canvas_string_width(canvas, buf);
    int x = (128 - w) / 2;
    canvas_draw_str(canvas, x, 50, buf);

    elements_button_left(canvas, "Back");
    elements_button_center(canvas, "Calib");
    elements_button_right(canvas, "Next");
    }
}

static void draw_temperature_detail(Canvas* canvas, void* data) {
    App* app = (App*)data;
    // Use the exact provided ice cube bitmap (67x59)
    static const uint8_t image_Ice_cube_0_bits[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf8,0x1f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x9f,0x72,0x00,0x00,0x00,0x00,0x00,0x00,0xde,0x0f,0xed,0x07,0x00,0x00,0x00,0x00,0xd0,0x77,0x22,0x77,0x05,0x00,0x00,0x00,0x00,0xff,0x6b,0xda,0xbd,0x00,0x00,0x00,0x00,0x80,0x7f,0x9e,0xfd,0xaf,0x07,0x00,0x00,0x00,0x80,0xeb,0xfb,0xbd,0x7f,0x07,0x00,0x00,0x00,0x80,0xeb,0xfb,0xbd,0x7f,0x07,0x00,0x00,0x00,0x80,0x05,0xbe,0xff,0xff,0x07,0x00,0x00,0x00,0x80,0x12,0xf0,0xfe,0xff,0x01,0x00,0x00,0x00,0x80,0x01,0x80,0xdf,0x0f,0x00,0x00,0x00,0x00,0x80,0x07,0x08,0xfe,0x00,0x00,0x00,0x00,0x00,0x80,0x69,0x04,0x19,0x8a,0x00,0x00,0x00,0x00,0x80,0x0b,0xc8,0x18,0x58,0x00,0x00,0x00,0x00,0x80,0x3b,0xd0,0x1b,0x19,0x00,0x00,0x00,0x00,0x80,0x03,0xf8,0x1a,0xa1,0x02,0x00,0x00,0x00,0x80,0x86,0x70,0x1b,0x19,0x02,0x00,0x00,0x00,0x80,0x86,0x70,0x1b,0x19,0x02,0x00,0x00,0x00,0x80,0x0f,0x28,0x1a,0xd5,0x06,0x00,0x00,0x00,0x80,0x16,0x52,0x1b,0x6b,0x05,0x00,0x00,0x00,0x80,0x47,0x36,0x1b,0xff,0x01,0x00,0x00,0x00,0x80,0xfb,0x58,0x3b,0xdc,0x03,0x00,0x00,0x00,0x80,0xce,0xc2,0x5a,0xff,0x01,0x00,0x00,0x00,0x98,0x7e,0xa5,0xd9,0xed,0x01,0x00,0x00,0xe0,0x9f,0xdc,0x6f,0x99,0xb6,0x03,0x00,0xc4,0xff,0x96,0xec,0xe6,0x1b,0xff,0x07,0x00,0xc0,0xff,0x97,0xec,0xe6,0x1b,0xff,0x07,0x00,0xf0,0xdf,0x8e,0xfe,0xdf,0x9b,0xf6,0x07,0x00,0xf8,0xff,0x87,0xe4,0xcf,0x9b,0xfb,0x07,0x00,0xf8,0xaf,0xc6,0xf0,0xc8,0x59,0xdf,0x07,0x00,0xe0,0xff,0x17,0x03,0x14,0x99,0xfe,0x01,0x00,0x02,0xff,0x1f,0x1c,0xb5,0x18,0xff,0x00,0x00,0x08,0xfc,0x7f,0xe0,0x98,0x19,0xff,0x04,0x00,0x80,0xf8,0xff,0x01,0x6f,0x19,0x21,0x07,0x00,0x02,0xfc,0xff,0x03,0x1c,0x18,0x80,0x03,0x80,0x00,0xfc,0xff,0x0f,0x18,0x18,0x80,0x03,0x40,0xc0,0xff,0xff,0x9f,0xf0,0x3f,0x7e,0x00,0x08,0xff,0xdf,0xff,0xff,0x00,0xff,0x05,0x00,0xe0,0xbf,0xee,0xff,0xff,0x03,0x3c,0x80,0x01,0xb8,0x76,0xbf,0xff,0xff,0x03,0x00,0xf0,0x07,0xfe,0xff,0xff,0xff,0xff,0x1f,0x80,0xfc,0x07,0xf7,0xff,0xff,0xff,0xff,0xff,0xe1,0xff,0x07,0xdc,0xb7,0xdb,0xfe,0xff,0xff,0xff,0xff,0x07,0xf0,0xff,0xff,0xfb,0xff,0xff,0xff,0xff,0x07,0x04,0x00,0x00,0xde,0xff,0xff,0xff,0xff,0x07,0x00,0x22,0x20,0xfe,0xff,0xff,0xff,0xff,0x07,0x00,0x01,0x04,0xf0,0xfe,0xff,0xff,0xff,0x05,0x00,0x00,0x00,0x82,0xff,0xff,0xff,0xaf,0x07,0x00,0x00,0x00,0x88,0xfe,0xff,0xff,0xf3,0x02,0x00,0x00,0x00,0xa8,0xff,0xff,0xff,0x0e,0x00,0x00,0x00,0x00,0xc0,0xff,0xff,0x6f,0x03,0x00,0x00,0x00,0x00,0xe1,0xfd,0xff,0x9f,0x00,0x00,0x00,0x00,0x00,0xc0,0xaf,0x7f,0xcd,0x20,0x00,0x00,0x00,0x00,0x80,0xff,0xd7,0x3f,0x00,0x00,0x00,0x00,0x00,0x80,0xf7,0xfd,0x3d,0x00,0x00,0x00,0x00,0x00,0x20,0xde,0xff,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x23,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x02,0x00,0x00};

    canvas_clear(canvas);

    // Ice cube
    canvas_draw_xbm(canvas, 61, 5, 67, 59, image_Ice_cube_0_bits);

    // Layer 2 - labels
    canvas_set_font(canvas, FontPrimary);
    const char* lbl_gx = "GXCAS:";
    const char* lbl_st = "ST:";
    canvas_draw_str(canvas, 0, 9, lbl_gx);
    canvas_draw_str(canvas, 56, 9, lbl_st);

    // GXCAS TEMP
    char buf[32];
    // Draw centered under label text
    if(app->th_sample.ok) snprintf(buf, sizeof(buf), "%.1f\xC2\xB0" "C", (double)app->th_sample.temperature_c);
    else snprintf(buf, sizeof(buf), "--\xC2\xB0" "C");
    int lbl_gx_w = canvas_string_width(canvas, lbl_gx);
    int gx_center_x = 0 + (lbl_gx_w / 2);
    canvas_draw_str_aligned(canvas, gx_center_x, 18, AlignCenter, AlignBottom, buf);

    // ST TEMP (LSM303)
    if(app->lsm_sample.ok && !isnan(app->lsm_sample.temp_c)) snprintf(buf, sizeof(buf), "%.1f\xC2\xB0" "C", (double)app->lsm_sample.temp_c);
    else snprintf(buf, sizeof(buf), "--\xC2\xB0" "C");
    int lbl_st_w = canvas_string_width(canvas, lbl_st);
    int st_center_x = 56 + (lbl_st_w / 2);
    canvas_draw_str_aligned(canvas, st_center_x, 18, AlignCenter, AlignBottom, buf);

    // AVERAGE TEMP
    float t1 = app->th_sample.ok ? app->th_sample.temperature_c : NAN;
    float t2 = (app->lsm_sample.ok && !isnan(app->lsm_sample.temp_c)) ? app->lsm_sample.temp_c : NAN;
    float avg = NAN;
    if(!isnan(t1) && !isnan(t2)) avg = 0.5f * (t1 + t2);
    else if(!isnan(t1)) avg = t1;
    else if(!isnan(t2)) avg = t2;

    // Center the average under the GXCAS label with degree symbol to the right
    canvas_set_font(canvas, FontBigNumbers);
    int avg_x_start = 9; // default fallback start
    int num_w = 0;
    if(!isnan(avg)) {
        char nbuf[16];
        // Keep BigNumbers for digits (no decimal point in this font on some builds), use integer rounding
        snprintf(nbuf, sizeof(nbuf), "%.0f", (double)avg);
        num_w = canvas_string_width(canvas, nbuf);
        canvas_set_font(canvas, FontSecondary);
        const char* deg = "\xC2\xB0" "C";
        int deg_w = canvas_string_width(canvas, deg);
        int total_w = num_w + deg_w;
        // center relative to GXCAS label center
        avg_x_start = gx_center_x - (total_w / 2);
        // Draw number
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, avg_x_start, 39, nbuf);
        // Draw degree at the end
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, avg_x_start + num_w, 39, deg);
    } else {
        // Center fallback
        canvas_set_font(canvas, FontSecondary);
        const char* nodata = "--\xC2\xB0" "C";
        int w = canvas_string_width(canvas, nodata);
        avg_x_start = gx_center_x - (w / 2);
        canvas_draw_str(canvas, avg_x_start, 39, nodata);
        num_w = w; // for centering "approx." below
    }

    // Layer 7 - approx note
    canvas_set_font(canvas, FontSecondary);
    const char* approx = "approx.";
    int approx_w = canvas_string_width(canvas, approx);
    int approx_x = avg_x_start + (num_w / 2) - (approx_w / 2);
    canvas_draw_str(canvas, approx_x, 46, approx);

    // OS back button
    elements_button_left(canvas, "Back");
}

static void draw_humidity_detail(Canvas* canvas, void* data) {
    // Adopt the provided icon-heavy UI for humidity details
    Gxhtc3cSample* sample = (Gxhtc3cSample*)data;
    canvas_clear(canvas);

    // Icon bitmap (9x14)
    static const uint8_t image_icon_0_bits[] = {0x10,0x00,0x38,0x00,0x38,0x00,0x74,0x00,0x74,0x00,0xfa,0x00,0xfa,0x00,0xfd,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xfe,0x00,0xfe,0x00,0x38,0x00};

    // Icons scattered around (as per provided coordinates)
    canvas_draw_xbm(canvas, 114, 35, 9, 14, image_icon_0_bits);
    canvas_draw_xbm(canvas, 29, 13, 9, 14, image_icon_0_bits);
    canvas_draw_xbm(canvas, 87, 37, 9, 14, image_icon_0_bits);
    canvas_draw_xbm(canvas, 52, 4, 9, 14, image_icon_0_bits);
    canvas_draw_xbm(canvas, 10, 6, 9, 14, image_icon_0_bits);
    canvas_draw_xbm(canvas, 84, 7, 9, 14, image_icon_0_bits);
    canvas_draw_xbm(canvas, 13, 30, 9, 14, image_icon_0_bits);
    canvas_draw_xbm(canvas, 32, 37, 9, 14, image_icon_0_bits);
    canvas_draw_xbm(canvas, 100, 21, 9, 14, image_icon_0_bits);
    canvas_draw_xbm(canvas, 113, 5, 9, 14, image_icon_0_bits);

    // Big number layer at fixed position (46,39). Use live humidity if available.
    char numbuf[8];
    if(sample->ok) snprintf(numbuf, sizeof(numbuf), "%.0f", (double)sample->humidity_rh);
    else snprintf(numbuf, sizeof(numbuf), "--");
    canvas_set_font(canvas, FontBigNumbers);
    int wnum = canvas_string_width(canvas, numbuf);
    int nx = 46; int ny = 39;
    canvas_draw_str(canvas, nx, ny, numbuf);
    // Append a small '%' immediately after the number to mimic "99%"
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, nx + wnum, ny, "%");

    // OS back button
    elements_button_left(canvas, "Back");
}

static void draw_light_detail(Canvas* canvas, void* data) {
    // Background art for light sensor page
    static const uint8_t image_pixil_frame_0__1__1_0_bits[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x02,0xe0,0x01,0x34,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xe0,0x01,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xe0,0x01,0x1a,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x08,0x1c,0xe0,0x01,0x0c,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x1c,0xe0,0x01,0x0e,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x3c,0xe0,0x01,0x0f,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x78,0x80,0x3c,0xe0,0x13,0x0f,0x80,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0x21,0xf8,0xf0,0xe3,0x07,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x03,0xf8,0xf1,0xeb,0x07,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xfc,0xf1,0xe3,0x07,0x3c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xfc,0xf1,0xe3,0x07,0x3c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0xf0,0xff,0xff,0x0b,0x3e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x3e,0xf0,0x01,0xe0,0x03,0x5f,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7c,0x30,0xfe,0x1f,0x83,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0xf8,0xc9,0xff,0xff,0xe6,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0xf7,0xff,0xff,0xf9,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0xf7,0xff,0xff,0xf9,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0xfb,0xff,0xff,0xf7,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0xfd,0xff,0xff,0xef,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0x00,0xc0,0xfe,0xff,0xff,0xdf,0x00,0x80,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x07,0x40,0xff,0xff,0xff,0xbf,0x00,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfe,0xa1,0xff,0xff,0xff,0x7f,0xe1,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0xfc,0xdf,0xff,0xff,0xff,0xff,0xfe,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0xc0,0xef,0xff,0xff,0xff,0xff,0xfd,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xef,0xff,0xff,0xff,0xff,0x7d,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0xee,0xff,0xff,0xff,0xff,0x1d,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf4,0xff,0xff,0xff,0xff,0xab,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf4,0xff,0xff,0xff,0xff,0x0b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf4,0xff,0xff,0xff,0xff,0x0b,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf5,0xff,0xff,0xff,0xff,0x0b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xff,0xf7,0xff,0xff,0xff,0xff,0xfb,0x7f,0x00,0x00,0x00,0x00,0x00,0x00,0xf8,0xff,0xff,0xf7,0xff,0xff,0xff,0xff,0xfb,0xff,0xff,0x07,0x00,0x00,0x00,0x00,0xf8,0xff,0xff,0xf7,0xff,0xff,0xff,0xff,0xfb,0xff,0xff,0x07,0x00,0x00,0x00,0x00,0xf8,0xff,0xff,0xf7,0xff,0xff,0xff,0xff,0xfb,0xff,0xff,0x07,0x00,0x00,0x00,0x00,0x00,0x82,0xff,0xf7,0xff,0xff,0xff,0xff,0xfb,0x7f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf4,0xff,0xff,0xff,0xff,0x0b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0xf4,0xff,0xff,0xff,0xff,0x0b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0xf4,0xff,0xff,0xff,0xff,0x0b,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf4,0xff,0xff,0xff,0xff,0x0b,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x81,0xee,0xff,0xff,0xff,0xff,0x39,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xef,0xff,0xff,0xff,0xff,0x7d,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0xdf,0xff,0xff,0xff,0xff,0xfe,0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfc,0xbf,0xff,0xff,0xff,0x7f,0xff,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfe,0xa1,0xff,0xff,0xff,0x7f,0xe1,0x1f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x07,0x42,0xff,0xff,0xff,0xbf,0x04,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x01,0xc0,0xfe,0xff,0xff,0xdf,0x00,0x80,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0xfd,0xff,0xff,0xef,0x01,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0xfb,0xff,0xff,0xf7,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0xe7,0xff,0xff,0xf9,0x03,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0xe7,0xff,0xff,0xf9,0x03,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xfa,0xd9,0xff,0x7f,0xe6,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x7d,0x30,0xfe,0x8f,0x83,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7e,0xf1,0x01,0xf0,0x03,0x1f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x1f,0xf0,0xff,0xff,0x03,0x3e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xf8,0xf1,0xe3,0x07,0x3c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xf8,0xf1,0xe3,0x07,0x3c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x43,0xf8,0xf5,0xe3,0x07,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x00,0x78,0xf0,0xab,0x07,0xc0,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x78,0x80,0x3c,0xe0,0x01,0x0f,0x80,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x3c,0xe0,0x01,0x0f,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x1d,0xe0,0x21,0x0e,0x02,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x40,0x0c,0xe0,0x05,0x0c,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xe0,0x01,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0xe0,0x01,0x38,0x00,0x00,0x00,0x00,0x00,0x00};

    KiisuLightAdcSample* sample = (KiisuLightAdcSample*)data;

    // Draw background image (explicitly in black)
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, 0, 0, 128, 64, image_pixil_frame_0__1__1_0_bits);

    // Overlay live values using XOR so they show over any background
    char buf[32];
    canvas_set_color(canvas, ColorXOR);

    if(sample->ok) {
        // Percentage (replace "70%") - centered
        canvas_set_font(canvas, FontPrimary);
        snprintf(buf, sizeof(buf), "%.0f%%", (double)sample->percent);
        int w = canvas_string_width(canvas, buf);
        int x = (128 - w) / 2;
        canvas_draw_str(canvas, x, 28, buf);

        // Voltage mV (replace "8888mV") - centered
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "%.0fmV", (double)sample->voltage_mv);
        w = canvas_string_width(canvas, buf);
        x = (128 - w) / 2;
        canvas_draw_str(canvas, x, 38, buf);
    } else {
        // Centered fallback
        canvas_set_font(canvas, FontPrimary);
        const char* no_data_line1 = "--% (no data)";
        int w1 = canvas_string_width(canvas, no_data_line1);
        int x1 = (128 - w1) / 2;
        canvas_draw_str(canvas, x1, 28, no_data_line1);

        canvas_set_font(canvas, FontSecondary);
        const char* no_data_line2 = "---- mV";
        int w2 = canvas_string_width(canvas, no_data_line2);
        int x2 = (128 - w2) / 2;
        canvas_draw_str(canvas, x2, 38, no_data_line2);
    }

    // Restore default draw color
    canvas_set_color(canvas, ColorBlack);

    // Keep OS back button
    elements_button_left(canvas, "Back");
}

// Main draw callback
static void draw_cb(Canvas* canvas, void* ctx) {
    App* app = ctx;

    if(app->state == AppStateWidgetMenu) {
        // Use provided iPhone-like widget layout with icons and rounded frames
        canvas_clear(canvas);

        // --- Icon bitmaps (monochrome XBM data) ---
    // Thermometer icon for the tall temperature box (11x37)
    static const uint8_t image_Thermometer_0_bits[] = {0xf8,0x00,0x8c,0x01,0x04,0x01,0x1c,0x01,0x04,0x01,0x04,0x01,0x1c,0x01,0x04,0x01,0xfc,0x01,0xe4,0x01,0xfc,0x01,0xfc,0x01,0xe4,0x01,0xfc,0x01,0xfc,0x01,0xe4,0x01,0xfc,0x01,0xfc,0x01,0xe4,0x01,0xfc,0x01,0xfc,0x01,0xe4,0x01,0xfc,0x01,0xfc,0x01,0xe4,0x01,0xfc,0x01,0xfc,0x01,0xfc,0x01,0xfe,0x03,0xff,0x07,0xff,0x07,0xff,0x07,0xff,0x07,0xff,0x07,0xfe,0x03,0xfc,0x01,0xf8,0x00};
        static const uint8_t image_icon_1_bits[] = {0x01,0x00,0x07,0x00,0x19,0x00,0xe7,0xff,0x1f,0x80,0xff,0xff,0xff,0xff,0xff,0xff,0x1f,0x00,0x07,0x00,0x01,0x00};
        static const uint8_t image_icon_2_bits[] = {0x10,0x00,0x38,0x00,0x38,0x00,0x74,0x00,0x74,0x00,0xfa,0x00,0xfa,0x00,0xfd,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xfe,0x00,0xfe,0x00,0x38,0x00};
        static const uint8_t image_icon_3_bits[] = {0x07,0x00,0x00,0x07,0x00,0x00,0x07,0x00,0x00,0x07,0x00,0x00,0x07,0x00,0x00,0x07,0x00,0x00,0x07,0x00,0x00,0x07};
    // Compass icon for the magnetometer box (16x15)
    static const uint8_t image_Compass_0_bits[] = {0xe0,0x03,0x58,0x0f,0x64,0x1f,0x3a,0x3e,0x3a,0x3e,0x3d,0x7e,0x3d,0x7e,0xbd,0x7e,0xff,0x7f,0x3f,0x7e,0xbe,0x3f,0x3e,0x3e,0xfc,0x1e,0x38,0x0e,0xe0,0x03};
    // Removed extra overlay layers that overlapped the compass and caused artifacts

        // Draw main frames (clock and widgets)
        // Clock
        canvas_draw_rframe(canvas, 2, 2, 55, 25, 3);

        // Temperature (tall right column)
        canvas_draw_rframe(canvas, 101, 2, 24, 60, 3);

        // Light button (left bottom)
        canvas_draw_rframe(canvas, 2, 29, 31, 33, 3);

    // Humidity button (bottom row, center-left)
        canvas_draw_rframe(canvas, 35, 29, 31, 33, 3);

        // Magnet button (bottom row, center-right)
        canvas_draw_rframe(canvas, 68, 29, 31, 33, 3);

        // Accel button (top center)
        canvas_draw_rframe(canvas, 59, 2, 40, 25, 3);

    // Icons (positions from provided UI)
    canvas_draw_xbm(canvas, 108, 21, 11, 37, image_Thermometer_0_bits);
    canvas_draw_xbm(canvas, 76, 44, 16, 15, image_Compass_0_bits);
        canvas_draw_xbm(canvas, 10, 47, 16, 11, image_icon_1_bits);
        canvas_draw_xbm(canvas, 46, 45, 9, 14, image_icon_2_bits);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_xbm(canvas, 110, 24, 3, 22, image_icon_3_bits);

    // Misc layers removed to avoid overlapping the compass icon

    // Time/date from RTC
    canvas_set_color(canvas, ColorBlack);
    DateTime furi_time;
    furi_hal_rtc_get_datetime(&furi_time);
    canvas_set_font(canvas, FontPrimary);
    // Format time HH:MM and center inside clock frame (x=2,width=55)
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02u:%02u", furi_time.hour, furi_time.minute);
    int tw = canvas_string_width(canvas, time_str);
    int tx = 2 + ((55 - tw) / 2);
    canvas_draw_str(canvas, tx, 13, time_str);

    canvas_set_font(canvas, FontSecondary);
    // Format date DD/MM/YY and center inside clock frame
    char date_str[16];
    snprintf(date_str, sizeof(date_str), "%02u/%02u/%02u", furi_time.day, furi_time.month, furi_time.year % 100);
    int dw = canvas_string_width(canvas, date_str);
    int dx = 2 + ((55 - dw) / 2);
    canvas_draw_str(canvas, dx, 23, date_str);

    // Sensor text: use actual samples where available
        char buf[32];
        // Temperature in the tall right box
        canvas_set_font(canvas, FontSecondary);
        if(app->th_sample.ok) {
            snprintf(buf, sizeof(buf), "%.0fC", (double)app->th_sample.temperature_c);
        } else snprintf(buf, sizeof(buf), "--C");
        // Center temp inside temp frame (x=101,width=24)
        {
            int temp_w = canvas_string_width(canvas, buf);
            int temp_x = 101 + ((24 - temp_w) / 2);
            canvas_draw_str(canvas, temp_x, 16, buf);
        }

        // Light percentage (left bottom)
        if(app->light_sample.ok) {
            snprintf(buf, sizeof(buf), "%.0f%%", (double)app->light_sample.percent);
        } else {
            snprintf(buf, sizeof(buf), "--");
        }
        {
            int lw = canvas_string_width(canvas, buf);
            int lx = 2 + ((31 - lw) / 2);
            canvas_draw_str(canvas, lx, 42, buf);
        }

        // Humidity (bottom row, center-left)
        if(app->th_sample.ok) {
            snprintf(buf, sizeof(buf), "%.0f%%", (double)app->th_sample.humidity_rh);
        } else {
            snprintf(buf, sizeof(buf), "--");
        }
        {
            int hw = canvas_string_width(canvas, buf);
            int hx = 35 + ((31 - hw) / 2);
            canvas_draw_str(canvas, hx, 42, buf);
        }

        // Wind direction (bottom row, center-right) based on smoothed compass heading
        if(app->widgets[WidgetTypeMagnetometer].ok) {
            float hd = app->heading_deg_smooth;
            while(hd < 0.0f) hd += 360.0f;
            while(hd >= 360.0f) hd -= 360.0f;
            static const char* labels8[8] = {"N","NE","E","SE","S","SW","W","NW"};
            int idx = (int)lroundf(hd / 45.0f) & 7;
            const char* dir = labels8[idx];
            snprintf(buf, sizeof(buf), "%s", dir);
        } else {
            snprintf(buf, sizeof(buf), "--");
        }
        {
            int mw = canvas_string_width(canvas, buf);
            int mx_pos = 68 + ((31 - mw) / 2);
            canvas_draw_str(canvas, mx_pos, 42, buf);
        }

    // No extra label above clock (we display time/date directly)

        // --- Homescreen kitty inside Accelerometer box ---
        if(app->home_has_kitty) {
            // Physics integration using accelerometer mapping (similar to detail view, lighter)
            uint32_t now = furi_get_tick();
            float dt = (now - app->home_last_tick) / 1000.0f;
            if(dt < 0.0f) dt = 0.0f;
            if(dt > 0.05f) dt = 0.05f;
            app->home_last_tick = now;

            float ax = 0.0f, ay = 0.0f;
            if(app->lsm_sample.ok) {
                float saxr = app->lsm_sample.ax, sayr = app->lsm_sample.ay, sazr = app->lsm_sample.az;
                if(app->sensor_swap_xy){ float t=saxr; saxr=sayr; sayr=t; }
                float sax, say, saz; am_apply(app, saxr, sayr, sazr, &sax, &say, &saz);
                if(app->leveled) { sax -= app->level_ax; say -= app->level_ay; }
                float rx = sax, ry = say;
                switch(app->orientation_rot & 3){
                    case 1: { float tx=rx; rx=ry; ry=-tx; } break;
                    case 2: { rx=-rx; ry=-ry; } break;
                    case 3: { float tx=rx; rx=-ry; ry=tx; } break;
                    default: break;
                }
                const float gscale = 80.0f;
                ax = rx * gscale;
                ay = ry * gscale;
                float swirl = app->yaw_rate_dps * 0.03f;
                ax += -ry * swirl * 0.2f;
                ay +=  rx * swirl * 0.2f;
            }

            // Integrate
            app->home_vx += ax * dt; app->home_vy += ay * dt;
            app->home_vx *= 0.995f;  app->home_vy *= 0.995f;
            if(fabsf(ax) < 3.0f && fabsf(ay) < 3.0f){
                if(fabsf(app->home_vx) < 3.0f) app->home_vx *= 0.9f;
                if(fabsf(app->home_vy) < 3.0f) app->home_vy *= 0.9f;
            }
            app->home_x += app->home_vx * dt; app->home_y += app->home_vy * dt;
            float yaw_spin = app->yaw_rate_dps * (float)M_PI / 180.0f * 0.5f;
            app->home_ang += (app->home_av + yaw_spin) * dt;

            // Collide with accelerometer box walls: x=59,y=2,w=40,h=25
            const float bx = 59.0f, by = 2.0f, bw = 40.0f, bh = 25.0f;
            const float minx = bx + app->home_r;
            const float maxx = bx + bw - app->home_r;
            const float miny = by + app->home_r;
            const float maxy = by + bh - app->home_r;
            const float e = 0.75f;
            if(app->home_x < minx){ app->home_x = minx; app->home_vx = -app->home_vx * e; }
            if(app->home_x > maxx){ app->home_x = maxx; app->home_vx = -app->home_vx * e; }
            if(app->home_y < miny){ app->home_y = miny; app->home_vy = -app->home_vy * e; }
            if(app->home_y > maxy){ app->home_y = maxy; app->home_vy = -app->home_vy * e; }

            // Draw the kitty bitmap centered at (home_x, home_y)
            static const uint8_t image_kitty_0_bits[] = {0x01,0x08,0x03,0x0c,0x07,0x0e,0x0f,0x0f,0xff,0x0f,0xff,0x0f,0xf3,0x0c,0xf3,0x0c,0xf3,0x0c,0xff,0x0f,0x0f,0x0f,0x9f,0x0f,0xff,0x0f,0xfe,0x07,0xfc,0x03};
            canvas_draw_xbm_rotated(
                canvas,
                (int)roundf(app->home_x),
                (int)roundf(app->home_y),
                16,
                15,
                image_kitty_0_bits,
                app->home_ang);
        }

        // Highlight selected widget by drawing another rounded frame
        // Draw an expanded frame: x-1, y-1, w+2, h+2 with roundness 3
        switch(app->selected_widget) {
            case WidgetTypeAccelerometer: {
                int x = 59, y = 2, w = 40, h = 25;
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rframe(canvas, x - 1, y - 1, w + 2, h + 2, 3);
            } break;
            case WidgetTypeMagnetometer: {
                int x = 68, y = 29, w = 31, h = 33;
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rframe(canvas, x - 1, y - 1, w + 2, h + 2, 3);
            } break;
            case WidgetTypeTemperature: {
                int tx = 101, ty = 2, tw = 24, th = 60; // temp tall column
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rframe(canvas, tx - 1, ty - 1, tw + 2, th + 2, 3);
            } break;
            case WidgetTypeHumidity: {
                int hx = 35, hy = 29, hw = 31, hh = 33; // humidity box
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rframe(canvas, hx - 1, hy - 1, hw + 2, hh + 2, 3);
            } break;
            case WidgetTypeLight: {
                int x = 2, y = 29, w = 31, h = 33;
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rframe(canvas, x - 1, y - 1, w + 2, h + 2, 3);
            } break;
            default:
                break;
        }
    } else if(app->state == AppStateWidgetDetail) {
        // Draw detailed view for selected widget
        if(app->selected_widget >= 0 && app->selected_widget < WidgetTypeCount) {
            if(app->widgets[app->selected_widget].draw_detail) {
                app->widgets[app->selected_widget].draw_detail(canvas, app->widgets[app->selected_widget].data);
            }
        }
    } else if(app->state == AppStateCalibrating) {
        draw_calibrating_screen(canvas, app);
    }
}

// Input callback
static void input_cb(InputEvent* e, void* ctx) {
    App* app = ctx;
    furi_message_queue_put(app->input_queue, e, 0);
}


// Apply mapping: given raw (x,y,z) -> mapped (xo,yo,zo)
static void am_apply(const App* app, float x, float y, float z, float* xo, float* yo, float* zo){
    UNUSED(app);
    // Fixed +Z rotation mapping: X' = +Y, Y' = -X, Z' = +Z
    *xo = y;
    *yo = -x;
    *zo = z;
}

// App initialization
static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    
    // Initialize GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->vp = view_port_alloc();
    view_port_draw_callback_set(app->vp, draw_cb, app);
    view_port_input_callback_set(app->vp, input_cb, app);
    gui_add_view_port(app->gui, app->vp, GuiLayerFullscreen);
    
    // Initialize input queue
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    
    // Initialize sensors
    i2c_bus_init(&app->i2c, &furi_hal_i2c_handle_external, 50, false);
    lsm303_init(&app->lsm, &app->i2c, false);
    gxhtc3c_init(&app->th, &app->i2c, false);
    kiisu_light_adc_init();
    
    // Initialize sensor data
    app->lsm_sample.ok = false;
    app->th_sample.ok = false;
    app->light_sample.ok = false;
    app->light_sample.voltage_mv = 0.0f;
    app->light_sample.percent = 0.0f;
    
    // Initialize UI state
    app->state = AppStateWidgetMenu;
    app->selected_widget = 0;
    app->orientation_rot = 0;
    app->sensor_swap_xy = false; // use explicit mapping below instead of legacy swap
    app->mag_detail_page = 0;
    app->heading_deg = 0.0f;
    app->heading_deg_smooth = 0.0f;
    app->head_vec_x = 0.0f; // points North initially
    app->head_vec_y = 1.0f;
    app->mag_declination_deg = 0.0f;
    app->heading_tilt_limit_deg = 60.0f;
    app->compass_snapped = false;
    app->compass_snap_idx = -1;
    app->acc_pitch = 0.0f;
    app->acc_roll = 0.0f;
    app->yaw_rate_dps = 0.0f;
    app->pitch_rate_dps = 0.0f;
    app->roll_rate_dps = 0.0f;
    app->last_yaw_deg = 0.0f;
    app->last_pitch = 0.0f;
    app->last_roll = 0.0f;
    app->last_orient_tick = furi_get_tick();

    // Initialize calibration data
    app->mag_min_x = app->mag_min_y = app->mag_min_z = 9999;
    app->mag_max_x = app->mag_max_y = app->mag_max_z = -9999;
    app->mag_calibrated = false;
    app->mag_hw_offsets_applied = false;
    app->mag_offs[0] = app->mag_offs[1] = app->mag_offs[2] = 0.0f;
    app->mag_scale[0] = app->mag_scale[1] = app->mag_scale[2] = 1.0f;
    app->mag_filt[0] = app->mag_filt[1] = app->mag_filt[2] = 0.0f;
    app->mag_iir_alpha = 0.2f; // ~ light smoothing; adjust 0.1..0.3 based on responsiveness
    app->leveled = false;
    app->level_ax = app->level_ay = 0.0f; app->level_az = 1.0f;
    
    // Initialize polling timing
    app->last_lsm_poll_ms = 0;
    app->last_th_poll_ms = 0;
    app->last_light_poll_ms = 0;

    // Initialize Little Kiisu physics state (start centered in accel detail area)
    app->img_x = 64.0f; // legacy
    app->img_y = 32.0f;
    app->img_vx = 0.0f;
    app->img_vy = 0.0f;
    app->img_angle = 0.0f;
    app->img_av = 0.0f;
    app->accel_detail_page = 0;
    app->sim_count = 5;
    app->sim_w = 16; // kitty bitmap is 16x15 (from user snippet); height 15
    app->sim_h = 15;
    app->sim_r = 8.0f; // simple circle radius
    // Seed 5 sprites around a loose ring with varied velocities and spins
    for(int i=0;i<5;i++){
        float ang = (float)i * 6.2831853f / 5.0f; // 2*pi/5
        float rx = 26.0f + 3.0f * (float)((i*37)%5); // slight radius variation
        float ry = 14.0f + 2.0f * (float)((i*53)%3);
        app->sim_x[i] = 64.0f + cosf(ang) * rx;
        app->sim_y[i] = 32.0f + sinf(ang) * ry;
        // varied initial velocity roughly tangential to the ring
        float tangx = -sinf(ang), tangy = cosf(ang);
        float vmag = 10.0f + (float)((i*17)%7);
        app->sim_vx[i] = tangx * (vmag * 0.2f);
        app->sim_vy[i] = tangy * (vmag * 0.2f);
        app->sim_ang[i] = ang * 0.3f;
        app->sim_av[i] = ((i%2)?1.0f:-1.0f) * (0.6f + 0.1f * (float)((i*29)%3));
    }
    app->last_physics_tick = furi_get_tick();
    // Homescreen kitty
    app->home_has_kitty = true;
    // Accelerometer widget box: x=59,y=2,w=40,h=25; keep sprite fully inside
    app->home_r = 7.5f; // close to half of 16x15 sprite
    float box_x = 59.0f, box_y = 2.0f, box_w = 40.0f, box_h = 25.0f;
    app->home_x = box_x + box_w * 0.5f;
    app->home_y = box_y + box_h * 0.5f;
    app->home_vx = 0.0f;
    app->home_vy = 0.0f;
    app->home_ang = 0.0f;
    app->home_av = 0.0f;
    app->home_last_tick = furi_get_tick();
    // Axis mapping hard-locked via am_apply()
    
    // Notifications
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    // Level averaging accumulators
    app->level_sum_ax = app->level_sum_ay = 0.0f;
    app->level_sum_az = 0.0f;
    app->level_samples = 0;

    // Initialize widgets
    init_widgets(app);
    
    return app;
}

// App cleanup
static void app_free(App* app) {
    view_port_enabled_set(app->vp, false);
    gui_remove_view_port(app->gui, app->vp);
    view_port_free(app->vp);
    furi_message_queue_free(app->input_queue);
    furi_record_close(RECORD_GUI);
    if(app->notification) furi_record_close(RECORD_NOTIFICATION);
    free(app);
}

// Main app function
int32_t kiisu_sensor_hub_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    const uint32_t poll_period_ms = 100;
    bool running = true;
    
    while(running) {
        InputEvent e;
    if(furi_message_queue_get(app->input_queue, &e, poll_period_ms) == FuriStatusOk) {
            if(e.type == InputTypeShort) {
                if(app->state == AppStateWidgetMenu) {
                    switch(e.key) {
                        case InputKeyBack:
                            running = false;
                            break;
                        case InputKeyUp:
                            app->selected_widget = NAV_UP[app->selected_widget];
                            break;
                        case InputKeyDown:
                            app->selected_widget = NAV_DOWN[app->selected_widget];
                            break;
                        case InputKeyLeft:
                            app->selected_widget = NAV_LEFT[app->selected_widget];
                            break;
                        case InputKeyRight:
                            app->selected_widget = NAV_RIGHT[app->selected_widget];
                            break;
                        case InputKeyOk:
                            app->state = AppStateWidgetDetail;
                            if(app->selected_widget == WidgetTypeMagnetometer) {
                                app->mag_detail_page = 0; // default to Compass page on entry
                            }
                            break;
                        default:
                            break;
                    }
                } else if(app->state == AppStateWidgetDetail) {
                    switch(e.key) {
                        case InputKeyBack:
                            app->state = AppStateWidgetMenu;
                            break;
                        case InputKeyOk:
                            if(app->selected_widget == WidgetTypeMagnetometer && app->mag_detail_page == 1) {
                                // Start flow at intro
                                app->calib_step = 0; // Flat template (wait)
                                app->calib_duration_ms = 3000; // 3s average for flat capture
                                app->calib_mode = CalibModeMagnetometer;
                                // Clear previous mag calibration range ready for step 1
                                app->mag_min_x = app->mag_min_y = app->mag_min_z = 9999;
                                app->mag_max_x = app->mag_max_y = app->mag_max_z = -9999;
                                app->mag_calibrated = false;
                                // Ensure hardware offsets cleared before new calibration
                                lsm303_set_hardiron_offsets_uT(&app->lsm, 0.0f, 0.0f, 0.0f);
                                app->mag_hw_offsets_applied = false;
                                app->state = AppStateCalibrating;
                            } else if(app->selected_widget == WidgetTypeAccelerometer && app->accel_detail_page == 2) {
                                // Start shared flow for accelerometer leveling (reuses flat averaging step)
                                app->calib_step = 0; // Flat template (wait)
                                app->calib_duration_ms = 3000; // 3s average for flat capture
                                app->calib_mode = CalibModeAccelerometer;
                                app->state = AppStateCalibrating;
                            }
                            break;
                        case InputKeyRight:
                            // Use Right as "Next" on detail pages for paging when available
                            if(app->selected_widget == WidgetTypeMagnetometer) {
                                app->mag_detail_page = (app->mag_detail_page + 1) & 1;
                            } else if(app->selected_widget == WidgetTypeAccelerometer) {
                                // Order: 0 = Kitties, 1 = Level, 2 = Raw
                                app->accel_detail_page = (app->accel_detail_page + 1) % 3;
                            }
                            break;
                        // Up/Down no longer change mapping
                        
                        // No Up/Down/Right actions in magnetometer detail for heading anymore
                        default:
                            break;
                    }
                } else if(app->state == AppStateCalibrating) {
                    if(e.key == InputKeyBack) {
                        // Exit calibration and return to originating detail page
                        if(app->calib_mode == CalibModeMagnetometer) {
                            app->mag_detail_page = 0;
                        }
                        app->state = AppStateWidgetDetail;
                    } else if(e.key == InputKeyOk) {
                        if(app->calib_step == 0) {
                            // Start averaging on flat template
                            app->level_sum_ax = app->level_sum_ay = 0.0f;
                            app->level_sum_az = 0.0f;
                            app->level_samples = 0;
                            app->calibration_start_time = furi_get_tick();
                            app->calib_duration_ms = 3000; // 3 seconds averaging
                            app->calib_step = 1; // averaging phase on flat template
                        } else if(app->calib_step == 2) {
                            // Start figure-eight run
                            app->calibration_start_time = furi_get_tick();
                            app->calib_duration_ms = 10000; // 10s
                            app->calib_step = 3; // fig8 run
                        } else if(app->calib_step == 3) {
                            // Early finish of figure-eight -> compute
                            app->calib_step = 4;
                        }
                    }
                }
            }
        }
    // No mapping long-press handling anymore
        
        uint32_t now = furi_get_tick();

        // Calibration state handling
    if(app->state == AppStateCalibrating) {
        if(app->calib_step == 1) {
                // Averaging flat accel
                uint32_t elapsed = now - app->calibration_start_time;
                if(app->lsm_sample.ok) {
                    app->level_sum_ax += app->lsm_sample.ax;
                    app->level_sum_ay += app->lsm_sample.ay;
                    app->level_sum_az += app->lsm_sample.az;
                    app->level_samples++;
                }
                if(elapsed >= app->calib_duration_ms) {
                    // Compute baseline and confirm with a short beep
                    if(app->level_samples > 0) {
                        app->level_ax = app->level_sum_ax / (float)app->level_samples;
                        app->level_ay = app->level_sum_ay / (float)app->level_samples;
                        app->level_az = app->level_sum_az / (float)app->level_samples;
                        app->leveled = true;
                    }
                    if(app->notification) notification_message(app->notification, &sequence_semi_success);
                    if(app->calib_mode == CalibModeMagnetometer) {
                        // For magnetometer, proceed to figure-eight step
                        app->calib_duration_ms = 10000; // plan 10s for fig8 and display it on next page
                        app->calib_step = 2; // switch to fig8 wait template
                    } else {
                        // For accelerometer, we're done after flat average
                        app->calib_step = 5; // completion
                    }
                }
        } else if(app->calib_step == 3) {
                // Figure-eight timed sweep
                uint32_t elapsed = now - app->calibration_start_time;
                if(elapsed >= app->calib_duration_ms) {
            app->calib_step = 4; // compute
                }
        } else if(app->calib_step == 4) {
                // Compute calibration from collected min/max, beep, and finish
                float span_x = (app->mag_max_x - app->mag_min_x) * 0.5f;
                float span_y = (app->mag_max_y - app->mag_min_y) * 0.5f;
                float span_z = (app->mag_max_z - app->mag_min_z) * 0.5f;
                if(span_x <= 1e-6f) span_x = 1.0f;
                if(span_y <= 1e-6f) span_y = 1.0f;
                if(span_z <= 1e-6f) span_z = 1.0f;
                float off_x = (app->mag_max_x + app->mag_min_x) * 0.5f;
                float off_y = (app->mag_max_y + app->mag_min_y) * 0.5f;
                float off_z = (app->mag_max_z + app->mag_min_z) * 0.5f;

                float gmean = cbrtf(span_x * span_y * span_z);
                app->mag_offs[0] = off_x;
                app->mag_offs[1] = off_y;
                app->mag_offs[2] = off_z;
                app->mag_scale[0] = (span_x > 0.0f) ? (gmean / span_x) : 1.0f;
                app->mag_scale[1] = (span_y > 0.0f) ? (gmean / span_y) : 1.0f;
                app->mag_scale[2] = (span_z > 0.0f) ? (gmean / span_z) : 1.0f;
                app->mag_calibrated = true;

                // Program hard-iron into sensor
                lsm303_set_hardiron_offsets_uT(&app->lsm, off_x, off_y, off_z);
                app->mag_hw_offsets_applied = true;
                if(app->notification) {
                    notification_message(app->notification, &sequence_success);
                    notification_message(app->notification, &sequence_blink_cyan_100);
                }
                app->calib_step = 5; // complete screen
            }
        }
        
        // Poll sensors
        Lsm303Sample lsm_out;
        if(now - app->last_lsm_poll_ms >= 100) {
            if(lsm303_poll(&app->lsm, &lsm_out)) {
                app->lsm_sample = lsm_out;
                app->widgets[WidgetTypeAccelerometer].ok = lsm_out.ok;
                app->widgets[WidgetTypeMagnetometer].ok = lsm_out.ok;

                // Update compass data after polling
                if(lsm_out.ok) {
                    // During figure-eight active step, collect min/max
                    if(app->state == AppStateCalibrating && app->calib_step == 3) {
                        float mx = app->lsm_sample.mx;
                        float my = app->lsm_sample.my;
                        float mz = app->lsm_sample.mz;
                        if(mx < app->mag_min_x) app->mag_min_x = mx;
                        if(mx > app->mag_max_x) app->mag_max_x = mx;
                        if(my < app->mag_min_y) app->mag_min_y = my;
                        if(my > app->mag_max_y) app->mag_max_y = my;
                        if(mz < app->mag_min_z) app->mag_min_z = mz;
                        if(mz > app->mag_max_z) app->mag_max_z = mz;
                    }
                    update_compass(app);
                    // Update pitch/roll from accel
                    float axr = app->lsm_sample.ax;
                    float ayr = app->lsm_sample.ay;
                    float azr = app->lsm_sample.az;
                    if(app->sensor_swap_xy) { float t=axr; axr=ayr; ayr=t; }
                    float ax, ay, az; am_apply(app, axr, ayr, azr, &ax, &ay, &az);
                    float roll = atan2f(ay, az == 0.0f ? 1e-6f : az);
                    float denom = sqrtf(ay*ay + az*az);
                    if(denom < 1e-6f) denom = 1e-6f;
                    float pitch = atan2f(-ax, denom);
                    // Rates
                    uint32_t tnow = now;
                    float dt_s = (tnow - app->last_orient_tick) / 1000.0f;
                    if(dt_s <= 0.0f) dt_s = 1e-3f;
                    // Yaw from heading
                    float yaw_deg = app->heading_deg_smooth;
                    float dyaw = yaw_deg - app->last_yaw_deg;
                    while(dyaw > 180.0f) dyaw -= 360.0f;
                    while(dyaw < -180.0f) dyaw += 360.0f;
                    app->yaw_rate_dps = dyaw / dt_s;
                    app->pitch_rate_dps = (pitch - app->last_pitch) * (180.0f / (float)M_PI) / dt_s;
                    app->roll_rate_dps  = (roll  - app->last_roll)  * (180.0f / (float)M_PI) / dt_s;
                    // Store
                    app->acc_pitch = pitch;
                    app->acc_roll = roll;
                    app->last_pitch = pitch;
                    app->last_roll = roll;
                    app->last_yaw_deg = yaw_deg;
                    app->last_orient_tick = tnow;
                }
            }
            app->last_lsm_poll_ms = now;
        }
        
        Gxhtc3cSample th_out;
        if(now - app->last_th_poll_ms >= 1000) {
            if(gxhtc3c_poll(&app->th, &th_out)) {
                app->th_sample = th_out;
                app->widgets[WidgetTypeTemperature].ok = th_out.ok;
                app->widgets[WidgetTypeHumidity].ok = th_out.ok;
            }
            app->last_th_poll_ms = now;
        }
        
        KiisuLightAdcSample light_out;
        if(now - app->last_light_poll_ms >= 500) {
            if(kiisu_light_adc_poll(&light_out)) {
                app->light_sample = light_out;
                app->widgets[WidgetTypeLight].ok = light_out.ok;
            }
            app->last_light_poll_ms = now;
        }
        
        view_port_update(app->vp);
    }
    
    app_free(app);
    return 0;
}