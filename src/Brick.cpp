#include "Brick.h"
#include <cmath>

// 静态成员初始化：传送门ID计数器
int Brick::nextPortalId = 0;

// 构造函数（普通砖块）
// 创建一个砖块实例
// 参数：
//   x: 砖块左上角X坐标
//   y: 砖块左上角Y坐标
//   w: 砖块宽度（像素）
//   h: 砖块高度（像素）
//   brickColor: 砖块填充颜色（不同行使用不同颜色）
Brick::Brick(float x, float y, float w, float h, Color brickColor) {
    rect = { x, y, w, h };
    active = true;           // 新创建的砖块默认激活（未被击碎）
    color = brickColor;
    
    // ========== 新增：默认砖块类型 ==========
    type = BrickType::NORMAL;
    
    // 移动砖块参数初始化
    originalY = y;
    moveSpeed = 0.0f;
    moveRange = 0.0f;
    movePhase = 0.0f;
    
    // 传送砖块参数初始化
    portalId = -1;
}

// ========== 新增：带类型的构造函数 ==========
// 创建指定类型的砖块实例
// 参数：
//   x: 砖块左上角X坐标
//   y: 砖块左上角Y坐标
//   w: 砖块宽度（像素）
//   h: 砖块高度（像素）
//   brickColor: 砖块填充颜色
//   brickType: 砖块类型（普通/移动/传送/分裂）
Brick::Brick(float x, float y, float w, float h, Color brickColor, BrickType brickType) {
    rect = { x, y, w, h };
    active = true;
    color = brickColor;
    type = brickType;
    
    // 移动砖块参数初始化
    originalY = y;
    moveSpeed = 60.0f;      // 默认移动速度：60像素/秒
    moveRange = 15.0f;       // 默认移动幅度：15像素
    movePhase = 0.0f;
    
    // 传送砖块参数初始化
    portalId = (type == BrickType::PORTAL) ? GetNextPortalId() : -1;
    
    // 分裂砖块不需要额外参数
}

// 更新砖块状态（每帧调用）
// 主要用于移动砖块的位置更新
void Brick::Update(float dt) {
    if (!active) return;
    
    if (type == BrickType::MOVING) {
        // 移动砖块：使用正弦波运动，上下移动
        movePhase += dt * moveSpeed;
        float offset = sinf(movePhase) * moveRange;
        rect.y = originalY + offset;
    }
    // 传送砖块和分裂砖块不需要每帧更新位置
    // 传送逻辑在碰撞检测时处理
}

// 绘制砖块
// 普通砖块：填充色 + 深灰色边框
// 移动砖块：填充色 + 浅色边框（表示可移动）
// 传送砖块：填充色 + 紫色边框（传送特效）
// 分裂砖块：填充色 + 橙色边框 + 闪烁效果
void Brick::Draw() {
    if (!active) return;
    
    // 绘制砖块主体（填充色）
    DrawRectangleRec(rect, color);
    
    switch (type) {
        case BrickType::NORMAL: {
            DrawRectangleLinesEx(rect, 1, DARKGRAY);
            break;
        }
            
        case BrickType::MOVING: {
            // 移动砖块保留，但可以不用
            DrawRectangleLinesEx(rect, 2, SKYBLUE);
            break;
        }
            
        case BrickType::PORTAL: {
            // 传送砖块保留，但可以不用
            DrawRectangleLinesEx(rect, 2, PURPLE);
            break;
        }
            
        case BrickType::SPLIT: {
            // 分裂砖块：橙色边框 + 闪烁 S
            float pulse = (sinf(GetTime() * 8) + 1) / 2;
            Color borderColor = ColorAlpha(ORANGE, 0.5f + pulse * 0.5f);
            DrawRectangleLinesEx(rect, 2, borderColor);
            float centerX = rect.x + rect.width / 2;
            float centerY = rect.y + rect.height / 2;
            DrawText("S", (int)centerX - 4, (int)centerY - 8, 16, 
                     ColorAlpha(ORANGE, 0.8f + pulse * 0.2f));
            break;
        }
            
        default: {
            DrawRectangleLinesEx(rect, 1, DARKGRAY);
            break;
        }
    }
    
    // 砖块内高光
    DrawLine(rect.x + 2, rect.y + 2, rect.x + rect.width - 4, rect.y + 2, 
             ColorAlpha(WHITE, 0.3f));
    DrawLine(rect.x + 2, rect.y + 2, rect.x + 2, rect.y + rect.height - 4, 
             ColorAlpha(WHITE, 0.3f));
}

// ========== 新增：设置移动参数（用于移动砖块） ==========
void Brick::SetMoveParams(float speed, float range) {
    moveSpeed = speed;
    moveRange = range;
    movePhase = 0.0f;
}

// ========== 新增：重置移动砖块的位置到原始位置 ==========
void Brick::ResetMovePosition() {
    if (type == BrickType::MOVING) {
        rect.y = originalY;
        movePhase = 0.0f;
    }
}