#ifndef BRICK_H
#define BRICK_H

#include "raylib.h"

class Brick {
private:
    Rectangle rect;
    bool active;
    Color color;
public:
    Brick(float x, float y, float w, float h, Color brickColor);
    void Draw();
    bool IsActive() const { return active; }
    void SetActive(bool a) { active = a; }
    Rectangle GetRect() const { return rect; }
    Color GetColor() const { return color; }
};

#endif