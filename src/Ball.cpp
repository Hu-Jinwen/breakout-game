#include "Ball.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>

// 构造函数
// 初始化小球的位置、速度、半径，设置物理参数默认值
// 同时初始化随机数种子（仅首次调用时执行）
Ball::Ball(Vector2 pos, Vector2 sp, float r) {
    position = pos;
    speed = sp;
    radius = r;
    launched = false;
    gravity = 0.05f;
    maxSpeed = 12.0f;
    bounceForce = 0.3f;
    launchCooldown = 0.0f;
    
    // 静态变量，仅首次调用时执行随机数种子初始化
    // 为什么放在这里：确保每次程序运行产生不同的随机方向
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(nullptr));
        seeded = true;
    }
}

// 移动小球
// 将速度向量加到位置上
// 调用前提：launched必须为true
void Ball::Move() {
    if (!launched) return;
    position.x += speed.x;
    position.y += speed.y;
}

// 绘制小球
// 绘制红色圆形，发射后显示速度方向指示线（黄色拖尾效果）
// 未发射时闪烁显示"PRESS SPACE"提示文字
void Ball::Draw() {
    // 绘制小球主体
    DrawCircleV(position, radius, RED);
    
    // 发射状态：显示速度方向指示线（拖尾效果）
    if (launched) {
        Vector2 endPos = { position.x + speed.x * 3, position.y + speed.y * 3 };
        DrawLineEx(position, endPos, 2, Fade(YELLOW, 0.5f));
    }
    
    // 未发射状态：闪烁显示提示文字（每0.5秒闪烁一次）
    if (!launched) {
        if ((int)(GetTime() * 2) % 2 == 0) {
            DrawText("PRESS SPACE", (int)position.x - 55, (int)position.y - 30, 16, YELLOW);
        }
    }
}

// 应用重力
// 每帧将gravity加到speed.y上，实现下落加速
// 同时检查并限制速度不超过maxSpeed，防止高速穿透物体
void Ball::ApplyGravity() {
    if (!launched) return;
    speed.y += gravity;
    
    // 限制最大速度
    float currentSpeed = std::sqrt(speed.x * speed.x + speed.y * speed.y);
    if (currentSpeed > maxSpeed) {
        speed.x = (speed.x / currentSpeed) * maxSpeed;
        speed.y = (speed.y / currentSpeed) * maxSpeed;
    }
}

// 添加反弹力度
// 球碰撞挡板时额外增加向上的冲量，增加游戏变数
void Ball::AddBounceForce(float force) {
    speed.y -= force;
}

// 边界碰撞处理
// 检测小球是否触碰屏幕边界并反弹
// 碰撞规则：
//   - 左/右边界：反转speed.x
//   - 上边界：反转speed.y并增加bounceForce
//   - 下边界不在此处理（在Game中处理生命值扣除）
void Ball::BounceEdge(int screenWidth, int screenHeight) {
    if (!launched) return;
    
    // 左边界碰撞（x = 5px）
    if (position.x - radius <= 5) {
        speed.x = std::abs(speed.x);
        position.x = radius + 5;
    }
    // 右边界碰撞（x = screenWidth - 5px）
    if (position.x + radius >= screenWidth - 5) {
        speed.x = -std::abs(speed.x);
        position.x = screenWidth - radius - 5;
    }
    
    // 上边界碰撞（y = 5px）
    if (position.y - radius <= 5) {
        speed.y = std::abs(speed.y);
        position.y = radius + 5;
        speed.y += bounceForce;  // 额外反弹力度，增加趣味性
    }
    
    // 下边界不在本函数处理，由Game::CheckCollisions处理扣血
}

// 与矩形碰撞处理（通用版，用于砖块等）
// 使用最近点法检测圆与矩形碰撞，自动修正位置并反转速度
// 算法原理：
//   1. 找到矩形上离圆心最近的点
//   2. 计算该点到圆心的距离
//   3. 若距离 <= 半径，则发生碰撞
//   4. 根据最近点位置判断碰撞面（上/下/左/右）
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
        // 根据重叠最小的方向确定碰撞面
        if (overlapX < overlapY) {
            // 水平碰撞（左/右）
            if (dx > 0) {
                speed.x = std::abs(speed.x);
                position.x = rect.x + rect.width + radius;
            } else {
                speed.x = -std::abs(speed.x);
                position.x = rect.x - radius;
            }
        } else {
            // 垂直碰撞（上/下）
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

// 与挡板碰撞处理（专用版）
// 根据击中点偏离挡板中心的比例计算反弹角度
// 偏离越大，反弹角度越斜，增加游戏技巧性
// 角度计算公式：angle = 90° - hitPoint * 50°
//   - hitPoint = 0（中心）：垂直向上（90°）
//   - hitPoint = ±1（边缘）：斜向40°
void Ball::BouncePaddle(Rectangle paddleRect) {
    if (!launched) return;
    if (speed.y <= 0) return;  // 只处理下落时的碰撞
    
    // 检查是否与挡板碰撞
    if (position.y + radius >= paddleRect.y &&
        position.y + radius <= paddleRect.y + paddleRect.height + std::abs(speed.y) &&
        position.x >= paddleRect.x - radius &&
        position.x <= paddleRect.x + paddleRect.width + radius) {
        
        // 计算击中点偏离中心的比例（-1.0 到 1.0）
        float hitPoint = (position.x - (paddleRect.x + paddleRect.width / 2.0f)) / (paddleRect.width / 2.0f);
        hitPoint = std::clamp(hitPoint, -1.0f, 1.0f);
        
        // 计算速度大小
        float speedMagnitude = std::sqrt(speed.x * speed.x + speed.y * speed.y);
        speedMagnitude = std::max(speedMagnitude + bounceForce * 2, 5.0f);
        
        // 根据击中点计算反弹角度：偏离越大角度越斜
        float angle = 90.0f - hitPoint * 50.0f;
        float angleRad = angle * 3.14159f / 180.0f;
        
        // 更新速度
        speed.x = speedMagnitude * std::cos(angleRad);
        speed.y = -speedMagnitude * std::abs(std::sin(angleRad));
        
        // 修正位置，防止穿透
        position.y = paddleRect.y - radius;
    }
}

// 检测与砖块的碰撞（无位置修正，仅检测）
// 使用最近点法检测圆与矩形是否相交
// 与BounceFromRect不同，本函数不修改小球位置和速度
// 返回值：true=发生碰撞，false=未碰撞
bool Ball::CheckBrickCollision(Rectangle brickRect) {
    if (!launched) return false;
    
    // 找到矩形上离圆心最近的点
    float closestX = std::max(brickRect.x, std::min(position.x, brickRect.x + brickRect.width));
    float closestY = std::max(brickRect.y, std::min(position.y, brickRect.y + brickRect.height));
    
    // 计算最近点到圆心的距离
    float dx = position.x - closestX;
    float dy = position.y - closestY;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    // 如果距离小于等于半径，则发生碰撞
    if (distance <= radius) {
        // 计算碰撞方向（用于修正位置）
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

// 发射小球
// 从挡板位置发射小球，给予随机初始方向（-30°到+30°偏差）
void Ball::Launch(float paddleX, float paddleY, float paddleWidth) {
    if (launched) return;
    
    // 随机角度：-30度到+30度
    float angleDeg = (rand() % 61 - 30);
    float angleRad = angleDeg * 3.14159f / 180.0f;
    
    float launchSpeed = 6.5f;
    
    // 计算速度分量
    speed.x = launchSpeed * sin(angleRad);
    speed.y = -launchSpeed * cos(angleRad);
    
    launched = true;
}

// 跟随挡板移动（未发射状态）
// 将小球置于挡板中心上方，随挡板移动
void Ball::FollowPaddle(float paddleX, float paddleY) {
    if (!launched) {
        position.x = paddleX;
        position.y = paddleY - radius - 5;
    }
}

// 重置到挡板位置（生命值扣除后）
// 将小球放回挡板上方，重置速度为0，设为未发射状态
void Ball::ResetToPaddle(float paddleX, float paddleY) {
    position.x = paddleX;
    position.y = paddleY - radius - 5;
    speed = {0, 0};
    launched = false;
}

// 重置小球状态
// 用于关卡重置或存档加载
void Ball::Reset(Vector2 pos, Vector2 sp) {
    position = pos;
    speed = sp;
    launched = false;
}