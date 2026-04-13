#include "Paddle.h"

Paddle::Paddle(float x, float y, float w, float h) {
    rect = { x, y, w, h };
    originalWidth = w;
    currentWidth = w;
    effectRemainingTime = 0;
    isExtended = false;
}

void Paddle::Draw() {
    // 根据是否有buff改变颜色
    Color paddleColor = isExtended ? ColorAlpha(GREEN, 0.8f) : BLUE;
    DrawRectangleRec(rect, paddleColor);
    DrawRectangleLinesEx(rect, 2, isExtended ? DARKGREEN : DARKBLUE);
    
    // 显示buff剩余时间进度条
    if (isExtended && effectRemainingTime > 0) {
        int barWidth = (int)(rect.width * (effectRemainingTime / 5.0f));
        DrawRectangle(rect.x, rect.y - 5, barWidth, 3, GREEN);
    }
}

void Paddle::MoveLeft(float speed) {
    rect.x -= speed;
    if (rect.x < 5) rect.x = 5;
}

void Paddle::MoveRight(float speed) {
    rect.x += speed;
    if (rect.x + rect.width > GetScreenWidth() - 5)
        rect.x = GetScreenWidth() - rect.width - 5;
}

void Paddle::Extend(float extraWidth, float duration) {
    if (!isExtended) {
        originalWidth = rect.width;
    }
    rect.width = originalWidth + extraWidth;
    currentWidth = rect.width;
    effectRemainingTime = duration;
    isExtended = true;
    
    // 确保扩展后不超出屏幕
    if (rect.x + rect.width > GetScreenWidth() - 5) {
        rect.x = GetScreenWidth() - rect.width - 5;
    }
}

void Paddle::ResetWidth() {
    rect.width = originalWidth;
    currentWidth = originalWidth;
    effectRemainingTime = 0;
    isExtended = false;
}

void Paddle::Update(float dt) {
    if (isExtended) {
        effectRemainingTime -= dt;
        if (effectRemainingTime <= 0) {
            ResetWidth();
        }
    }
}

void Paddle::SetWidth(float width) {
    rect.width = width;
}