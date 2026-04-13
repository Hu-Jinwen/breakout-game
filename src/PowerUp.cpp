#include "PowerUp.h"
#include "Game.h"
#include <cmath>

// ==================== ExtendPaddleEffect ====================
ExtendPaddleEffect::ExtendPaddleEffect(float w, float d)
    : extraWidth(w), duration(d), remainingTime(d), applied(false) {}

void ExtendPaddleEffect::Apply(Game& game) {
    if (!applied) {
        game.GetPaddle().Extend(extraWidth, duration);
        applied = true;
        TraceLog(LOG_INFO, "PowerUp: Paddle extended by %.0f for %.1f seconds", extraWidth, duration);
    }
}

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
MultiBallEffect::MultiBallEffect(int count) : extraBalls(count) {}

void MultiBallEffect::Apply(Game& game) {
    game.AddExtraBalls(extraBalls);
    TraceLog(LOG_INFO, "PowerUp: Added %d extra balls", extraBalls);
}

// ==================== SlowBallEffect ====================
SlowBallEffect::SlowBallEffect(float factor, float d)
    : speedFactor(factor), duration(d), remainingTime(d), applied(false) {}

void SlowBallEffect::Apply(Game& game) {
    if (!applied) {
        game.SlowDownBalls(speedFactor);
        applied = true;
        TraceLog(LOG_INFO, "PowerUp: Balls slowed by %.0f%% for %.1f seconds", (1-speedFactor)*100, duration);
    }
}

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
PowerUp::PowerUp(float x, float y, PowerUpType t)
    : position({x, y}), type(t), active(true), speed(150.0f), radius(12.0f), rotationAngle(0.0f) {}

void PowerUp::Update(float dt) {
    if (!active) return;
    
    position.y += speed * dt;
    rotationAngle += 180.0f * dt;
    if (rotationAngle >= 360.0f) rotationAngle -= 360.0f;
}

void PowerUp::Draw() {
    if (!active) return;
    
    // 根据类型选择颜色
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
    
    // 绘制光晕效果（多层圆环）
    for (int i = 3; i > 0; i--) {
        DrawCircleV(position, radius + i * 2, ColorAlpha(color, 0.1f));
    }
    
    // 绘制道具主体
    DrawCircleV(position, radius, color);
    DrawCircleLinesV(position, radius, WHITE);
    
    // 绘制内圈
    DrawCircleV(position, radius - 4, Fade(WHITE, 0.5f));
    
    // 绘制类型符号
    DrawText(symbol, (int)position.x - 6, (int)position.y - 8, 16, WHITE);
}

Rectangle PowerUp::GetRect() const {
    return { position.x - radius, position.y - radius, radius * 2, radius * 2 };
}

// 修复：添加 const 关键字
bool PowerUp::IsOffScreen(int screenHeight) const {
    return position.y + radius > screenHeight + 50;
}