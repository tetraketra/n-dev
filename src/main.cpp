/* --- --- --- --- Msc Defs --- --- --- --- */

#include <math.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CLAMP(x, min, max) (MIN(MAX(x, min), max))
#define RAND_SIGN() (random(2) == 0 ? -1 : 1) 
#define RAND_WEIGHTED(magnitude) (int)(sqrt(random((int)pow((float)magnitude, 2.0)))) // Weighs the random towards 0.
#define CHANCE(chance) ((chance == 0.0f) ? false : random(100) < chance*100) // Checks if the chance passes.

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

/* --- --- --- --- N Defs --- --- --- --- */

namespace N {
    enum modes {
        NCFG_M_MIN,
        NCFG_M_KNOCKEDTFOUT,
        NCFG_M_TEST_CARD,
        NCFG_M_TEST_ANIM,
        NCFG_M_MAX,
    } modes;

    int mode = NCFG_M_KNOCKEDTFOUT;
    int mode_prev;

    const char* mode_to_string(int mode) {
        switch (mode) {
            case modes::NCFG_M_MIN: return "NCFG_M_MIN";
            case modes::NCFG_M_KNOCKEDTFOUT: return "NCFG_M_KNOCKEDTFOUT";
            case modes::NCFG_M_TEST_CARD: return "NCFG_M_TEST_CARD";
            case modes::NCFG_M_TEST_ANIM: return "NCFG_M_TEST_ANIM";
            default: return "!!Unknown Mode!!";
        }
    }

    bool debug;
    bool displayOn;

    namespace display {
        float bloomScale = 0.0; // `0.0` represents whatever the PNG actually has from asprite blurring. `1.0` maxes every transparent pixel fully opaque.
        
        namespace frame {
            uint32_t millisPer = 1000/24;
            uint32_t millisLastAt;
            uint32_t millisSinceLast;
            float    tooSlowAlert = (float)90/100;
        };
    };
};

/* --- --- --- --- PNG Printer Defs --- --- --- --- */

#include "hardcoded_images/knockedtfout.h"
#include "hardcoded_images/test_card.h"

#include "include/hsv.hpp"
#include "include/PNGdec/PNGdec.h"

#define CHANCE_GLITCHHAPPENS(glitch) ((glitch.enabled) ? CHANCE(glitch.chance) : false)

typedef struct Glitch {
    bool  enabled; // Whether to do the glitch at all.
    float chance;
    int   magnitude;
};

typedef struct DrawArgs { // NOTE LOOKATME: I need to figure out color mixing!
    int xOffset;
    int yOffset;

    float* bloomScale;
    bool*  debug;
    bool   mixBlack; // If `false`, treats all non-trans pixels as *fully* opaque. If `true`, mixes with black. Combine with `doBlack` to control behavior.
    bool   drawBlack; // Whether to draw black pixels. Saves cycles sometimes when combined iwth `mixBlack=true`.
    
    struct { // TODO: Make the "looks nice" values defaults for glitches being on/off.
        Glitch jitter; // Left-right jitter. `{. chance = 0.02f, .magnitude = 3 }` looks nice. 
        Glitch chromatic; // Color alteration. `{ .chance = 0.02f, .magnitude = 80 }` loks nice.
        Glitch desaturate; // Desaturation. `{ .chance = 0.02f, .magnitude = 80 }` looks nice.
        Glitch fail; // Don't draw. `{ .chance = 0.02f, .magnitude = NOT_USED }` looks nice.
    } glitches;
} PRIVATE; // `PRIVATE` needed for `PNGdec::PNGDraw`.

#define DrawArgs_DEFAULT _DrawARGS_DEFAULT // This is literally just for the colors.
static const DrawArgs _DrawARGS_DEFAULT = {    
    .bloomScale = &N::display::bloomScale,
    .debug = &N::debug,
    .mixBlack = true,
    .drawBlack = false,

    // All else `0` or `false`.
};

namespace DrawHandler {
    PNG png;

    /* Draws one line. `<PNGdec>` calls this for each line in the PNG on `png.decode()`. */
    void DrawCallback(PNGDRAW *pDraw) {
        PRIVATE *pPriv = (PRIVATE *)pDraw->pUser; // IDK if I can change these names? Cpp is weird. 
        uint16_t pixelsRow[64]; // image width is *always* 64.
        uint8_t  pixelsOpaque[8];
    
        /* Fetch line information. */
        png.getLineAsRGB565(pDraw, pixelsRow, PNG_RGB565_LITTLE_ENDIAN, (pPriv->mixBlack) ? 0x00000000 : 0xFFFFFFFF); // With 0xFFFFFFFF, all non-zero transparencies of a given color are that color, and `png.getAlphaMask(...)` works. With 0x00000000, every pixel gets mixed with black to include transparency as a color modifier (such as in dimmed bloom pixels). Might want to pass in a `doTransparency` arg via the `PRIVATE` struct to only selectively use this behavior. Drawing on top of without entirely erasing the scene isn't possible with 0x00000000.
        if (!png.getAlphaMask(pDraw, pixelsOpaque, 0)) { // Color mixing can turn transparency into black, which counts as non-opaque!
            return; // Skip row if no pixels.
        }

        /* Draw line. */
        int16_t glitchJitterX    = CHANCE_GLITCHHAPPENS(pPriv->glitches.jitter) ? RAND_SIGN() * RAND_WEIGHTED(pPriv->glitches.jitter.magnitude) : 0;
        int16_t glitchDesaturate = CHANCE_GLITCHHAPPENS(pPriv->glitches.desaturate) ? RAND_WEIGHTED(pPriv->glitches.desaturate.magnitude) : 0;
        int16_t glitchChromatic  = CHANCE_GLITCHHAPPENS(pPriv->glitches.chromatic) ? RAND_SIGN() * RAND_WEIGHTED(pPriv->glitches.chromatic.magnitude) : 0;
        bool    glitchFailDraw   = CHANCE_GLITCHHAPPENS(pPriv->glitches.fail);

        if (glitchFailDraw) {
            return;
        }

        for (size_t x = 0; x < 64; x++) {
            if (!pPriv->mixBlack && !((pixelsOpaque[x/8] >> (7 - x%8)) & 1)) {
                continue; // Skip pixel if we're drawing transparency and this pixel is transparent.
            }

            if (!pPriv->drawBlack && !pixelsRow[x]) {
                continue; // Skip pixel if we're not drawing black and this pixel is black.
            }

            /* Recompose as rgb24. */
            uint16_t rgb565pixel = pixelsRow[x];

            uint8_t blue =  ( rgb565pixel        & 0x1F) << 3;
            uint8_t green = ((rgb565pixel >> 5)  & 0x3F) << 2;
            uint8_t red =   ((rgb565pixel >> 11) & 0x1F) << 3;
    
            rgb24 rgb24pixel = rgb24(red, green, blue);
            hsv24 hsv24pixel = rgbToHsv(rgb24pixel);

            hsv24pixel.h = (hsv24pixel.h + glitchChromatic) % 255; // Intentional rollover!
            hsv24pixel.v = hsv24pixel.v + (255 - hsv24pixel.v)*(*pPriv->bloomScale);
            hsv24pixel.s = hsv24pixel.s + (255 - hsv24pixel.s)*(*pPriv->bloomScale);
            hsv24pixel.s = hsv24pixel.s - MIN(glitchDesaturate, 255 - hsv24pixel.v);

            rgb24pixel = hsvToRgb(hsv24pixel);
    
            /* Draw. */
            int16_t modX =        x + pPriv->xOffset + glitchJitterX; 
            int16_t modY = pDraw->y + pPriv->yOffset;
            modX = CLAMP(modX, 0, 63);
            modY = CLAMP(modY, 0, 63);

            backgroundLayer.drawPixel((int16_t)modX, (int16_t)modY, rgb24pixel);   
        }
    }
    
    /* Draw PNG from RAM. */
    inline void drawRAM(DrawArgs args, uint8_t* png_data, int png_data_len) {
        DrawHandler::png.close();
        DrawHandler::png.openRAM(png_data, png_data_len, DrawHandler::DrawCallback);
        DrawHandler::png.decode((void *)&args, 0);
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

/* --- --- --- --- Animation Defs --- --- --- --- */

struct Animation {
    char name[32] = {0}; // Used to search file structure, so be consistent.
    char folderPath[128] = {0}; // animations/{name}/
    char basePath[12] = "animations/"; // Has the forward slash!
    int  curFrame = 0;

    void init(const char* animName) {
        strcpy(name, animName);

        strcpy(folderPath, basePath);
        strcat(folderPath, animName);
        strcat(folderPath, "/"); // animations/{name}/
    }

    void drawNextFrame(PRIVATE args) {
        /* Create path of next-to-be-drawn frame file. */
        char framePath[128] = {0};
        strcpy(framePath, folderPath);
        strcat(framePath, name); // animations/{anim}/{anim}

        curFrame++;
        char frameNum[16] = {0};
        itoa(curFrame, frameNum, 10);
        strcat(framePath, frameNum);
        strcat(framePath, ".png"); // animations/{anim}/{anim}{frameNum}.png

        /* Revert back to frame #1 if at the end of the animation. */
        if (!SD.exists(framePath)) {
            if (args.debug) {
                Serial.printf("No \"%s\". Rewinding animation \"%s\".\n", framePath, folderPath);
            }

            memset(framePath, 0, sizeof(framePath));
            strcpy(framePath, folderPath);
            strcat(framePath, name); // /animations/{anim}/{anim}

            curFrame = 1;
            char frameNum[16] = {0};
            itoa(curFrame, frameNum, 10);
            strcat(framePath, frameNum);
            strcat(framePath, ".png"); // /animations/{anim}/{anim}1.png
        }
        
        /* Draw from frame file path. */
        DrawHandler::png.close();
        DrawHandler::png.open((const char*)framePath, SDHandler::sdOpen, SDHandler::sdClose, SDHandler::sdRead, SDHandler::sdSeek, DrawHandler::DrawCallback);
        DrawHandler::png.decode((void *)&args, 0);
    }
};

Animation testSuite; // NOTE LOOKATME
Animation testSpeed; // NOTE LOOKATME

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

    /* Animation Setup */
    testSuite.init("test_suite");
    testSpeed.init("test_speed");
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
                if (N::display::bloomScale < 0.39) { 
                    N::display::bloomScale += (bloomChanged =  0.05); 
                }
            } else if (IrReceiver.decodedIRData.command == IRConfig::commands::bloomDown) {
                if (N::display::bloomScale > 0.01) { 
                    N::display::bloomScale += (bloomChanged = -0.05); 
                }
            }

            if (N::debug && bloomChanged) {
                Serial.printf(
                    "FakeBloom %s from %.2f%% to %.2f%%.\n", 
                    ((bloomChanged > 0) ? "increased" : "decreased"), 
                    (N::display::bloomScale - bloomChanged) * 100, N::display::bloomScale * 100
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
            } else if (IrReceiver.decodedIRData.command == IRConfig::commands::prev && N::mode > N::modes::NCFG_M_MIN + 1) {
                N::mode -= 1;
            }
        }

        IrReceiver.resume();
    }

    /* N Drawing w/ SmartMatrix */
    uint32_t curMillis = millis();

    N::display::frame::millisSinceLast = curMillis - N::display::frame::millisLastAt;
    if (N::display::frame::millisSinceLast > N::display::frame::millisPer - 1) { // Draw frame if it's been a bit.
        N::display::frame::millisLastAt = curMillis;

        /* Debug print if been too long since last frame. */
        if (N::debug && N::display::frame::millisSinceLast > N::display::frame::millisPer / N::display::frame::tooSlowAlert) { 
            Serial.printf(
                "Frame rate dropped dangerously low! "
                "Alert set at %.2f%%, maximally %u millis per frame. "
                "Expected close to %u millis since last frame. Has been %u.\n", 
                N::display::frame::tooSlowAlert*100, 
                N::display::frame::millisPer / N::display::frame::tooSlowAlert, 
                N::display::frame::millisPer, N::display::frame::millisSinceLast
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
                DrawArgs args_alt = DrawArgs_DEFAULT;
                args_alt.glitches.jitter = { .enabled = true, .chance = 0.02f, .magnitude = 3 };
                args_alt.glitches.desaturate = { .enabled = true, .chance = 0.02f, .magnitude = 80 };
                args_alt.glitches.fail = { .enabled = true, .chance = 0.02f };
                args_alt.glitches.chromatic = { .enabled = true, .chance = 0.02f, .magnitude = 80 };
                DrawHandler::drawRAM(args_alt, (uint8_t *)knockedtfout_png, (int)knockedtfout_png_len);
                break;
            }

            case (N::modes::NCFG_M_TEST_CARD): { // animation on sd card example
                DrawArgs args_alt = DrawArgs_DEFAULT;
                DrawHandler::drawRAM(args_alt, (uint8_t *)test_card_png, (int)test_card_png_len);
                break;
            }

            case (N::modes::NCFG_M_TEST_ANIM): { // animation on sd card with transparency and layers example
                DrawArgs args_alt = DrawArgs_DEFAULT;
                args_alt.drawBlack = false;
                args_alt.mixBlack = false;

                testSpeed.drawNextFrame(DrawArgs_DEFAULT);
                testSuite.drawNextFrame(args_alt);
                break;
            }
    
            /* Do nothing if `NCFG_M_MIN` or default. Intentional fallthrough. */
            case (N::modes::NCFG_M_MIN):
            default: {
                break;
            }
        }
    } 

    end_of_frame:

    /* Mode change. */
    if (N::mode != N::mode_prev) {
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