#include "Ball.h"
#include <cmath>

Ball::Ball(Vector2 pos, Vector2 sp, float r) {
    position = pos;
    speed = sp;
    radius = r;
}

void Ball::Move() {
    position.x += speed.x;
    position.y += speed.y;
}

void Ball::Draw() {
    DrawCircleV(position, radius, RED);
}

void Ball::BounceEdge(int screenWidth, int screenHeight) {
    // 左右边界
    if (position.x - radius <= 0) {
        speed.x = fabs(speed.x);  // 确保向右
        position.x = radius;
    }
    if (position.x + radius >= screenWidth) {
        speed.x = -fabs(speed.x); // 确保向左
        position.x = screenWidth - radius;
    }
    
    // 上边界
    if (position.y - radius <= 0) {
        speed.y = fabs(speed.y);  // 确保向下
        position.y = radius;
    }
}

void Ball::Reset(Vector2 pos, Vector2 sp) {
    position = pos;
    speed = sp;
}

// 新增：从矩形反弹的精确计算
void Ball::BounceFromRect(Rectangle rect) {
    // 计算球心到矩形中心的偏移
    float ballCenterX = position.x;
    float ballCenterY = position.y;
    float rectCenterX = rect.x + rect.width / 2;
    float rectCenterY = rect.y + rect.height / 2;
    
    // 计算重叠深度
    float dx = ballCenterX - rectCenterX;
    float dy = ballCenterY - rectCenterY;
    float absDx = fabs(dx);
    float absDy = fabs(dy);
    
    // 矩形半宽半高
    float halfWidth = rect.width / 2 + radius;
    float halfHeight = rect.height / 2 + radius;
    
    // 计算重叠量
    float overlapX = halfWidth - absDx;
    float overlapY = halfHeight - absDy;
    
    // 根据最小重叠方向决定反弹
    if (overlapX > 0 && overlapY > 0) {
        if (overlapX < overlapY) {
            // 水平方向碰撞
            if (dx > 0) {
                speed.x = fabs(speed.x);   // 向右
                position.x = rect.x + rect.width + radius;
            } else {
                speed.x = -fabs(speed.x);  // 向左
                position.x = rect.x - radius;
            }
        } else {
            // 垂直方向碰撞
            if (dy > 0) {
                speed.y = fabs(speed.y);   // 向下
                position.y = rect.y + rect.height + radius;
            } else {
                speed.y = -fabs(speed.y);  // 向上
                position.y = rect.y - radius;
            }
        }
    }
}