#ifndef POWERUP_H
#define POWERUP_H

#include "raylib.h"
#include <functional>
#include <memory>

// 道具类型枚举
enum class PowerUpType {
    PADDLE_EXTEND,  // 加长板
    MULTI_BALL,     // 多球
    SLOW_BALL,      // 减速球
    EXTRA_LIFE      // 额外生命（可选）
};

// ========== 道具效果基类 - 工厂模式 ==========
class PowerUpEffect {
public:
    virtual ~PowerUpEffect() = default;
    virtual void Apply(class Game& game) = 0;
    virtual void Update(class Game& game, float dt) {}
    virtual bool IsExpired() const { return false; }
};

// 加长板效果
class ExtendPaddleEffect : public PowerUpEffect {
private:
    float extraWidth;
    float duration;
    float remainingTime;
    bool applied;
    
public:
    ExtendPaddleEffect(float w, float d);
    void Apply(Game& game) override;
    void Update(Game& game, float dt) override;
    bool IsExpired() const override { return remainingTime <= 0; }
};

// 多球效果
class MultiBallEffect : public PowerUpEffect {
private:
    int extraBalls;
    
public:
    MultiBallEffect(int count);
    void Apply(Game& game) override;
};

// 减速球效果
class SlowBallEffect : public PowerUpEffect {
private:
    float speedFactor;
    float duration;
    float remainingTime;
    bool applied;
    
public:
    SlowBallEffect(float factor, float d);
    void Apply(Game& game) override;
    void Update(Game& game, float dt) override;
    bool IsExpired() const override { return remainingTime <= 0; }
};

// ========== 道具掉落物 ==========
class PowerUp {
private:
    Vector2 position;
    PowerUpType type;
    bool active;
    float speed;
    float radius;
    float rotationAngle;
    
public:
    PowerUp(float x, float y, PowerUpType t);
    
    void Update(float dt);
    void Draw();
    
    bool IsActive() const { return active; }
    void SetActive(bool a) { active = a; }
    Rectangle GetRect() const;
    PowerUpType GetType() const { return type; }
    
    // 边缘检测 - 添加 const 关键字
    bool IsOffScreen(int screenHeight) const;
};

#endif