#include <MatrixHardware_Teensy4_ShieldV5.h>
#include <SmartMatrix.h>

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

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);

#include "gimpbitmap.h"

const int defaultBrightness = (100*255)/100;        // full (100%) brightness
//const int defaultBrightness = (15*255)/100;       // dim: 15% brightness
const int defaultScrollOffset = 6;
const rgb24 defaultBackgroundColor = {0x40, 0, 0};

// Teensy 3.0 has the LED on pin 13
const int ledPin = 13;

void drawBitmap(int16_t x, int16_t y, const gimp32x32bitmap* bitmap) {
  for(unsigned int i=0; i < bitmap->height; i++) {
    for(unsigned int j=0; j < bitmap->width; j++) {
      rgb24 pixel = { bitmap->pixel_data[(i*bitmap->width + j)*3 + 0],
                      bitmap->pixel_data[(i*bitmap->width + j)*3 + 1],
                      bitmap->pixel_data[(i*bitmap->width + j)*3 + 2] };

      backgroundLayer.drawPixel(x + j, y + i, pixel);
    }
  }
}

void setup() {
    // initialize the digital pin as an output.
    pinMode(ledPin, OUTPUT);

    Serial.begin(115200);

    matrix.addLayer(&backgroundLayer); 
    matrix.addLayer(&indexedLayer); 
    matrix.begin();

    matrix.setBrightness(defaultBrightness);

    backgroundLayer.enableColorCorrection(true);
}


/* IDEAS
 - SPLINES https://github.com/snsinfu/cxx-spline
  - one spline equals one curve or hollow shape. thickness var when drawing
  - draw by sampling the spline a number of times proportional to the distance between knots, drawing the corresponding pixel (might redraw)
  - optionally floodfill? draw spline, draw line connecting endpoints to make closed, fill in space? floodfill def needs to be a draw-buffer operation
  - animate by moving spline knots? You could spline to interpolate between keyframes for knots positions lmao. Double spline ^^
  - 
*/
void loop() {
    int i;
    unsigned long currentMillis;

    // clear screen
    backgroundLayer.fillScreen(defaultBackgroundColor);
    backgroundLayer.swapBuffers();

    const int delayBetweenShapes = 250;

    for (i = 0; i < 5000; i += delayBetweenShapes) {
        // draw for 100ms, then update frame, repeat
        currentMillis = millis();
        int x0, y0, x1, y1, x2, y2, radius, radius2;
        // x0,y0 pair is always on the screen
        x0 = random(matrix.getScreenWidth());
        y0 = random(matrix.getScreenHeight());

        x1 = random(matrix.getScreenWidth());
        y1 = random(matrix.getScreenHeight());
        // x2,y2 pair is on screen;
        x2 = random(matrix.getScreenWidth());
        y2 = random(matrix.getScreenHeight());

        // radius is positive, up to screen width size
        radius = random(matrix.getScreenWidth());
        radius2 = random(matrix.getScreenWidth());

        rgb24 fillColor = {(uint8_t)random(192), (uint8_t)random(192), (uint8_t)random(192)};
        rgb24 outlineColor = {(uint8_t)random(192), (uint8_t)random(192), (uint8_t)random(192)};

        switch (random(15)) {
        case 0:
            backgroundLayer.drawPixel(x0, y0, outlineColor);
            break;

        case 1:
            backgroundLayer.drawLine(x0, y0, x1, y1, outlineColor);
            break;

        case 2:
            backgroundLayer.drawCircle(x0, y0, radius, outlineColor);
            break;

        case 3:
            backgroundLayer.drawTriangle(x0, y0, x1, y1, x2, y2, outlineColor);
            break;

        case 4:
            backgroundLayer.drawRectangle(x0, y0, x1, y1, outlineColor);
            break;

        case 5:
            backgroundLayer.drawRoundRectangle(x0, y0, x1, y1, radius, outlineColor);
            break;

        case 6:
            backgroundLayer.fillCircle(x0, y0, radius, fillColor);
            break;

        case 7:
            backgroundLayer.fillTriangle(x0, y0, x1, y1, x2, y2, fillColor);
            break;

        case 8:
            backgroundLayer.fillRectangle(x0, y0, x1, y1, fillColor);
            break;

        case 9:
            backgroundLayer.fillRoundRectangle(x0, y0, x1, y1, radius, fillColor);
            break;

        case 10:
            backgroundLayer.fillCircle(x0, y0, radius, outlineColor, fillColor);
            break;

        case 11:
            backgroundLayer.fillTriangle(x0, y0, x1, y1, x2, y2, outlineColor, fillColor);
            break;

        case 12:
            backgroundLayer.fillRectangle(x0, y0, x1, y1, outlineColor, fillColor);
            break;

        case 13:
            backgroundLayer.fillRoundRectangle(x0, y0, x1, y1, radius, outlineColor, fillColor);
            break;

        case 14:
            backgroundLayer.drawEllipse(x0, y0, radius, radius2, outlineColor);

        default:
            break;
        }
        backgroundLayer.swapBuffers();
        //backgroundLayer.fillScreen({0,0,0});
        while (millis() < currentMillis + delayBetweenShapes);
    }
}