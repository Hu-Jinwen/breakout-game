#ifndef POWERUP_H
#define POWERUP_H

#include "raylib.h"
#include <functional>
#include <memory>

// 道具类型枚举
// 定义了游戏中所有可掉落和可使用的道具种类
// 每种道具对应不同的效果和外观
enum class PowerUpType {
    PADDLE_EXTEND,  // 加长板：临时增加挡板宽度（绿色 ↔️）
    MULTI_BALL,     // 多球：生成额外小球（橙色 ●）
    SLOW_BALL,      // 减速球：降低所有小球速度（蓝色 🐌）
    EXTRA_LIFE      // 额外生命：增加一条命（粉色 ❤），预留未实现
};

// ========== 道具效果基类 - 工厂模式 ==========
// class PowerUpEffect
// 道具效果的抽象基类，使用工厂模式管理不同道具的行为
//
// 为什么使用工厂模式：
//   - 将道具效果与道具掉落物解耦
//   - 便于扩展新道具类型，不需修改现有代码
//   - 支持效果持续时间管理和自动过期
//
// 主要用法：
//   std::unique_ptr<PowerUpEffect> effect = std::make_unique<ExtendPaddleEffect>(40, 5);
//   effect->Apply(game);
//   effect->Update(game, dt);
//   if (effect->IsExpired()) { 移除效果 }
//
// 注意事项：
//   - Apply()在效果生效时调用一次
//   - Update()每帧调用，用于处理持续时间倒计时
//   - 子类需实现Apply()，选择性实现Update()和IsExpired()
class PowerUpEffect {
public:
    virtual ~PowerUpEffect() = default;
    
    // 应用效果（首次生效时调用）
    // 例如：修改挡板宽度、生成额外小球、降低球速
    virtual void Apply(class Game& game) = 0;
    
    // 更新效果状态（每帧调用，可选重写）
    // 用于处理持续时间倒计时、效果结束后的恢复逻辑
    virtual void Update(class Game& /*game*/, float /*dt*/) {}
    
    // 检查效果是否已过期（可选重写）
    // 返回值：true=效果已结束，应从activeEffects中移除
    virtual bool IsExpired() const { return false; }
};

// class ExtendPaddleEffect
// 加长板效果
//
// 效果说明：
//   - 将挡板宽度增加extraWidth像素
//   - 持续duration秒后自动恢复原始宽度
//
// 实现原理：
//   - Apply()时调用Game::GetPaddle().Extend()
//   - Update()每帧减少remainingTime，到期时调用ResetWidth()
//   - IsExpired()在remainingTime <= 0时返回true
class ExtendPaddleEffect : public PowerUpEffect {
private:
    float extraWidth;       // 额外增加的宽度（像素），通常为40
    float duration;         // 总持续时间（秒），通常为5
    float remainingTime;    // 剩余时间（秒），每帧递减
    bool applied;           // 是否已应用效果，防止重复Apply
    
public:
    // 构造函数
    // 参数：
    //   w: 额外增加的宽度
    //   d: 效果持续时间
    ExtendPaddleEffect(float w, float d);
    
    // 应用加长效果
    // 调用Game::GetPaddle().Extend()增加挡板宽度
    void Apply(Game& game) override;
    
    // 更新效果状态
    // 每帧减少剩余时间，到期时调用Game::GetPaddle().ResetWidth()
    void Update(Game& game, float dt) override;
    
    // 检查效果是否过期
    bool IsExpired() const override { return remainingTime <= 0; }
};

// class MultiBallEffect
// 多球效果
//
// 效果说明：
//   - 在当前位置生成extraBalls个额外小球
//   - 额外小球初始方向与主球成角度偏移
//   - 永久存在，直到小球飞出底部消失
//
// 实现原理：
//   - Apply()时调用Game::AddExtraBalls()
//   - 不需要Update()，因为效果是永久性的
//   - IsExpired()始终返回true（立即生效后即移除）
class MultiBallEffect : public PowerUpEffect {
private:
    int extraBalls;     // 额外小球数量，通常为2
    
public:
    // 构造函数
    // 参数count：要生成的额外小球数量
    MultiBallEffect(int count);
    
    // 应用多球效果
    // 调用Game::AddExtraBalls()生成额外小球
    void Apply(Game& game) override;
};

// class SlowBallEffect
// 减速球效果
//
// 效果说明：
//   - 将所有小球速度乘以speedFactor（如0.6表示减速40%）
//   - 持续duration秒后自动恢复原始速度
//
// 实现原理：
//   - Apply()时调用Game::SlowDownBalls()
//   - Update()每帧减少剩余时间，到期时调用Game::RestoreBallSpeed()
//   - IsExpired()在remainingTime <= 0时返回true
class SlowBallEffect : public PowerUpEffect {
private:
    float speedFactor;      // 速度倍率，0.6表示减速40%
    float duration;         // 总持续时间（秒），通常为4
    float remainingTime;    // 剩余时间（秒），每帧递减
    bool applied;           // 是否已应用效果，防止重复Apply
    
public:
    // 构造函数
    // 参数：
    //   factor: 速度倍率（0.0 ~ 1.0）
    //   d: 效果持续时间
    SlowBallEffect(float factor, float d);
    
    // 应用减速效果
    // 调用Game::SlowDownBalls()降低所有小球速度
    void Apply(Game& game) override;
    
    // 更新效果状态
    // 每帧减少剩余时间，到期时调用Game::RestoreBallSpeed()
    void Update(Game& game, float dt) override;
    
    // 检查效果是否过期
    bool IsExpired() const override { return remainingTime <= 0; }
};

// ========== 道具掉落物 ==========
// class PowerUp
// 屏幕上的道具掉落物，砖块被击碎时随机生成并下落
//
// 职责：
//   - 管理道具的位置和类型
//   - 更新下落动画（带旋转效果）
//   - 绘制圆形道具和对应的图标符号
//   - 检测是否超出屏幕底部（需移除）
//
// 主要用法：
//   PowerUp powerUp(x, y, PowerUpType::PADDLE_EXTEND);
//   powerUp.Update(dt);
//   powerUp.Draw();
//   if (CheckCollisionRecs(powerUp.GetRect(), paddleRect)) {
//       应用效果
//       powerUp.SetActive(false);
//   }
//
// 外观说明：
//   - 每种道具使用不同颜色和符号
//   - 绘制3层光晕圆环增强视觉效果
//   - 持续旋转（rotationAngle每帧增加）
class PowerUp {
private:
    Vector2 position;       // 道具中心位置（随下落而降低）
    PowerUpType type;       // 道具类型，决定效果和外观
    bool active;            // true=正在掉落中，false=已拾取或超出屏幕
    float speed;            // 下落速度（像素/秒），默认150
    float radius;           // 绘制半径（像素），默认12
    float rotationAngle;    // 旋转角度（度），每帧增加实现旋转动画
    
public:
    // 构造函数
    // 创建道具掉落物
    // 参数：
    //   x: 生成位置的X坐标（砖块中心）
    //   y: 生成位置的Y坐标（砖块中心）
    //   t: 道具类型
    PowerUp(float x, float y, PowerUpType t);
    
    // 更新道具状态
    // 每帧调用，更新位置（下落）和旋转角度
    void Update(float dt);
    
    // 绘制道具
    // 根据类型选择颜色和符号：
    //   - PADDLE_EXTEND: 绿色 ↔️
    //   - MULTI_BALL:    橙色 ●
    //   - SLOW_BALL:     天蓝色 🐌
    //   - EXTRA_LIFE:    粉色 ❤
    // 绘制多层光晕圆环增强视觉效果
    void Draw();
    
    // 检查道具是否处于激活状态
    bool IsActive() const { return active; }
    
    // 设置激活状态
    // 参数a：true=继续掉落，false=已拾取或移除
    void SetActive(bool a) { active = a; }
    
    // 获取道具矩形区域（用于碰撞检测）
    Rectangle GetRect() const;
    
    // 获取道具类型
    PowerUpType GetType() const { return type; }
    
    // 检查道具是否超出屏幕底部
    // 返回值：true=超出屏幕（应移除），false=仍在屏幕内
    // 注意：此函数声明为const，不修改对象状态
    bool IsOffScreen(int screenHeight) const;
};

#endif