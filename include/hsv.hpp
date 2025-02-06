#ifndef HSV_HPP
#define HSV_HPP

// #include "SmartMatrix/SmartMatrix.h"
#include <SmartMatrix.h>

typedef struct hsv24{
    unsigned char h; /* 0-255 not 0-360 */
    unsigned char s;
    unsigned char v;
} hsv24;

rgb24 HsvToRgb(hsv24 hsv)
{
    rgb24 rgb;
    unsigned char region, remainder, p, q, t;
    
    if (hsv.s == 0)
    {
        rgb.red = hsv.v;
        rgb.green = hsv.v;
        rgb.blue = hsv.v;
        return rgb;
    }
    
    region = hsv.h / 43;
    remainder = (hsv.h - (region * 43)) * 6; 
    
    p = (hsv.v * (255 - hsv.s)) >> 8;
    q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
    t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;
    
    switch (region)
    {
        case 0:
            rgb.red = hsv.v; rgb.green = t; rgb.blue = p;
            break;
        case 1:
            rgb.red = q; rgb.green = hsv.v; rgb.blue = p;
            break;
        case 2:
            rgb.red = p; rgb.green = hsv.v; rgb.blue = t;
            break;
        case 3:
            rgb.red = p; rgb.green = q; rgb.blue = hsv.v;
            break;
        case 4:
            rgb.red = t; rgb.green = p; rgb.blue = hsv.v;
            break;
        default:
            rgb.red = hsv.v; rgb.green = p; rgb.blue = q;
            break;
    }
    
    return rgb;
}

#endif