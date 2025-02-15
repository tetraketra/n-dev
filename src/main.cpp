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

#define DECODE_STRICT_CHECKS
#define EXCLUDE_EXOTIC_PROTOCOLS
#define SUPPRESS_ERROR_MESSAGE_FOR_BEGIN
#include <IRremote.hpp>

/* --- --- --- --- Draw Defs --- --- --- --- */

#include "hardcoded_images/knockedtfout.h"
#include "hardcoded_images/test_card.h"

#include "include/hsv.hpp"
#include "include/PNGdec/PNGdec.h"

#define DrawArgs_DEFAULT N::DRAW::_DrawARGS_DEFAULT // This is literally just for the colors.

/* --- --- --- --- SD Card Defs --- --- --- ---  */

#include <SD.h>
#include <cstring>

/* --- --- --- --- LCD Defs --- --- --- --- */

#include <LCD_I2C.h>

/* --- --- --- --- MPU Defs --- --- --- --- */

#include <I2Cdev.h>
#include <MPU6050.h>

/* --- --- --- --- Monolith Defs --- --- --- --- */
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

    namespace IR   { // IR Communcation. 
        constexpr int receivePin = 8; // Make sure to use a pin that isn't taken by the SmartMatrix LED shield!
    
        namespace commands { // Change these based on the commands given by your specific IR remote.
            constexpr uint32_t debugToggle = 0x44;
            constexpr uint32_t powerToggle = 0x8;
            constexpr uint32_t bloomUp     = 0x2;
            constexpr uint32_t bloomDown   = 0x3;
            constexpr uint32_t next        = 0x0;
            constexpr uint32_t prev        = 0x1;

            const char* toString(uint32_t command) {
                switch (command) {
                    case debugToggle: return "debugToggle";
                    case powerToggle: return "powerToggle";
                    case bloomUp:     return "bloomUp";
                    case bloomDown:   return "bloomDOwn";
                    case next:        return "next";
                    case prev:        return "prev"; 
                    default:          return "!!!UNRECOGNIZED!!!";
                }
            } 
        };
    
        inline bool inputIsGood() {
            return (
                IrReceiver.repeatCount == 0 &&
                IrReceiver.decodedIRData.decodedRawData != 0 &&
                IrReceiver.decodedIRData.numberOfBits == 32 &&
                IrReceiver.decodedIRData.protocol != UNKNOWN
            );
        }

        inline uint32_t decodedCommand() {
            return IrReceiver.decodedIRData.command;
        }

        inline const char* decodedCommandString() {
            return commands::toString(decodedCommand());
        }
    };
    namespace DRAW { // Drawing. 
        PNG png;
        
        float bloomScale = 0.0; // `0.0` represents whatever the PNG actually has from asprite blurring. `1.0` maxes every transparent pixel fully opaque.
        
        uint32_t frameMillisPer = 1000/24;
        uint32_t frameMillisLastAt;
        uint32_t frameMillisSinceLast;
        float    frameTooSlowAlert = (float)90/100;

        struct Glitch {
            bool  enabled; // Whether to do the glitch at all.
            float chance;
            int   magnitude;

            inline bool happens() {
                return ((enabled) ? CHANCE(chance) : false);
            }
        };        

        typedef struct DrawArgs { // TODO: Figure out color mixing.
            int xOffset;
            int yOffset;

            float* bloomScale;
            bool*  debug;
            bool   mixBlack; // If `false`, treats all non-trans pixels as *fully* opaque. If `true`, mixes with black. Combine with `doBlack` to control behavior.
            bool   drawBlack; // Whether to draw black pixels. Saves cycles sometimes when combined iwth `mixBlack=true`.
            
            struct Glitches { // TODO: Make the "looks nice" values defaults for glitches being on/off.
                Glitch jitter; // Left-right jitter. `{. chance = 0.02f, .magnitude = 3 }` looks nice. 
                Glitch chromatic; // Color alteration. `{ .chance = 0.02f, .magnitude = 80 }` loks nice.
                Glitch desaturate; // Desaturation. `{ .chance = 0.02f, .magnitude = 80 }` looks nice.
                Glitch fail; // Don't draw. `{ .chance = 0.02f, .magnitude = NOT_USED }` looks nice.
            } glitches;
        } PRIVATE; // `PRIVATE` needed for `PNGdec::PNGDraw`.

        static const DrawArgs _DrawARGS_DEFAULT = {    
            .bloomScale = &N::DRAW::bloomScale,
            .debug = &N::debug,
            .mixBlack = true,
            .drawBlack = false,
        };
    
        /* Draws one line. `<PNGdec>` calls this for each line in the PNG on `png.decode()`. */
        void drawLineCallback(PNGDRAW *pDraw) {
            PRIVATE *pPriv = (PRIVATE *)pDraw->pUser; // IDK if I can change these names? Cpp is weird. 
            uint16_t pixelsRow[64]; // image width is *always* 64.
            uint8_t  pixelsOpaque[8];
        
            /* Fetch line information. */
            png.getLineAsRGB565(pDraw, pixelsRow, PNG_RGB565_LITTLE_ENDIAN, (pPriv->mixBlack) ? 0x00000000 : 0xFFFFFFFF); // With 0xFFFFFFFF, all non-zero transparencies of a given color are that color, and `png.getAlphaMask(...)` works. With 0x00000000, every pixel gets mixed with black to include transparency as a color modifier (such as in dimmed bloom pixels). Might want to pass in a `doTransparency` arg via the `PRIVATE` struct to only selectively use this behavior. Drawing on top of without entirely erasing the scene isn't possible with 0x00000000.
            if (!png.getAlphaMask(pDraw, pixelsOpaque, 0)) { // Color mixing can turn transparency into black, which counts as non-opaque!
                return; // Skip row if no pixels.
            }
    
            /* Draw line. */
            int16_t glitchJitterX    = pPriv->glitches.jitter.happens() ? RAND_SIGN() * RAND_WEIGHTED(pPriv->glitches.jitter.magnitude) : 0;
            int16_t glitchDesaturate = pPriv->glitches.desaturate.happens() ? RAND_WEIGHTED(pPriv->glitches.desaturate.magnitude) : 0;
            int16_t glitchChromatic  = pPriv->glitches.chromatic.happens() ? RAND_SIGN() * RAND_WEIGHTED(pPriv->glitches.chromatic.magnitude) : 0;
            bool    glitchFailDraw   = pPriv->glitches.fail.happens();
    
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
        inline void drawFromRAM(DrawArgs args, uint8_t* png_data, int png_data_len) {
            png.close();
            png.openRAM(png_data, png_data_len, drawLineCallback);
            png.decode((void *)&args, 0);
        }
    };
    namespace SDC  { // SD Card. 
        File sdFile;
    
        void* open(const char* filename, int32_t* size) {
            Serial.printf("Opening file \"%s\".\n", filename);
            
            sdFile = SD.open(filename);
            *size = sdFile.size();
        
            return &sdFile;
        }
    
        void close(void* handle) {
            if (sdFile) { sdFile.close(); }
        }
    
        int32_t read(PNGFILE* handle, uint8_t* buffer, int32_t length) {
            return (sdFile) ? sdFile.read(buffer, length) : 0;
        }
    
        int32_t seek(PNGFILE* handle, int32_t position) {
            return (sdFile) ? sdFile.seek(position) : 0;
        }
    };
    namespace LCD  { // LCD Screen. 
        LCD_I2C lcd(0x27, 16, 2);
    };
    namespace MPU  { // Accelerometer. 
        MPU6050 mpu(0x68, &Wire1);
    
        int16_t ax, ay, az; // (A)ccel (X|Y|Z)-axis
        int16_t gx, gy, gz; // (G)yro  (X|Y|Z)-axis
        int16_t axd, ayd, azd; // Deltas.
        int16_t gxd, gyd, gzd; // Deltas.
    
        bool filterSmall = true;
        int  filterThrsh = 600;
    
        void update() {
            int16_t tax, tay, taz; tax = ax; tay = ay; taz = az; // Temporary variables.
            int16_t tgx, tgy, tgz; tgx = gx; tgy = gy; tgz = gz; // Temporary variables.
            
            mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    
            axd = ax-tax; ayd = ay-tay; azd = az-taz;
            gxd = gx-tgx; gyd = gy-tgy; gzd = gz-tgz;
    
            if (filterSmall) {
                axd = (axd < filterThrsh) ? 0 : axd;
                ayd = (ayd < filterThrsh) ? 0 : ayd;
                azd = (azd < filterThrsh) ? 0 : azd;
    
                gxd = (gxd < filterThrsh) ? 0 : gxd;
                gyd = (gyd < filterThrsh) ? 0 : gyd;
                gzd = (gzd < filterThrsh) ? 0 : gzd;
            }
        }
    
        /** Print raw accel and gyro data. */
        void printAG() {
            Serial.printf(
                "Accl: x=%05d, \ty=%05d, \tz=%05d, "
                "Gyro: x=%05d, \ty=%05d, \tz=%05d.\n", 
                           ax,       ay,       az,
                           gx,       gy,       gz
            );
        }
    
        /* Print accel and gyro deltas. */
        void printAGD() {
            Serial.printf(
                "AcclD: x=%05d, \ty=%05d, \tz=%05d, "
                "GyroD: x=%05d, \ty=%05d, \tz=%05d.\n", 
                           axd,      ayd,      azd,
                           gxd,      gyd,      gzd
            );
        }
    };
    namespace ANIM { // Animations. 
        typedef struct Animation {
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
        
            void drawNextFrame(N::DRAW::PRIVATE args) {
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
                N::DRAW::png.close();
                N::DRAW::png.open((const char*)framePath, N::SDC::open, N::SDC::close, N::SDC::read, N::SDC::seek, N::DRAW::drawLineCallback);
                N::DRAW::png.decode((void *)&args, 0);
            }
        };
        
        Animation testSuite; // NOTE LOOKATME
        Animation testSpeed; // NOTE LOOKATME
    
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
    pinMode(N::IR::receivePin, INPUT);

    IrReceiver.begin(N::IR::receivePin, DISABLE_LED_FEEDBACK);

    /* SD Card Setup */
    SD.begin(BUILTIN_SDCARD);
    // SD.sdfs.ls(LS_R); // Optional to see the file structure during testing.

    /* LCD Setup */
    Wire.begin();
    N::LCD::lcd.begin();
    N::LCD::lcd.backlight();

    /* MPU Setup */
    Wire1.begin();
    N::MPU::mpu.initialize();

    /* N Defaults */
    N::debug = false; // Overwride default debug state if needed (e.g. on new controller to get cmd#s).
    N::mode = N::modes::NCFG_M_KNOCKEDTFOUT;

    /* Animation Setup */
    N::ANIM::testSuite.init("test_suite");
    N::ANIM::testSpeed.init("test_speed");
}

void loop() {

    /* IR Remote Command Receiving */
    if (IrReceiver.decode()) {
        if (N::IR::inputIsGood()) { // This has to be indented or stuff breaks.

            /* Print debug if enabled. */
            if (N::debug) {
                Serial.printf("IR received \"%s\": ", N::IR::decodedCommandString());
                IrReceiver.printIRResultShort(&Serial);
            }

            /* Act on command. */
            float bloomChanged = 0.0;
            switch (N::IR::decodedCommand()) {
                case N::IR::commands::debugToggle: 
                    Serial.printf("Debug %s.\n", (N::debug) ? "disabled" : "enabled");
                    N::debug ^= true;
                    break;
                case N::IR::commands::powerToggle:
                    N::displayOn = !N::displayOn;
                    if (N::debug) { Serial.printf("N display turned %s.\n", (N::displayOn) ? "on" : "off"); }
                    break;
                /* NOTE all *cases* below this line to be refactored NOTE */
                case N::IR::commands::bloomUp: 
                    if (N::DRAW::bloomScale < 0.39) { N::DRAW::bloomScale += (bloomChanged =  0.05); }
                    break;
                case N::IR::commands::bloomDown: 
                    if (N::DRAW::bloomScale > 0.01) { N::DRAW::bloomScale += (bloomChanged = -0.05); }
                    break;
                case N::IR::commands::next:
                    if (N::mode < N::modes::NCFG_M_MAX - 1) { N::mode += 1; }
                    break;
                case N::IR::commands::prev:
                    if (N::mode > N::modes:: NCFG_M_MIN + 1) { N::mode -= 1; }
                    break;
            }

            if (N::debug && bloomChanged) { // NOTE FIXME TODO all of this bloom stuff will be refactored into the per-state settings screen
                Serial.printf(
                    "FakeBloom %s from %.2f%% to %.2f%%.\n", 
                    ((bloomChanged > 0) ? "increased" : "decreased"), 
                    (N::DRAW::bloomScale - bloomChanged) * 100, N::DRAW::bloomScale * 100
                );
            }
        }

        /* You need this or it locks. */
        IrReceiver.resume();
    }

    /* N Drawing w/ SmartMatrix */
    uint32_t curMillis = millis();

    N::DRAW::frameMillisSinceLast = curMillis - N::DRAW::frameMillisLastAt;
    if (N::DRAW::frameMillisSinceLast > N::DRAW::frameMillisPer - 1) { // Draw frame if it's been a bit.
        N::DRAW::frameMillisLastAt = curMillis;

        /* Debug print if been too long since last frame. */
        if (N::debug && N::DRAW::frameMillisSinceLast > N::DRAW::frameMillisPer / N::DRAW::frameTooSlowAlert) { 
            Serial.printf(
                "Frame rate dropped dangerously low! "
                "Alert set at %.2f%%, maximally %u millis per frame. "
                "Expected close to %u millis since last frame. Has been %u.\n", 
                N::DRAW::frameTooSlowAlert*100, 
                (int)(N::DRAW::frameMillisPer / N::DRAW::frameTooSlowAlert), 
                N::DRAW::frameMillisPer, N::DRAW::frameMillisLastAt
            ); 
        }

        /* Update accelerometer. */
        N::MPU::update();

        /* Draw LCD. */
        N::LCD::lcd.clear();
        N::LCD::lcd.print(millis());

        /* Draw frame. */
        backgroundLayer.fillScreen(defaultBackgroundColor);

        if (!N::displayOn) {
            goto end_of_frame;
        }

        /* Case per mode. */
        switch (N::mode) {
            case (N::modes::NCFG_M_KNOCKEDTFOUT): { // baked still image example
                N::DRAW::DrawArgs args_alt = DrawArgs_DEFAULT;
                args_alt.glitches.jitter = { .enabled = true, .chance = 0.02f, .magnitude = 3 };
                args_alt.glitches.desaturate = { .enabled = true, .chance = 0.02f, .magnitude = 80 };
                args_alt.glitches.fail = { .enabled = true, .chance = 0.02f };
                args_alt.glitches.chromatic = { .enabled = true, .chance = 0.02f, .magnitude = 80 };
                N::DRAW::drawFromRAM(args_alt, (uint8_t *)knockedtfout_png, (int)knockedtfout_png_len);
                break;
            }

            case (N::modes::NCFG_M_TEST_CARD): { // animation on sd card example
                N::DRAW::DrawArgs args_alt = DrawArgs_DEFAULT;
                N::DRAW::drawFromRAM(args_alt, (uint8_t *)test_card_png, (int)test_card_png_len);
                break;
            }

            case (N::modes::NCFG_M_TEST_ANIM): { // animation on sd card with transparency and layers example
                N::DRAW::DrawArgs args_alt = DrawArgs_DEFAULT;
                args_alt.drawBlack = false;
                args_alt.mixBlack = false;

                N::ANIM::testSpeed.drawNextFrame(DrawArgs_DEFAULT);
                N::ANIM::testSuite.drawNextFrame(args_alt);
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