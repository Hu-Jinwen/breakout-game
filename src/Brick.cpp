#include "Brick.h"

Brick::Brick(float x, float y, float w, float h, Color brickColor) {
    rect = { x, y, w, h };
    active = true;
    color = brickColor;
}

void Brick::Draw() {
    if (active) {
        DrawRectangleRec(rect, color);
        DrawRectangleLinesEx(rect, 1, DARKGRAY);
    }
}