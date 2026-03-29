#include "Brick.h"

// 默认构造函数（绿色）
Brick::Brick(float x, float y, float w, float h) {
    rect = { x, y, w, h };
    active = true;
    color = GREEN;  // 默认绿色
}

// 新增带颜色的构造函数
Brick::Brick(float x, float y, float w, float h, Color brickColor) {
    rect = { x, y, w, h };
    active = true;
    color = brickColor;
}

void Brick::Draw() {
    if (active) {
        DrawRectangleRec(rect, color);
        // 添加边框效果，让砖块更立体
        DrawRectangleLinesEx(rect, 1, DARKGRAY);
    }
}