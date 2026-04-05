#include "Paddle.h"

Paddle::Paddle(float x, float y, float w, float h) {
    rect = { x, y, w, h };
}

void Paddle::Draw() {
    DrawRectangleRec(rect, BLUE);
    DrawRectangleLinesEx(rect, 2, DARKBLUE);
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