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
// - 支持主球/副球区分（主球漏掉扣命，副球漏掉只扣分）
// - 支持重球合并机制（两个球碰撞后合为一个重球，可穿透砖块）
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
    
    // ========== 新增：多球策略性相关 ==========
    bool isMainBall;        // true=主球（白色），false=副球（彩色）
                            // 主球漏掉扣生命，副球漏掉只扣分
    bool isHeavyBall;       // true=重球，可穿透砖块
                            // 两个普通球碰撞后合并生成重球
    int splitCount;         // 分裂计数器：球已经分裂过的次数
                            // 限制最多分裂2次，防止无限分裂
    
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
    // 主球（白色）带光晕，副球（根据速度方向变色）
    // 重球（带金色光晕和穿透特效）
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
    
    // ========== 新增：重球穿透碰撞处理 ==========
    // 重球与矩形碰撞处理（不反转速度，直接穿透砖块）
    // 重球击中砖块时，砖块被击碎但球继续前进
    // 返回值：true=发生碰撞并处理，false=未碰撞
    // 参数rect：矩形的边界框
    bool HeavyBallBounceFromRect(Rectangle rect);
    
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
    
    // ========== 新增：检测两个小球是否碰撞（用于合并） ==========
    // 检测当前小球与另一个小球是否发生碰撞
    // 返回值：true=发生碰撞，false=未碰撞
    // 碰撞后调用MergeWith()进行合并
    bool CheckBallCollision(const Ball& other) const;
    
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
    bool MergeWith(const Ball& other);
    
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
    
    // ========== 新增：多球策略性 Getter/Setter ==========
    
    // 检查是否为主球
    bool IsMainBall() const { return isMainBall; }
    
    // 设置是否为主球
    void SetMainBall(bool main) { isMainBall = main; }
    
    // 检查是否为重球（可穿透）
    bool IsHeavyBall() const { return isHeavyBall; }
    
    // 设置是否为重球
    void SetHeavyBall(bool heavy) { isHeavyBall = heavy; }
    
    // 获取分裂计数
    int GetSplitCount() const { return splitCount; }
    
    // 增加分裂计数（在分裂时调用）
    void IncrementSplitCount() { splitCount++; }
    
    // 重置分裂计数（在合并或关卡重置时调用）
    void ResetSplitCount() { splitCount = 0; }
};

#endif