#ifndef PADDLE_H
#define PADDLE_H

#include "raylib.h"

class Paddle {
private:
    Rectangle rect;
    float originalWidth;
    float currentWidth;
    float effectRemainingTime;
    bool isExtended;
    
public:
    Paddle(float x, float y, float w, float h);
    void Draw();
    void MoveLeft(float speed);
    void MoveRight(float speed);
    
    // 道具效果方法
    void Extend(float extraWidth, float duration);
    void ResetWidth();
    void Update(float dt);
    bool IsExtended() const { return isExtended; }
    float GetEffectRemaining() const { return effectRemainingTime; }
    
    Rectangle GetRect() const { return rect; }
    float GetCenterX() const { return rect.x + rect.width / 2; }
    float GetTopY() const { return rect.y; }
    void SetWidth(float width);
};

#endif