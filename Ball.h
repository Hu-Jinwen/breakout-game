#ifndef BALL_H
#define BALL_H

#include "raylib.h"

class Ball {
private:
    Vector2 position;
    Vector2 speed;
    float radius;
public:
    Ball(Vector2 pos, Vector2 sp, float r);
    void Move();
    void Draw();
    void BounceEdge(int screenWidth, int screenHeight);
    void Reset(Vector2 pos, Vector2 sp);
    
    // Getter方法
    Vector2 GetPosition() const { return position; }
    Vector2 GetSpeed() const { return speed; }
    float GetRadius() const { return radius; }
    
    // Setter方法
    void SetSpeed(Vector2 sp) { speed = sp; }
    void SetPosition(Vector2 pos) { position = pos; }
    
    // 新增：精确碰撞处理
    void BounceFromRect(Rectangle rect);
};

#endif