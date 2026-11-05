#ifndef TOUCH_H
#define TOUCH_H

#include <M5Unified.h>

struct TouchCalibration {
    int rawXMin = 5;
    int rawXMax = 475;
    int rawYMin = 5;
    int rawYMax = 795;
    bool swapXY  = true;
    bool invertX = false;
    bool invertY = true;
    int displayWidth = 480;
    int displayHeight = 800;
};

extern TouchCalibration touchCal;
extern bool _wasPressedLastFrame;

void initTouchCalibration();
bool mapTouchToDisplay(int rawX, int rawY, int& outX, int& outY);
bool getTouch(int& x, int& y);
bool isTouching();
void resetTouch();
bool getRawTouch(int& x, int& y);

#endif
