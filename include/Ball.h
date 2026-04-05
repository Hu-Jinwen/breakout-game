#ifndef BALL_H
#define BALL_H

#include "raylib.h"
#include <cmath>

class Ball {
private:
    Vector2 position;
    Vector2 speed;
    float radius;
    bool launched;
    float gravity;
    float maxSpeed;
    float bounceForce;
    float launchCooldown;  // 新增
    
public:
    Ball(Vector2 pos, Vector2 sp, float r);
    
    void Move();
    void Draw();
    void ApplyGravity();
    void AddBounceForce(float force);
    void BounceEdge(int screenWidth, int screenHeight);
    void BounceFromRect(Rectangle rect);
    void BouncePaddle(Rectangle paddleRect);
    bool CheckBrickCollision(Rectangle brickRect);
    void Launch(float paddleX, float paddleY, float paddleWidth);
    void FollowPaddle(float paddleX, float paddleY);
    void ResetToPaddle(float paddleX, float paddleY);
    void Reset(Vector2 pos, Vector2 sp);
    
    // Getter/Setter
    Vector2 GetPosition() const { return position; }
    Vector2 GetSpeed() const { return speed; }
    float GetRadius() const { return radius; }
    bool IsLaunched() const { return launched; }
    void SetSpeed(Vector2 sp) { speed = sp; }
    void SetPosition(Vector2 pos) { position = pos; }
    void SetLaunched(bool state) { launched = state; }
};

#endif