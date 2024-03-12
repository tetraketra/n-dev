#include <MatrixHardware_Teensy4_ShieldV5.h>
#include <SmartMatrix.h>

// #include <random>

#include "hsv.hpp"

#define COLOR_DEPTH 24               // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24)
const uint16_t kMatrixWidth   = 128; // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight  = 32;  // Set to the height of your display
const uint8_t kRefreshDepth   = 36;  // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On ESP32: 24, 36, 48
const uint8_t kDmaBufferRows  = 4;   // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
const uint8_t kPanelType = SM_PANELTYPE_HUB75_32ROW_MOD16SCAN; // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);       // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);

const int defaultBrightness = (100*255)/100;        // full (100%) brightness, adjust the 100* for brightness
const int defaultScrollOffset = 6;
const rgb24 defaultBackgroundColor = {0, 0, 0};

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);

typedef enum protos_state_os_e {
    __PSTATE_SEC_POWER_BEG,
    PSTATE_OFF,
    PSTATE_BOOTING,
    PSTATE_SHUTTINGDOWN,
    __PSTATE_SEC_POWER_END,

    __PSTATE_SEC_CELLAUTO_BEG,
    PSTATE_LIFE, /* game of life */
    PSTATE_RPSLS, /* rock paper scissors lizard spock */
    PSTATE_SAND, /* sand pile */
    __PSTATE_SEC_CELLAUTO_END
} protos_state_os_e;

typedef struct protos_state_anim_s {
    bool     is_animating;
    bool     is_paused;
    uint16_t frame;
    float    progress; /* 0.0 means first frame (if playing), 1.0 means 100% completed */
} protos_state_anim_s;

class ProtosState {
    public:
        protos_state_os_e state_os;
        protos_state_anim_s state_anim;

    public:
        ProtosState() : state_os(PSTATE_OFF), state_anim() {}

        void fetch_inputs(void) {
            /* ... */
        }

        void update_state(void) {
            switch (this->state_os) {
                case PSTATE_OFF:
                    /* do nothing */
                    break;
                case PSTATE_BOOTING:
                    /* TODO: play animation */
                    break;
                case PSTATE_SHUTTINGDOWN:
                    /* TODO: play animation */
                    break;
                default:
                    break;
            }
        }
};

ProtosState protos_state = ProtosState();

/* --- --- --- --- */

void setup() {
    pinMode(13, OUTPUT);

    matrix.addLayer(&backgroundLayer); 
    matrix.addLayer(&indexedLayer); 
    matrix.begin();

    matrix.setBrightness(defaultBrightness);

    backgroundLayer.enableColorCorrection(true);
}

void loop() {
    backgroundLayer.fillScreen(defaultBackgroundColor);

    protos_state.fetch_inputs();
    protos_state.update_state();

    backgroundLayer.swapBuffers();
}


// void loop() {
//     int i;
//     unsigned long currentMillis;

//     // clear screen
//     backgroundLayer.fillScreen(defaultBackgroundColor);
//     backgroundLayer.swapBuffers();

//     const int delayBetweenShapes = 250;

//     for (i = 0; i < 5000; i += delayBetweenShapes) {
//         // draw for 100ms, then update frame, repeat
//         currentMillis = millis();
//         int x0, y0, x1, y1, x2, y2, radius, radius2;
//         // x0,y0 pair is always on the screen
//         x0 = random(matrix.getScreenWidth());
//         y0 = random(matrix.getScreenHeight());

//         x1 = random(matrix.getScreenWidth());
//         y1 = random(matrix.getScreenHeight());
//         // x2,y2 pair is on screen;
//         x2 = random(matrix.getScreenWidth());
//         y2 = random(matrix.getScreenHeight());

//         // radius is positive, up to screen width size
//         radius = random(matrix.getScreenWidth());
//         radius2 = random(matrix.getScreenWidth());

//         rgb24 fillColor = {(uint8_t)random(192), (uint8_t)random(192), (uint8_t)random(192)};
//         rgb24 outlineColor = {(uint8_t)random(192), (uint8_t)random(192), (uint8_t)random(192)};

//         switch (random(15)) {
//         case 0:
//             backgroundLayer.drawPixel(x0, y0, outlineColor);
//             // backgroundLayer.readPixel(x0, y0);
//             break;

//         case 1:
//             backgroundLayer.drawLine(x0, y0, x1, y1, outlineColor);
//             break;

//         case 2:
//             backgroundLayer.drawCircle(x0, y0, radius, outlineColor);
//             break;

//         case 3:
//             backgroundLayer.drawTriangle(x0, y0, x1, y1, x2, y2, outlineColor);
//             break;

//         case 4:
//             backgroundLayer.drawRectangle(x0, y0, x1, y1, outlineColor);
//             break;

//         case 5:
//             backgroundLayer.drawRoundRectangle(x0, y0, x1, y1, radius, outlineColor);
//             break;

//         case 6:
//             backgroundLayer.fillCircle(x0, y0, radius, fillColor);
//             break;

//         case 7:
//             backgroundLayer.fillTriangle(x0, y0, x1, y1, x2, y2, fillColor);
//             break;

//         case 8:
//             backgroundLayer.fillRectangle(x0, y0, x1, y1, fillColor);
//             break;

//         case 9:
//             backgroundLayer.fillRoundRectangle(x0, y0, x1, y1, radius, fillColor);
//             break;

//         case 10:
//             backgroundLayer.fillCircle(x0, y0, radius, outlineColor, fillColor);
//             break;

//         case 11:
//             backgroundLayer.fillTriangle(x0, y0, x1, y1, x2, y2, outlineColor, fillColor);
//             break;

//         case 12:
//             backgroundLayer.fillRectangle(x0, y0, x1, y1, outlineColor, fillColor);
//             break;

//         case 13:
//             backgroundLayer.fillRoundRectangle(x0, y0, x1, y1, radius, outlineColor, fillColor);
//             break;

//         case 14:
//             backgroundLayer.drawEllipse(x0, y0, radius, radius2, outlineColor);

//         default:
//             break;
//         }
//         backgroundLayer.swapBuffers();
//         //backgroundLayer.fillScreen({0,0,0});
//         while (millis() < currentMillis + delayBetweenShapes);
//     }
// }