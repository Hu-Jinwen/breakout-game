#include "PowerUp.h"
#include "Game.h"
#include <cmath>

// ==================== ExtendPaddleEffect ====================

// 构造函数
// 参数：
//   w: 额外增加的宽度（像素），通常为40
//   d: 效果持续时间（秒），通常为5
ExtendPaddleEffect::ExtendPaddleEffect(float w, float d)
    : extraWidth(w), duration(d), remainingTime(d), applied(false) {}

// 应用加长效果
// 调用Game::GetPaddle().Extend()增加挡板宽度
void ExtendPaddleEffect::Apply(Game& game) {
    if (!applied) {
        game.GetPaddle().Extend(extraWidth, duration);
        applied = true;
        TraceLog(LOG_INFO, "PowerUp: Paddle extended by %.0f for %.1f seconds", extraWidth, duration);
    }
}

// 更新效果状态
// 每帧减少剩余时间，到期时调用Game::GetPaddle().ResetWidth()
void ExtendPaddleEffect::Update(Game& game, float dt) {
    if (applied && remainingTime > 0) {
        remainingTime -= dt;
        if (remainingTime <= 0) {
            game.GetPaddle().ResetWidth();
            TraceLog(LOG_INFO, "PowerUp: Paddle extension expired");
        }
    }
}

// ==================== MultiBallEffect ====================

// 构造函数
// 参数count：要生成的额外小球数量，通常为2
MultiBallEffect::MultiBallEffect(int count) : extraBalls(count) {}

// 应用多球效果
// 调用Game::AddExtraBalls()生成额外小球
void MultiBallEffect::Apply(Game& game) {
    game.AddExtraBalls(extraBalls);
    TraceLog(LOG_INFO, "PowerUp: Added %d extra balls", extraBalls);
}

// ==================== SlowBallEffect ====================

// 构造函数
// 参数：
//   factor: 速度倍率（0.0 ~ 1.0），0.6表示减速40%
//   d: 效果持续时间（秒），通常为4
SlowBallEffect::SlowBallEffect(float factor, float d)
    : speedFactor(factor), duration(d), remainingTime(d), applied(false) {}

// 应用减速效果
// 调用Game::SlowDownBalls()降低所有小球速度
void SlowBallEffect::Apply(Game& game) {
    if (!applied) {
        game.SlowDownBalls(speedFactor);
        applied = true;
        TraceLog(LOG_INFO, "PowerUp: Balls slowed by %.0f%% for %.1f seconds", (1-speedFactor)*100, duration);
    }
}

// 更新效果状态
// 每帧减少剩余时间，到期时调用Game::RestoreBallSpeed()
void SlowBallEffect::Update(Game& game, float dt) {
    if (applied && remainingTime > 0) {
        remainingTime -= dt;
        if (remainingTime <= 0) {
            game.RestoreBallSpeed();
            TraceLog(LOG_INFO, "PowerUp: Ball speed restored");
        }
    }
}

// ==================== PowerUp 掉落物 ====================

// 构造函数
// 创建道具掉落物
// 参数：
//   x: 生成位置的X坐标（砖块中心）
//   y: 生成位置的Y坐标（砖块中心）
//   t: 道具类型
PowerUp::PowerUp(float x, float y, PowerUpType t)
    : position({x, y}), type(t), active(true), speed(150.0f), radius(12.0f), rotationAngle(0.0f) {}

// 更新道具状态
// 每帧调用，更新位置（下落）和旋转角度
void PowerUp::Update(float dt) {
    if (!active) return;
    
    // 向下移动（下落）
    position.y += speed * dt;
    // 旋转动画（每帧增加180度，约3圈/秒，达到闪烁效果）
    rotationAngle += 180.0f * dt;
    if (rotationAngle >= 360.0f) rotationAngle -= 360.0f;
}

// 绘制道具
// 根据类型选择颜色和符号：
//   - PADDLE_EXTEND: 绿色 ↔️
//   - MULTI_BALL:    橙色 ●
//   - SLOW_BALL:     天蓝色 🐌
//   - EXTRA_LIFE:    粉色 ❤（预留）
// 绘制多层光晕圆环增强视觉效果
void PowerUp::Draw() {
    if (!active) return;
    
    // 根据类型选择颜色和符号
    Color color;
    const char* symbol = "?";
    switch (type) {
        case PowerUpType::PADDLE_EXTEND:
            color = GREEN;
            symbol = "↔";
            break;
        case PowerUpType::MULTI_BALL:
            color = ORANGE;
            symbol = "●";
            break;
        case PowerUpType::SLOW_BALL:
            color = SKYBLUE;
            symbol = "🐌";
            break;
        case PowerUpType::EXTRA_LIFE:
            color = PINK;
            symbol = "❤";
            break;
        default:
            color = WHITE;
            symbol = "?";
    }
    
    // 绘制光晕效果（3层半透明圆环，从外到内透明度递减）
    for (int i = 3; i > 0; i--) {
        DrawCircleV(position, radius + i * 2, ColorAlpha(color, 0.1f));
    }
    
    // 绘制道具主体（彩色圆形）
    DrawCircleV(position, radius, color);
    // 绘制白色边框
    DrawCircleLinesV(position, radius, WHITE);
    
    // 绘制内圈（半透明白色，增强立体感）
    DrawCircleV(position, radius - 4, Fade(WHITE, 0.5f));
    
    // 绘制类型符号（文字图标）
    DrawText(symbol, (int)position.x - 6, (int)position.y - 8, 16, WHITE);
}

// 获取道具矩形区域（用于碰撞检测）
// 返回值：以道具中心为圆心的外接正方形矩形
Rectangle PowerUp::GetRect() const {
    return { position.x - radius, position.y - radius, radius * 2, radius * 2 };
}

// 检查道具是否超出屏幕底部
// 返回值：true=超出屏幕（应移除），false=仍在屏幕内
bool PowerUp::IsOffScreen(int screenHeight) const {
    return position.y + radius > screenHeight + 50;
}