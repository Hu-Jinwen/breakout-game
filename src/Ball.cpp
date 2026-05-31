#include "Ball.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <string>

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
    
    // ========== 新增：多球策略性默认值 ==========
    isMainBall = false;     // 默认不是主球，主球在Game中创建时单独设置
    isHeavyBall = false;    // 默认不是重球
    splitCount = 0;         // 初始分裂计数为0
    
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
// 主球（白色）带光晕，副球（根据速度方向变色）
// 重球（带金色光晕和穿透特效）
// 未发射时闪烁显示"PRESS SPACE"提示文字
void Ball::Draw() {
    // ========== 根据小球类型选择绘制颜色和特效 ==========
    Color ballColor;
    Color glowColor;
    
    if (isHeavyBall) {
        // 重球：金色，带额外的光晕特效
        ballColor = GOLD;
        glowColor = ColorAlpha(GOLD, 0.4f);
        
        // 绘制重球外光晕（3层）
        for (int i = 3; i > 0; i--) {
            DrawCircleV(position, radius + i * 3, ColorAlpha(GOLD, 0.1f));
        }
        // 绘制重球内光晕
        DrawCircleV(position, radius + 2, ColorAlpha(GOLD, 0.3f));
    } 
    else if (isMainBall) {
        // 主球：白色，带淡蓝色光晕
        ballColor = WHITE;
        glowColor = ColorAlpha(SKYBLUE, 0.3f);
        DrawCircleV(position, radius + 2, ColorAlpha(SKYBLUE, 0.2f));
    } 
    else {
        // 副球：根据速度方向决定颜色
        // 速度快偏红，速度慢偏蓝，速度适中偏绿
        float speedMag = std::sqrt(speed.x * speed.x + speed.y * speed.y);
        float normSpeed = std::min(1.0f, speedMag / 12.0f);
        
        // 速度方向影响色调
        float angle = std::atan2(speed.y, speed.x);
        float hue = (angle + 3.14159f) / (2 * 3.14159f);
        
        // 根据速度和方向计算颜色
        if (normSpeed > 0.7f) {
            ballColor = ColorAlpha(RED, 0.85f);
        } else if (normSpeed > 0.3f) {
            ballColor = ColorAlpha(ORANGE, 0.85f);
        } else {
            ballColor = ColorAlpha(SKYBLUE, 0.85f);
        }
        glowColor = ColorAlpha(ballColor, 0.2f);
    }
    
    // 绘制小球主体
    DrawCircleV(position, radius, ballColor);
    
    // 绘制小球高光（左上角亮斑）
    DrawCircleV({position.x - radius * 0.3f, position.y - radius * 0.3f}, 
                radius * 0.25f, Fade(WHITE, 0.6f));
    
    // 发射状态：显示速度方向指示线（拖尾效果）
    if (launched) {
        // 根据小球类型，拖尾颜色不同
        Color trailColor = isHeavyBall ? GOLD : (isMainBall ? SKYBLUE : ballColor);
        Vector2 endPos = { position.x + speed.x * 3, position.y + speed.y * 3 };
        DrawLineEx(position, endPos, 2, Fade(trailColor, 0.5f));
        
        // 重球额外显示穿透特效（旋转的星形光晕）
        if (isHeavyBall) {
            float angle = GetTime() * 10;
            for (int i = 0; i < 4; i++) {
                float a = angle + i * 3.14159f / 2;
                Vector2 spikePos = {
                    position.x + cosf(a) * (radius + 2),
                    position.y + sinf(a) * (radius + 2)
                };
                DrawCircleV(spikePos, 2, GOLD);
            }
        }
    }
    
    // 未发射状态：闪烁显示提示文字（每0.5秒闪烁一次）
    if (!launched) {
        if ((int)(GetTime() * 2) % 2 == 0) {
            DrawText("PRESS SPACE", (int)position.x - 55, (int)position.y - 30, 16, YELLOW);
        }
    }
    
    // 显示重球标识文字（穿透提示）
    if (isHeavyBall && launched) {
        if ((int)(GetTime() * 3) % 2 == 0) {
            DrawText("PIERCE", (int)position.x - 20, (int)position.y - radius - 10, 10, GOLD);
        }
    }
    
    // 显示分裂次数标识（调试/展示用）
    if (splitCount > 0 && launched && !isMainBall) {
        DrawText(TextFormat("x%d", splitCount + 1), 
                 (int)position.x - 8, (int)position.y - radius - 5, 8, Fade(WHITE, 0.6f));
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
    
    // 重球使用穿透碰撞，不反弹
    if (isHeavyBall) {
        return;
    }
    
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

// ========== 新增：重球穿透碰撞处理 ==========
// 重球与矩形碰撞处理（不反转速度，直接穿透砖块）
// 重球击中砖块时，砖块被击碎但球继续前进
// 返回值：true=发生碰撞并处理，false=未碰撞
bool Ball::HeavyBallBounceFromRect(Rectangle rect) {
    if (!launched) return false;
    if (!isHeavyBall) return false;  // 只有重球才能穿透
    
    // 使用最近点法检测碰撞
    float closestX = std::max(rect.x, std::min(position.x, rect.x + rect.width));
    float closestY = std::max(rect.y, std::min(position.y, rect.y + rect.height));
    
    float dx = position.x - closestX;
    float dy = position.y - closestY;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    if (distance <= radius) {
        // 发生碰撞，但不反转速度
        // 只是将小球位置修正到不重叠的状态
        float overlap = radius - distance;
        if (dx != 0 || dy != 0) {
            float nx = dx / distance;
            float ny = dy / distance;
            position.x += nx * overlap;
            position.y += ny * overlap;
        }
        return true;
    }
    return false;
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

// ========== 新增：检测两个小球是否碰撞（用于合并） ==========
// 检测当前小球与另一个小球是否发生碰撞
// 返回值：true=发生碰撞，false=未碰撞
bool Ball::CheckBallCollision(const Ball& other) const {
    if (!launched || !other.IsLaunched()) return false;
    if (this == &other) return false;  // 不与自身比较
    
    // 两个重球不能合并（避免无限合并循环）
    if (isHeavyBall && other.IsHeavyBall()) return false;
    
    // 如果任意一个已经是重球，不触发合并（重球不能再合并）
    if (isHeavyBall || other.IsHeavyBall()) return false;
    
    // 计算圆心距离
    float dx = position.x - other.GetPosition().x;
    float dy = position.y - other.GetPosition().y;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    // 如果距离小于两半径之和，则发生碰撞
    return distance <= (radius + other.GetRadius());
}

// ========== 新增：与另一个小球合并（生成重球） ==========
// 合并条件：
//   - 两个球都不是重球
//   - 两个球都已发射
//   - 检测到碰撞
// 合并后：
//   - 当前球变为重球（isHeavyBall = true）
//   - 速度取两个球的矢量和
//   - 另一个球标记为待删除
// 返回值：true=合并成功，false=不满足合并条件
bool Ball::MergeWith(const Ball& other) {
    // 检查合并条件
    if (!launched || !other.IsLaunched()) return false;
    if (isHeavyBall || other.IsHeavyBall()) return false;
    
    // 计算圆心距离
    float dx = position.x - other.GetPosition().x;
    float dy = position.y - other.GetPosition().y;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    if (distance > (radius + other.GetRadius())) return false;
    
    // 合并：当前球变为重球
    isHeavyBall = true;
    
    // 速度取矢量和（动量守恒的简化版本）
    speed.x = (speed.x + other.GetSpeed().x) * 0.8f;
    speed.y = (speed.y + other.GetSpeed().y) * 0.8f;
    
    // 合并后速度不能太小，否则无法游戏
    float newSpeed = std::sqrt(speed.x * speed.x + speed.y * speed.y);
    if (newSpeed < 4.0f) {
        // 给予一个默认速度
        speed.x = (speed.x > 0 ? 4.0f : -4.0f);
        speed.y = -6.0f;
    }
    
    // 限制最大速度
    float currentSpeed = std::sqrt(speed.x * speed.x + speed.y * speed.y);
    if (currentSpeed > maxSpeed) {
        speed.x = (speed.x / currentSpeed) * maxSpeed;
        speed.y = (speed.y / currentSpeed) * maxSpeed;
    }
    
    // 重球位置设在两个球的中点
    position.x = (position.x + other.GetPosition().x) / 2;
    position.y = (position.y + other.GetPosition().y) / 2;
    
    // 合并时产生闪光效果（通过外部粒子系统实现）
    
    return true;
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
    // 重置时保留主球/副球属性，但重置分裂计数
    splitCount = 0;
    // 注意：不重置 isHeavyBall，因为重球合并后应该保持重球状态
    // 但如果关卡重置，外部会重新创建小球
}

// 重置小球状态
// 用于关卡重置或存档加载
void Ball::Reset(Vector2 pos, Vector2 sp) {
    position = pos;
    speed = sp;
    launched = false;
    splitCount = 0;
    // 重置时不重置 isHeavyBall，由外部决定
}