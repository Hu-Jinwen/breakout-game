#ifndef BALL_H
#define BALL_H

#include "raylib.h"
#include <cmath>

// class Ball
// 游戏中的小球，负责物理运动和碰撞检测
//
// 职责：
// - 管理小球的位置和速度
// - 处理与边界、挡板、砖块的碰撞
// - 支持多球道具效果（多个Ball实例共存）
//
// 主要用法：
//   Ball ball({400, 500}, {0, 0}, 8);
//   ball.SetLaunched(true);
//   while (游戏循环) {
//       ball.Move();
//       ball.ApplyGravity();
//       ball.BounceEdge(800, 600);
//       ball.BouncePaddle(paddleRect);
//   }
//
// 注意事项：
// - 发射前需要调用FollowPaddle()让小球跟随挡板
// - 碰撞检测函数会修改小球的位置和速度
// - 多球模式下，每个Ball对象独立更新
class Ball {
private:
    Vector2 position;   // 小球当前中心位置坐标
    Vector2 speed;      // 小球当前速度向量（像素/秒），X和Y方向独立
    float radius;       // 小球半径（像素），用于碰撞检测和绘制
    bool launched;      // true=已发射（独立运动），false=未发射（跟随挡板）
    
    // 物理参数（从config.json加载）
    float gravity;      // 重力加速度，每帧累加到speed.y，模拟真实物理下落
    float maxSpeed;     // 最大速度限制，防止高速穿透物体
    float bounceForce;  // 碰撞时额外反弹力度，增加游戏变数
    float launchSpeed;  // 发射时的初始速度大小
    
    float launchCooldown;  // 发射冷却时间（预留，当前未使用）

public:
    // 构造函数
    // 初始化小球的位置、速度、半径
    // 物理参数使用默认值，后续可通过配置文件加载
    // 参数：
    //   pos: 初始位置（中心点坐标）
    //   sp: 初始速度向量
    //   r: 小球半径
    Ball(Vector2 pos, Vector2 sp, float r);
    
    // 移动小球
    // 将position加上speed * dt的效果
    // 注意：Raylib的帧时间通过GetFrameTime()获取，本函数不直接使用dt
    // 调用前提：launched必须为true
    void Move();
    
    // 绘制小球
    // 绘制红色圆形，发射后显示速度方向指示线
    // 未发射时闪烁显示"PRESS SPACE"提示文字
    void Draw();
    
    // 应用重力
    // 每帧将gravity加到speed.y上，实现下落加速效果
    // 同时检查并限制速度不超过maxSpeed
    // 调用前提：launched必须为true
    void ApplyGravity();
    
    // 添加反弹力度
    // 球碰撞挡板时额外增加向上的冲量，增加游戏变数
    // 参数force：向上的速度增量（正值使球向上加速）
    void AddBounceForce(float force);
    
    // 边界碰撞处理
    // 检测小球是否触碰屏幕边界并反弹
    // 碰撞规则：
    //   - 左/右边界：反转speed.x
    //   - 上边界：反转speed.y并增加bounceForce
    //   - 下边界不在此处理（在Game中处理生命值扣除）
    // 参数：
    //   screenWidth: 屏幕宽度（像素）
    //   screenHeight: 屏幕高度（像素，当前未使用，保留用于扩展）
    void BounceEdge(int screenWidth, int screenHeight);
    
    // 与矩形碰撞处理（通用版，用于砖块等）
    // 使用最近点法检测圆与矩形碰撞，自动修正位置并反转速度
    // 算法原理：
    //   1. 找到矩形上离圆心最近的点
    //   2. 计算该点到圆心的距离
    //   3. 若距离 <= 半径，则发生碰撞
    //   4. 根据最近点位置判断碰撞面（上/下/左/右）
    // 选择此算法的原因：砖块是轴对齐矩形，碰撞场景简单，
    // 比分离轴定理(SAT)更快，时间复杂度O(1)
    // 参数rect：矩形的边界框
    void BounceFromRect(Rectangle rect);
    
    // 与挡板碰撞处理（专用版）
    // 根据击中点偏离挡板中心的比例计算反弹角度
    // 偏离越大，反弹角度越斜，增加游戏技巧性
    // 角度计算公式：angle = 90° - hitPoint * 50°
    //   - hitPoint = 0（中心）：垂直向上（90°）
    //   - hitPoint = ±1（边缘）：斜向40°
    // 参数paddleRect：挡板的矩形区域
    // 调用前提：speed.y > 0（小球正在下落）
    void BouncePaddle(Rectangle paddleRect);
    
    // 检测与砖块的碰撞（无位置修正，仅检测）
    // 使用最近点法检测圆与矩形是否相交
    // 与BounceFromRect不同，本函数不修改小球位置和速度
    // 返回值：true=发生碰撞，false=未碰撞
    // 调用后需外部处理：砖块消失、加分、道具生成
    // 注意：此函数仅检测碰撞，不处理碰撞后的物理响应
    bool CheckBrickCollision(Rectangle brickRect);
    
    // 发射小球
    // 从挡板位置发射小球，给予随机初始方向（-30°到+30°偏差）
    // 发射速度使用launchSpeed常量
    // 参数（当前未使用，保留用于扩展）：
    //   paddleX: 挡板中心X坐标
    //   paddleY: 挡板顶部Y坐标
    //   paddleWidth: 挡板宽度
    void Launch(float paddleX, float paddleY, float paddleWidth);
    
    // 跟随挡板移动（未发射状态）
    // 将小球置于挡板中心上方，随挡板移动
    // 参数：
    //   paddleX: 挡板中心X坐标
    //   paddleY: 挡板顶部Y坐标
    void FollowPaddle(float paddleX, float paddleY);
    
    // 重置到挡板位置（生命值扣除后）
    // 将小球放回挡板上方，重置速度为0，设为未发射状态
    // 参数：
    //   paddleX: 挡板中心X坐标
    //   paddleY: 挡板顶部Y坐标
    void ResetToPaddle(float paddleX, float paddleY);
    
    // 重置小球状态
    // 设置位置和速度，设为未发射状态
    // 用于关卡重置或存档加载
    void Reset(Vector2 pos, Vector2 sp);
    
    // ========== Getter/Setter 方法 ==========
    
    // 获取小球位置
    Vector2 GetPosition() const { return position; }
    
    // 获取小球速度
    Vector2 GetSpeed() const { return speed; }
    
    // 获取小球半径
    float GetRadius() const { return radius; }
    
    // 检查小球是否已发射
    bool IsLaunched() const { return launched; }
    
    // 设置小球速度（用于道具效果）
    void SetSpeed(Vector2 sp) { speed = sp; }
    
    // 设置小球位置（用于存档加载）
    void SetPosition(Vector2 pos) { position = pos; }
    
    // 设置发射状态
    void SetLaunched(bool state) { launched = state; }
};

#endif