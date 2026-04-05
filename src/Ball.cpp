#include "Ball.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>

Ball::Ball(Vector2 pos, Vector2 sp, float r) {
    position = pos;
    speed = sp;
    radius = r;
    launched = false;
    gravity = 0.05f;
    maxSpeed = 12.0f;
    bounceForce = 0.3f;
    launchCooldown = 0.0f;
    
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(nullptr));
        seeded = true;
    }
}

void Ball::Move() {
    if (!launched) return;
    position.x += speed.x;
    position.y += speed.y;
}

void Ball::Draw() {
    DrawCircleV(position, radius, RED);
    
    if (launched) {
        Vector2 endPos = { position.x + speed.x * 3, position.y + speed.y * 3 };
        DrawLineEx(position, endPos, 2, Fade(YELLOW, 0.5f));
    }
    
    if (!launched) {
        if ((int)(GetTime() * 2) % 2 == 0) {
            DrawText("PRESS SPACE", (int)position.x - 55, (int)position.y - 30, 16, YELLOW);
        }
    }
}

void Ball::ApplyGravity() {
    if (!launched) return;
    speed.y += gravity;
    
    float currentSpeed = std::sqrt(speed.x * speed.x + speed.y * speed.y);
    if (currentSpeed > maxSpeed) {
        speed.x = (speed.x / currentSpeed) * maxSpeed;
        speed.y = (speed.y / currentSpeed) * maxSpeed;
    }
}

void Ball::AddBounceForce(float force) {
    speed.y -= force;
}

void Ball::BounceEdge(int screenWidth, int screenHeight) {
    if (!launched) return;
    
    if (position.x - radius <= 5) {
        speed.x = std::abs(speed.x);
        position.x = radius + 5;
    }
    if (position.x + radius >= screenWidth - 5) {
        speed.x = -std::abs(speed.x);
        position.x = screenWidth - radius - 5;
    }
    
    if (position.y - radius <= 5) {
        speed.y = std::abs(speed.y);
        position.y = radius + 5;
        speed.y += bounceForce;
    }
}

void Ball::BounceFromRect(Rectangle rect) {
    if (!launched) return;
    
    float ballCenterX = position.x;
    float ballCenterY = position.y;
    float rectCenterX = rect.x + rect.width / 2;
    float rectCenterY = rect.y + rect.height / 2;
    
    float dx = ballCenterX - rectCenterX;
    float dy = ballCenterY - rectCenterY;
    float absDx = std::abs(dx);
    float absDy = std::abs(dy);
    
    float halfWidth = rect.width / 2 + radius;
    float halfHeight = rect.height / 2 + radius;
    
    float overlapX = halfWidth - absDx;
    float overlapY = halfHeight - absDy;
    
    if (overlapX > 0 && overlapY > 0) {
        if (overlapX < overlapY) {
            if (dx > 0) {
                speed.x = std::abs(speed.x);
                position.x = rect.x + rect.width + radius;
            } else {
                speed.x = -std::abs(speed.x);
                position.x = rect.x - radius;
            }
        } else {
            if (dy > 0) {
                speed.y = std::abs(speed.y);
                position.y = rect.y + rect.height + radius;
            } else {
                speed.y = -std::abs(speed.y);
                position.y = rect.y - radius;
            }
        }
    }
}

void Ball::BouncePaddle(Rectangle paddleRect) {
    if (!launched) return;
    if (speed.y <= 0) return;
    
    if (position.y + radius >= paddleRect.y &&
        position.y + radius <= paddleRect.y + paddleRect.height + std::abs(speed.y) &&
        position.x >= paddleRect.x - radius &&
        position.x <= paddleRect.x + paddleRect.width + radius) {
        
        float hitPoint = (position.x - (paddleRect.x + paddleRect.width / 2.0f)) / (paddleRect.width / 2.0f);
        hitPoint = std::clamp(hitPoint, -1.0f, 1.0f);
        
        float speedMagnitude = std::sqrt(speed.x * speed.x + speed.y * speed.y);
        speedMagnitude = std::max(speedMagnitude + bounceForce * 2, 5.0f);
        
        float angle = 90.0f - hitPoint * 50.0f;
        float angleRad = angle * 3.14159f / 180.0f;
        
        speed.x = speedMagnitude * std::cos(angleRad);
        speed.y = -speedMagnitude * std::abs(std::sin(angleRad));
        
        position.y = paddleRect.y - radius;
    }
}

// 修复后的碰撞检测函数 - 使用 <= 来判断碰撞
bool Ball::CheckBrickCollision(Rectangle brickRect) {
    if (!launched) return false;
    
    // 计算球心到矩形最近点的距离
    float closestX = std::max(brickRect.x, std::min(position.x, brickRect.x + brickRect.width));
    float closestY = std::max(brickRect.y, std::min(position.y, brickRect.y + brickRect.height));
    
    float dx = position.x - closestX;
    float dy = position.y - closestY;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    // 如果距离小于等于半径，则发生碰撞（使用 <= 而不是 <）
    if (distance <= radius) {
        // 计算碰撞方向
        float overlapX = radius - std::abs(dx);
        float overlapY = radius - std::abs(dy);
        
        // 根据重叠最小的方向确定碰撞面
        if (overlapX < overlapY) {
            // 水平碰撞
            if (dx > 0) {
                position.x = brickRect.x + brickRect.width + radius;
            } else {
                position.x = brickRect.x - radius;
            }
            speed.x = -speed.x;
        } else {
            // 垂直碰撞
            if (dy > 0) {
                position.y = brickRect.y + brickRect.height + radius;
            } else {
                position.y = brickRect.y - radius;
            }
            speed.y = -speed.y;
        }
        return true;
    }
    return false;
}

void Ball::Launch(float paddleX, float paddleY, float paddleWidth) {
    if (launched) return;
    
    float angleDeg = (rand() % 61 - 30);
    float angleRad = angleDeg * 3.14159f / 180.0f;
    
    float launchSpeed = 6.5f;
    
    speed.x = launchSpeed * sin(angleRad);
    speed.y = -launchSpeed * cos(angleRad);
    
    launched = true;
}

void Ball::FollowPaddle(float paddleX, float paddleY) {
    if (!launched) {
        position.x = paddleX;
        position.y = paddleY - radius - 5;
    }
}

void Ball::ResetToPaddle(float paddleX, float paddleY) {
    position.x = paddleX;
    position.y = paddleY - radius - 5;
    speed = {0, 0};
    launched = false;
}

void Ball::Reset(Vector2 pos, Vector2 sp) {
    position = pos;
    speed = sp;
    launched = false;
}