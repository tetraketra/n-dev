/* --- --- --- --- Msc Defs --- --- --- --- */

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* --- --- --- --- SmartMatrix Defs --- --- --- --- */

#include <MatrixHardware_Teensy4_ShieldV5.h>
#include <SmartMatrix.h>

#define COLOR_DEPTH 24               // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24)
const uint16_t kMatrixWidth   = 64; // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight  = 64;  // Set to the height of your display
const uint8_t kRefreshDepth   = 36;  // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On ESP32: 24, 36, 48
const uint8_t kDmaBufferRows  = 4;   // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
const uint8_t kPanelType = SM_PANELTYPE_HUB75_32ROW_MOD16SCAN; // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_BOTTOM_TO_TOP_STACKING && SM_HUB75_OPTIONS_ESP32_INVERT_CLK); // (SM_HUB75_OPTIONS_NONE);       // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);

const int defaultBrightness = (100*255)/100;        // full (100%) brightness, adjust the 100* for brightness
const int defaultScrollOffset = 6;
const rgb24 defaultBackgroundColor = {0, 0, 0};

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);

/* --- --- --- --- IR Remote Defs --- --- --- --- */

#define SUPPRESS_ERROR_MESSAGE_FOR_BEGIN
#include <IRremote.hpp>

#define IR_RECEIVE_PIN 8 // Make sure to use a pin that isn't taken by the SmartMatrix LED shield!
#define IR_IS_GOOD_INPUT IrReceiver.repeatCount == 0 && IrReceiver.decodedIRData.decodedRawData != 0

struct IrConfig {
    static inline constexpr uint16_t commandDebugToggle = 0x44;
    static inline constexpr uint16_t commandPowerToggle = 0x8;
    static inline constexpr uint16_t commandBloomUp     = 0x2;
    static inline constexpr uint16_t commandBloomDown   = 0x3;
};

/* --- --- --- --- N Defs --- --- --- --- */

struct N {
    static enum modes {
        NCFG_M_OFF,
        NCFG_M_KNOCKEDTFOUT,
    } modes;
    static inline int mode;
    static inline int mode_prev;
    static inline const char* mode_to_string(int mode) {
        switch (mode) {
            case modes::NCFG_M_OFF: return "NCFG_M_OFF";
            case modes::NCFG_M_KNOCKEDTFOUT: return "NCFG_M_KNOCKEDTFOUT";
            default: return "!!Unknown Mode!!";
        }
    }

    static inline bool debug;

    struct anim {
        static inline constexpr uint32_t milisPerFrame = 1000/24;
        static inline           uint32_t lastFrameAtMilis;
        static inline           uint32_t milisSinceLastFrame;
        static inline           float    frameRateTooSlowAlert = (float)90/100;
        static inline           float    bloomScale = 0.0; // `0.0` represents whatever the PNG actually has from asprite blurring. `1.0` maxes every transparent pixel fully opaque.

    };  
};

/* --- --- --- --- PNG Printer Defs --- --- --- --- */

#include "Nfaces/knockedtfout.h"

#include "hsv.hpp"
#include "PNGdec.h"

static PNG png;

typedef struct callback_extra_info {
  // Nothing yet
} PRIVATE;

void PNGDraw(PNGDRAW *pDraw) {
    PRIVATE *pPriv = (PRIVATE *)pDraw->pUser;
    uint16_t pixelsRow[64]; // image width is 64

    png.getLineAsRGB565(pDraw, pixelsRow, PNG_RGB565_LITTLE_ENDIAN, 0x00000000); // With 0xffffffff, all non-zero transparencies of a given color are that color, and `png.getAlphaMask(...)` works. With 0x00000000, every pixel gets mixed with black to include transparency as a color modifier (such as in dimmed bloom pixels). Might want to pass in a `doTransparency` arg via the `PRIVATE` struct to only selectively use this behavior. Drawing on top of without entirely erasing the scene isn't possible with 0x00000000.
    for (size_t x = 0; x < 64; x++) {
        uint16_t rgb565pixel = pixelsRow[x];

        if (rgb565pixel == 0) { 
            continue; // Skip black pixels. See the long comment about 0xffffffff and 0x00000000.
        }

        /* Recompose as rgb24. */
        uint8_t blue =  ( rgb565pixel        & 0x1F) << 3;
        uint8_t green = ((rgb565pixel >> 5)  & 0x3F) << 2;
        uint8_t red =   ((rgb565pixel >> 11) & 0x1F) << 3;

        rgb24 rgb24pixel = rgb24(red, green, blue);
        hsv24 hsv24pixel = rgbToHsv(rgb24pixel);
        hsv24pixel.v = MIN(255, hsv24pixel.v + (255 - hsv24pixel.v)*N::anim::bloomScale);
        hsv24pixel.s = MIN(255, hsv24pixel.s + (255 - hsv24pixel.s)*N::anim::bloomScale); 
        rgb24pixel = hsvToRgb(hsv24pixel);

        /* Draw. */
        backgroundLayer.drawPixel((int16_t)x, (int16_t)pDraw->y, rgb24pixel);   
    }
}

/* --- --- --- --- --- --- --- ---  */

void setup() {

    /* SmartMatrix Setup */
    pinMode(13, OUTPUT); // Always pin 13; can't change it.

    matrix.addLayer(&backgroundLayer); 
    matrix.addLayer(&indexedLayer); 
    matrix.begin();

    matrix.setBrightness(defaultBrightness);

    backgroundLayer.enableColorCorrection(true);

    /* Serial Setup (w/ Timeout) */
    while (!Serial && millis() < 3000) { /* ... */}

    /* IR Remote Setup */
    pinMode(IR_RECEIVE_PIN, INPUT);

    IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);

    N::debug = false; // Overwride default debug state if needed (e.g. on new controller to get cmd#s).
}

void loop() {

    /* IR Remote Command Receiving */
    if (IrReceiver.decode()) {
        if (IR_IS_GOOD_INPUT) {

            /* Print debug if enabled. */
            if (N::debug) {
                IrReceiver.printIRResultShort(&Serial);
                IrReceiver.printIRResultAsCVariables(&Serial);
            }
            
            /* Toggle debug if `commandDebugToggle` pressed. */
            if (IrReceiver.decodedIRData.command == IrConfig::commandDebugToggle) {
                Serial.printf("Debug %s.\n\n", (N::debug) ? "disabled" : "enabled");
                N::debug ^= true;
            }

            /* [In|de]crease bloom. */
            float bloomChanged = 0.0;
            if (IrReceiver.decodedIRData.command == IrConfig::commandBloomUp) {
                if (N::anim::bloomScale < 0.39) { N::anim::bloomScale += (bloomChanged =  0.05); }
            } else if (IrReceiver.decodedIRData.command == IrConfig::commandBloomDown) {
                if (N::anim::bloomScale > 0.01) { N::anim::bloomScale += (bloomChanged = -0.05); }
            }

            if (bloomChanged && N::debug) {
                Serial.printf("FakeBloom %s from %.2f%% to %.2f%%.\n\n", ((bloomChanged > 0) ? "increased" : "decreased"), (N::anim::bloomScale - bloomChanged) * 100, N::anim::bloomScale * 100);
            }


            /* Toggle between `NCFG_M_OFF` and `NCFG_M_KNOCKEDTFOUT` if `commandPowerToggle` pressed. */
            /* NOTE: 
                There will eventually be a whole thing here. This is just for testing.
                Right now, the power button *only* toggles between off and a single static image.
            */
            if (IrReceiver.decodedIRData.command == IrConfig::commandPowerToggle) {
                N::mode = ((N::mode == N::modes::NCFG_M_OFF) ? N::modes::NCFG_M_KNOCKEDTFOUT : N::modes::NCFG_M_OFF);
            }
        }

        IrReceiver.resume();
    }

    /* SmartMatrix Drawing */
    backgroundLayer.fillScreen(defaultBackgroundColor);
    
    /* DRAW: Face at 24fps */
    PRIVATE priv;
    N::anim::milisSinceLastFrame = millis() - N::anim::lastFrameAtMilis;
    if (N::anim::milisSinceLastFrame > N::anim::milisPerFrame) { // Draw frame if it's been a bit.
        if (N::anim::milisSinceLastFrame > N::anim::milisPerFrame / N::anim::frameRateTooSlowAlert && N::debug) Serial.printf("Frame rate dropped dangerously low!\nAlert set at %.2f%%, maximally %u millis per frame.\nExpected close to %u millis since last frame. Has been %u.\n\n", N::anim::frameRateTooSlowAlert*100, N::anim::milisPerFrame / N::anim::frameRateTooSlowAlert, N::anim::milisPerFrame, N::anim::milisSinceLastFrame);
        N::anim::lastFrameAtMilis = millis();

        switch (N::mode) {

            /* Case per mode. Currently only draws a single image. */
            case (N::modes::NCFG_M_KNOCKEDTFOUT): { // Draw this face
                png.openRAM((uint8_t *)knockedtfout_png, (int)knockedtfout_png_len, PNGDraw);
                png.decode((void *)&priv, 0);
                break;
            }
    
            /* Do nothing if `NCFG_M_OFF` or default. Intentional fallthrough. */
            default: 
            case (N::modes::NCFG_M_OFF): {
                break;
            }
        }
    
        backgroundLayer.swapBuffers();
    } 

    /* Debug print mode change(s). */
    if (N::mode != N::mode_prev && N::debug) {
        Serial.printf("N mode changed from `%s` to `%s`.\n\n", N::mode_to_string(N::mode_prev), N::mode_to_string(N::mode));
    }

    N::mode_prev = N::mode;
}