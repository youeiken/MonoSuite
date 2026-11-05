#include "touch.h"

TouchCalibration touchCal;
bool _wasPressedLastFrame = false;

void initTouchCalibration() {
    touchCal.rawXMin = 5;
    touchCal.rawXMax = 475;
    touchCal.rawYMin = 5;
    touchCal.rawYMax = 795;

    touchCal.swapXY = true;
    touchCal.invertX = false;
    touchCal.invertY = true;

    touchCal.displayWidth = 480;
    touchCal.displayHeight = 800;
}

bool mapTouchToDisplay(int rawX, int rawY, int& outX, int& outY) {
    int x = rawX;
    int y = rawY;

    if (x < touchCal.rawXMin) x = touchCal.rawXMin;
    if (x > touchCal.rawXMax) x = touchCal.rawXMax;
    if (y < touchCal.rawYMin) y = touchCal.rawYMin;
    if (y > touchCal.rawYMax) y = touchCal.rawYMax;

    float fx = (float)(x - touchCal.rawXMin) / (touchCal.rawXMax - touchCal.rawXMin);
    float fy = (float)(y - touchCal.rawYMin) / (touchCal.rawYMax - touchCal.rawYMin);

    if (fx < 0) fx = 0;
    if (fx > 1) fx = 1;
    if (fy < 0) fy = 0;
    if (fy > 1) fy = 1;

    if (touchCal.swapXY) {
        float temp = fx;
        fx = fy;
        fy = temp;
    }

    if (touchCal.invertX) fx = 1.0f - fx;
    if (touchCal.invertY) fy = 1.0f - fy;

    outX = (int)(fx * touchCal.displayWidth);
    outY = (int)(fy * touchCal.displayHeight);

    return true;
}

bool getTouch(int& x, int& y) {
    if (M5.Touch.getCount() > 0) {
        auto detail = M5.Touch.getDetail(0);
        if (detail.isPressed()) {
            if (!_wasPressedLastFrame) {
                mapTouchToDisplay(detail.x, detail.y, x, y);
                _wasPressedLastFrame = true;
                return true;
            }
        } else {
            _wasPressedLastFrame = false;
        }
    } else {
        _wasPressedLastFrame = false;
    }
    return false;
}

bool isTouching() {
    if (M5.Touch.getCount() > 0) {
        auto detail = M5.Touch.getDetail(0);
        return detail.isPressed();
    }
    return false;
}

void resetTouch() {
    _wasPressedLastFrame = false;
}

bool getRawTouch(int& x, int& y) {
    if (M5.Touch.getCount() > 0) {
        auto detail = M5.Touch.getDetail(0);
        if (detail.isPressed()) {
            x = detail.x;
            y = detail.y;
            return true;
        }
    }
    return false;
}
