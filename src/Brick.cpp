#include "Brick.h"

// 构造函数
// 创建一个砖块实例
// 参数：
//   x: 砖块左上角X坐标
//   y: 砖块左上角Y坐标
//   w: 砖块宽度（像素）
//   h: 砖块高度（像素）
//   brickColor: 砖块填充颜色（不同行使用不同颜色）
Brick::Brick(float x, float y, float w, float h, Color brickColor) {
    rect = { x, y, w, h };
    active = true;           // 新创建的砖块默认激活（未被击碎）
    color = brickColor;
}

// 绘制砖块
// 绘制填充矩形 + 深灰色边框
// 只有在active为true时才绘制（被击碎的砖块不可见）
void Brick::Draw() {
    if (active) {
        // 绘制砖块主体（填充色）
        DrawRectangleRec(rect, color);
        // 绘制砖块边框（深灰色），使砖块之间界限清晰
        DrawRectangleLinesEx(rect, 1, DARKGRAY);
    }
}