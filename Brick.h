#ifndef BRICK_H
#define BRICK_H

#include "raylib.h"

class Brick {
private:
    Rectangle rect;
    bool active;
    Color color;  // 新增颜色属性
public:
    Brick(float x, float y, float w, float h);
    Brick(float x, float y, float w, float h, Color brickColor);  // 新增带颜色的构造函数
    void Draw();
    bool IsActive() const { return active; }
    void SetActive(bool a) { active = a; }
    Rectangle GetRect() const { return rect; }
    Color GetColor() const { return color; }  // 获取颜色
};

#endif