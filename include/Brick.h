#ifndef BRICK_H
#define BRICK_H

#include "raylib.h"

// 砖块类型枚举
// 定义了游戏中所有可能的砖块类型
// 不同砖块有不同的行为和效果
enum class BrickType {
    NORMAL,     // 普通砖块：被击中一次即破碎
    MOVING,     // 移动砖块：会上下或左右移动（用于金字塔关卡）
    PORTAL,     // 传送砖块：球进入后从另一个传送砖飞出（用于城堡关卡）
    SPLIT       // 分裂砖块：球击中后触发球分裂效果
};

// class Brick
// 游戏中的砖块，构成关卡的障碍物
//
// 职责：
// - 管理单个砖块的位置、尺寸、颜色和状态
// - 提供绘制功能
// - 支持激活/非激活状态切换（被击碎后变为非激活）
// - 支持移动砖块（上下移动动画）
// - 支持传送砖块（配对传送球）
//
// 主要用法：
//   Brick brick(100, 80, 70, 25, RED);
//   if (brick.IsActive()) {
//       brick.Draw();
//   }
//   // 球碰撞后
//   brick.SetActive(false);
//
// 关卡布局说明：
//   - 砖块按矩阵排列，每行使用不同颜色（共8种颜色循环）
//   - 每行颜色在Game::brickColors数组中定义
//   - 支持从JSON文件加载自定义布局
//
// 注意事项：
//   - 被击碎的砖块通过SetActive(false)标记为非激活
//   - 非激活的砖块不会被绘制，也不会参与碰撞检测
//   - 关卡通关条件是所有砖块IsActive() == false
class Brick {
private:
    Rectangle rect;         // 砖块矩形区域 {x, y, width, height}
    bool active;            // true=砖块存在（可被碰撞），false=已被击碎
    Color color;            // 砖块填充颜色
    
    // ========== 新增：砖块类型和行为 ==========
    BrickType type;         // 砖块类型（普通/移动/传送/分裂）
    
    // 移动砖块专用字段
    float originalY;        // 移动砖块的原始Y坐标（用于正弦波运动）
    float moveSpeed;        // 移动速度（像素/秒）
    float moveRange;        // 移动幅度（像素）
    float movePhase;        // 移动相位（随时间递增，产生循环运动）
    
    // 传送砖块专用字段
    int portalId;           // 传送门ID，相同ID的传送砖配对
    static int nextPortalId;// 静态计数器，用于自动分配传送门ID
    
public:
    // 构造函数（普通砖块）
    // 创建一个普通砖块实例
    // 参数：
    //   x: 砖块左上角X坐标
    //   y: 砖块左上角Y坐标
    //   w: 砖块宽度（像素）
    //   h: 砖块高度（像素）
    //   brickColor: 砖块填充颜色（不同行使用不同颜色）
    Brick(float x, float y, float w, float h, Color brickColor);
    
    // ========== 新增：带类型的构造函数 ==========
    // 创建指定类型的砖块实例
    // 参数：
    //   x: 砖块左上角X坐标
    //   y: 砖块左上角Y坐标
    //   w: 砖块宽度（像素）
    //   h: 砖块高度（像素）
    //   brickColor: 砖块填充颜色
    //   brickType: 砖块类型（普通/移动/传送/分裂）
    Brick(float x, float y, float w, float h, Color brickColor, BrickType brickType);
    
    // 更新砖块状态（每帧调用）
    // 主要用于移动砖块的位置更新
    // 参数dt：帧时间差（秒）
    void Update(float dt);
    
    // 绘制砖块
    // 普通砖块：填充色 + 深灰色边框
    // 移动砖块：填充色 + 浅色边框（表示可移动）
    // 传送砖块：填充色 + 紫色边框（传送特效）
    // 分裂砖块：填充色 + 橙色边框 + 闪烁效果
    void Draw();
    
    // 检查砖块是否处于激活状态
    // 返回值：true=砖块存在（未被击碎），false=已被击碎
    bool IsActive() const { return active; }
    
    // 设置砖块激活状态
    // 参数a：true=砖块存在，false=标记为已击碎
    // 调用时机：小球碰撞时设置为false
    void SetActive(bool a) { active = a; }
    
    // 获取砖块矩形区域（用于碰撞检测）
    Rectangle GetRect() const { return rect; }
    
    // 获取砖块颜色（用于粒子特效）
    Color GetColor() const { return color; }
    
    // 设置砖块颜色（用于异步加载完成后的特效演示）
    void SetColor(Color newColor) { color = newColor; }
    
    // ========== 新增：砖块类型 Getter/Setter ==========
    
    // 获取砖块类型
    BrickType GetType() const { return type; }
    
    // 设置砖块类型
    void SetType(BrickType newType) { type = newType; }
    
    // 检查是否为传送砖块
    bool IsPortal() const { return type == BrickType::PORTAL; }
    
    // 检查是否为移动砖块
    bool IsMoving() const { return type == BrickType::MOVING; }
    
    // 检查是否为分裂砖块
    bool IsSplit() const { return type == BrickType::SPLIT; }
    
    // ========== 传送砖块专用方法 ==========
    
    // 获取传送门ID
    int GetPortalId() const { return portalId; }
    
    // 设置传送门ID
    void SetPortalId(int id) { portalId = id; }
    
    // 获取下一个可用的传送门ID（静态方法）
    static int GetNextPortalId() { return nextPortalId++; }
    
    // ========== 移动砖块专用方法 ==========
    
    // 设置移动参数（用于移动砖块）
    // 参数：
    //   speed: 移动速度（像素/秒）
    //   range: 移动幅度（像素）
    void SetMoveParams(float speed, float range);
    
    // 重置移动砖块的位置到原始位置
    void ResetMovePosition();
};

#endif