#ifndef BRICK_H
#define BRICK_H

#include "raylib.h"

// class Brick
// 游戏中的砖块，构成关卡的障碍物
//
// 职责：
// - 管理单个砖块的位置、尺寸、颜色和状态
// - 提供绘制功能
// - 支持激活/非激活状态切换（被击碎后变为非激活）
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
    Rectangle rect;     // 砖块矩形区域 {x, y, width, height}
    bool active;        // true=砖块存在（可被碰撞），false=已被击碎
    Color color;        // 砖块填充颜色

public:
    // 构造函数
    // 创建一个砖块实例
    // 参数：
    //   x: 砖块左上角X坐标
    //   y: 砖块左上角Y坐标
    //   w: 砖块宽度（像素）
    //   h: 砖块高度（像素）
    //   brickColor: 砖块填充颜色（不同行使用不同颜色）
    Brick(float x, float y, float w, float h, Color brickColor);
    
    // 绘制砖块
    // 绘制填充矩形 + 深灰色边框
    // 只有在active为true时才绘制
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
};

#endif