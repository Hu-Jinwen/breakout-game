#ifndef PADDLE_H
#define PADDLE_H

#include "raylib.h"

class Paddle {
private:
    Rectangle rect;
public:
    Paddle(float x, float y, float w, float h);
    void Draw();
    void MoveLeft(float speed);
    void MoveRight(float speed);
    
    Rectangle GetRect() const { return rect; }
    float GetCenterX() const { return rect.x + rect.width / 2; }
    float GetTopY() const { return rect.y; }
};

#endif