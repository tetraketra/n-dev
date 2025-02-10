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

#define IR_IS_GOOD_INPUT IrReceiver.repeatCount == 0 && IrReceiver.decodedIRData.decodedRawData != 0

namespace IRConfig { 
    constexpr int receivePin = 8; // Make sure to use a pin that isn't taken by the SmartMatrix LED shield!

    namespace commands { // Change these based on the commands given by your specific IR remote.
        constexpr uint16_t debugToggle = 0x44;
        constexpr uint16_t powerToggle = 0x8;
        constexpr uint16_t bloomUp     = 0x2;
        constexpr uint16_t bloomDown   = 0x3;
        constexpr uint16_t next        = 0x0;
        constexpr uint16_t prev        = 0x1;
    };
};

/* --- --- --- --- PNG Printer Defs --- --- --- --- */

#include "Nfaces/knockedtfout.h"

#include "include/hsv.hpp"
#include "include/PNGdec/PNGdec.h"

typedef struct PNGDrawArgs {
    float bloomScale; // Needed for PNGDec. I can't just give it N's bloomScale directly.
} PRIVATE;

namespace PNGHandler {
    PNG png;

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
            hsv24pixel.v = MIN(255, hsv24pixel.v + (255 - hsv24pixel.v)*pPriv->bloomScale);
            hsv24pixel.s = MIN(255, hsv24pixel.s + (255 - hsv24pixel.s)*pPriv->bloomScale); 
            rgb24pixel = hsvToRgb(hsv24pixel);
    
            /* Draw. */
            backgroundLayer.drawPixel((int16_t)x, (int16_t)pDraw->y, rgb24pixel);   
        }
    }
};

/* --- --- --- --- SD Card Defs --- --- --- ---  */

#include <SD.h>
#include <cstring>

namespace SDHandler {
    File sdFile;

    void* sdOpen(const char* filename, int32_t* size) {
        Serial.printf("Opening file \"%s\".\n", filename);
        
        sdFile = SD.open(filename);
        *size = sdFile.size();
    
        return &sdFile;
    }

    void sdClose(void* handle) {
        if (sdFile) { sdFile.close(); }
    }

    int32_t sdRead(PNGFILE* handle, uint8_t* buffer, int32_t length) {
        return (sdFile) ? sdFile.read(buffer, length) : 0;
    }

    int32_t sdSeek(PNGFILE* handle, int32_t position) {
        return (sdFile) ? sdFile.seek(position) : 0;
    }
};

/* --- --- --- --- N Defs --- --- --- --- */

namespace N {
    enum modes {
        NCFG_M_OFF,
        NCFG_M_KNOCKEDTFOUT,
        NCFG_M_TESTING_SUITE,
        NCFG_M_TESTING_SPEED,
        NCFG_M_MAX,
    } modes;

    int mode;
    int mode_prev;

    const char* mode_to_string(int mode) {
        switch (mode) {
            case modes::NCFG_M_OFF: return "NCFG_M_OFF";
            case modes::NCFG_M_KNOCKEDTFOUT: return "NCFG_M_KNOCKEDTFOUT";
            case modes::NCFG_M_TESTING_SUITE: return "NCFG_M_TESTING_SUITE";
            case modes::NCFG_M_TESTING_SPEED: return "NCFG_M_TESTING_SPEED";
            default: return "!!Unknown Mode!!";
        }
    }

    bool debug;
    bool displayOn;

    namespace anim {
        constexpr char  basePath[] = "animations/"; // Has the forward slash!
                  char  curPath[128]; // Folder within `animations/` 
                  char  curName[64];
                  float bloomScale = 0.0; // `0.0` represents whatever the PNG actually has from asprite blurring. `1.0` maxes every transparent pixel fully opaque.
        
        namespace frame {
            constexpr uint32_t millisPer = 1000/24;
                      uint32_t millisLastAt;
                      uint32_t millisSinceLast;
                      float    tooSlowAlert = (float)90/100;
                      int      curNum; // animations/{anim}/{anim}{curNum}.png // 1-indexed for name consistency reasons!
        };

        void run(PRIVATE args, const char* animName) {
            
            /* Handle animation switching. */
            if (strcmp(animName, N::anim::curName)) {
                /* Update current animation name. */
                memset(N::anim::curName, 0, sizeof(N::anim::curName)); // update current animation name
                strcpy(N::anim::curName, animName);
    
                /* Update current animation path. */
                memset(N::anim::curPath, 0, sizeof(N::anim::curPath)); // update current animation path
                strcpy(N::anim::curPath, N::anim::basePath);
                strcat(N::anim::curPath, N::anim::curName);
                strcat(N::anim::curPath, "/"); // animations/{anim}/
                
                /* Reset stuff. */
                PNGHandler::png.close();
                N::anim::frame::curNum = 0; // Start on an invalid 0th frame so following code advances to the valid 1st.
            
                /* Debug. */
                if (N::debug) {
                    Serial.printf("Starting animation \"%s\".\n", N::anim::curPath);
                }
            }
    
            /* Create path of next-to-be-drawn frame file. */
            char framePath[128] = {0};
            strcpy(framePath, N::anim::curPath);
            strcat(framePath, N::anim::curName); // animations/{anim}/{anim}
    
            N::anim::frame::curNum++;
            char frameNum[16] = {0};
            itoa(N::anim::frame::curNum, frameNum, 10);
            strcat(framePath, frameNum);
            strcat(framePath, ".png"); // animations/{anim}/{anim}{frameNum}.png
    
            /* Revert back to frame #1 if at the end of the animation. */
            if (!SD.exists(framePath)) {
                if (N::debug) {
                    Serial.printf("No \"%s\". Rewinding animation \"%s\".\n", framePath, N::anim::curPath);
                }
    
                framePath[128] = {0};
    
                strcpy(framePath, N::anim::curPath);
                strcat(framePath, N::anim::curName);
    
                N::anim::frame::curNum = 1;
                char frameNum[16] = {0};
                itoa(N::anim::frame::curNum, frameNum, 10);
                strcat(framePath, frameNum);
                strcat(framePath, ".png"); // /animations/{anim}/{anim}1.png
            }
            
            /* Draw from frame file path. */
            PNGHandler::png.close();
            PNGHandler::png.open((const char*)framePath, SDHandler::sdOpen, SDHandler::sdClose, SDHandler::sdRead, SDHandler::sdSeek, PNGHandler::PNGDraw);
            PNGHandler::png.decode((void *)&args, 0);
        }
    };
};

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
    while (!Serial && millis() < 3000) { yield(); }

    /* IR Remote Setup */
    pinMode(IRConfig::receivePin, INPUT);

    IrReceiver.begin(IRConfig::receivePin, DISABLE_LED_FEEDBACK);

    /* SD Card Setup */
    SD.begin(BUILTIN_SDCARD);
    // SD.sdfs.ls(LS_R); // Optional to see the file structure during testing.

    /* N Defaults */
    N::debug = false; // Overwride default debug state if needed (e.g. on new controller to get cmd#s).
    N::mode = N::modes::NCFG_M_KNOCKEDTFOUT;
}

void loop() {

    /* IR Remote Command Receiving */
    if (IrReceiver.decode()) {
        if (IR_IS_GOOD_INPUT) {

            /* Print debug if enabled. */
            if (N::debug) {
                Serial.printf("IR received: ");
                IrReceiver.printIRResultShort(&Serial);
            }
            
            /* Toggle debug if `commandDebugToggle` pressed. */
            if (IrReceiver.decodedIRData.command == IRConfig::commands::debugToggle) {
                Serial.printf("Debug %s.\n", (N::debug) ? "disabled" : "enabled");
                N::debug ^= true;
            }

            /* [In|de]crease bloom. */
            float bloomChanged = 0.0;
            if (IrReceiver.decodedIRData.command == IRConfig::commands::bloomUp) {
                if (N::anim::bloomScale < 0.39) { 
                    N::anim::bloomScale += (bloomChanged =  0.05); 
                }
            } else if (IrReceiver.decodedIRData.command == IRConfig::commands::bloomDown) {
                if (N::anim::bloomScale > 0.01) { 
                    N::anim::bloomScale += (bloomChanged = -0.05); 
                }
            }

            if (N::debug && bloomChanged) {
                Serial.printf(
                    "FakeBloom %s from %.2f%% to %.2f%%.\n", 
                    ((bloomChanged > 0) ? "increased" : "decreased"), 
                    (N::anim::bloomScale - bloomChanged) * 100, N::anim::bloomScale * 100
                );
            }


            /* Power toggle. */
            if (IrReceiver.decodedIRData.command == IRConfig::commands::powerToggle) {
                N::displayOn = !N::displayOn;

                if (N::debug) {
                    Serial.printf("N display turned %s.\n", (N::displayOn) ? "on" : "off");
                }
            }

            /* Cycle mode. */
            if (IrReceiver.decodedIRData.command == IRConfig::commands::next && N::mode < N::modes::NCFG_M_MAX - 1) {
                N::mode += 1;
            } else if (IrReceiver.decodedIRData.command == IRConfig::commands::prev && N::mode > N::modes::NCFG_M_OFF + 1) {
                N::mode -= 1;
            }
        }

        IrReceiver.resume();
    }

    /* N Drawing w/ SmartMatrix */
    uint32_t curMillis = millis();
    PNGDrawArgs args = {
        .bloomScale = N::anim::bloomScale // Needed for PNGDec. I can't just give it N's bloomScale directly.
    };

    N::anim::frame::millisSinceLast = curMillis - N::anim::frame::millisLastAt;
    if (N::anim::frame::millisSinceLast > N::anim::frame::millisPer - 1) { // Draw frame if it's been a bit.
        N::anim::frame::millisLastAt = curMillis;

        /* Debug print if been too long since last frame. */
        if (N::debug && N::anim::frame::millisSinceLast > N::anim::frame::millisPer / N::anim::frame::tooSlowAlert) { 
            Serial.printf(
                "Frame rate dropped dangerously low! "
                "Alert set at %.2f%%, maximally %u millis per frame. "
                "Expected close to %u millis since last frame. Has been %u.\n", 
                N::anim::frame::tooSlowAlert*100, 
                N::anim::frame::millisPer / N::anim::frame::tooSlowAlert, 
                N::anim::frame::millisPer, N::anim::frame::millisSinceLast
            ); 
        }

        /* Draw frame. */
        backgroundLayer.fillScreen(defaultBackgroundColor);

        if (!N::displayOn) {
            goto end_of_frame;
        }

        /* Case per mode. */
        switch (N::mode) {
            case (N::modes::NCFG_M_KNOCKEDTFOUT): { // baked still image example
                PNGHandler::png.close();
                PNGHandler::png.openRAM((uint8_t *)knockedtfout_png, (int)knockedtfout_png_len, PNGHandler::PNGDraw);
                PNGHandler::png.decode((void *)&args, 0);
                break;
            }

            case (N::modes::NCFG_M_TESTING_SUITE): { // animation on sd card example
                N::anim::run(args, "test_suite");
                break;
            }

            case (N::modes::NCFG_M_TESTING_SPEED): {
                N::anim::run(args, "test_speed");
                break;
            }
    
            /* Do nothing if `NCFG_M_OFF` or default. Intentional fallthrough. */
            case (N::modes::NCFG_M_OFF):
            default: {
                break;
            }
        }
    } 

    end_of_frame:

    /* Mode change. */
    if (N::mode != N::mode_prev) {
        N::anim::frame::curNum = 0;

        if (N::debug) {
            Serial.printf(
                "N mode changed from `%s` to `%s`.\n", 
                N::mode_to_string(N::mode_prev), 
                N::mode_to_string(N::mode)
            ); 
        }
    
        N::mode_prev = N::mode;
    }

    /* You have to do this every loop, or it flickers. */
    backgroundLayer.swapBuffers();
}