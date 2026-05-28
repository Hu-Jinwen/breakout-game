#ifndef PADDLE_H
#define PADDLE_H

#include "raylib.h"

// class Paddle
// 游戏中的挡板（球拍），由玩家控制左右移动
//
// 职责：
// - 管理挡板的位置和尺寸
// - 响应左右移动输入
// - 支持加长板道具效果（临时增加宽度）
//
// 主要用法：
//   Paddle paddle(350, 550, 100, 15);
//   paddle.MoveLeft(speed);
//   paddle.MoveRight(speed);
//   paddle.Update(dt);  // 更新道具效果剩余时间
//   paddle.Draw();
//
// 道具效果说明：
//   - Extend()：临时增加挡板宽度，持续duration秒
//   - 效果期间挡板颜色变为半透明绿色，顶部显示进度条
//   - 效果结束后自动恢复原始宽度
//
// 注意事项：
// - 移动时会自动检查屏幕边界（左右各留5px边距）
// - 加长效果期间调用Extend()会重置计时器
class Paddle {
private:
    Rectangle rect;             // 挡板矩形区域 {x, y, width, height}
    float originalWidth;        // 原始宽度（像素），用于加长效果结束后恢复
    float currentWidth;         // 当前宽度（像素），加长期间等于originalWidth+extraWidth
    float effectRemainingTime;  // 加长效果剩余时间（秒），<=0时效果结束
    bool isExtended;            // true=当前处于加长状态，false=正常状态

public:
    // 构造函数
    // 初始化挡板的位置、尺寸
    // 参数：
    //   x: 挡板左上角X坐标
    //   y: 挡板左上角Y坐标
    //   w: 挡板宽度（像素）
    //   h: 挡板高度（像素）
    Paddle(float x, float y, float w, float h);
    
    // 绘制挡板
    // 正常状态：蓝色矩形 + 深蓝色边框
    // 加长状态：半透明绿色矩形 + 深绿色边框 + 顶部绿色进度条
    // 进度条宽度 = 当前宽度 * (剩余时间 / 总持续时间)
    void Draw();
    
    // 向左移动
    // 速度参数speed单位：像素/秒
    // 自动限制左边界为5px（防止超出屏幕）
    void MoveLeft(float speed);
    
    // 向右移动
    // 速度参数speed单位：像素/秒
    // 自动限制右边界为screenWidth - 5px（防止超出屏幕）
    void MoveRight(float speed);
    
    // 应用加长效果（道具）
    // 临时增加挡板宽度，持续duration秒
    // 如果效果期间再次调用，会重置计时器（重新计时）
    // 参数：
    //   extraWidth: 额外增加的宽度（像素），通常为40
    //   duration: 效果持续时间（秒），通常为5
    // 
    // 实现说明：
    //   - 首次调用时保存originalWidth
    //   - 每次调用都会重置effectRemainingTime为duration
    //   - 调用后自动检查是否超出屏幕右边界，如超出则修正位置
    void Extend(float extraWidth, float duration);
    
    // 恢复原始宽度
    // 将挡板宽度恢复为originalWidth，清除加长状态
    // 在Extend()效果到期时自动调用
    void ResetWidth();
    
    // 更新状态（每帧调用）
    // 减少effectRemainingTime，到期时自动调用ResetWidth()
    // 必须在每帧调用，以正确处理道具效果计时
    void Update(float dt);
    
    // 检查是否处于加长状态
    bool IsExtended() const { return isExtended; }
    
    // 获取加长效果剩余时间（秒）
    float GetEffectRemaining() const { return effectRemainingTime; }
    
    // 获取挡板矩形区域（用于碰撞检测）
    Rectangle GetRect() const { return rect; }
    
    // 获取挡板中心X坐标
    // 用途：计算小球发射起始位置
    float GetCenterX() const { return rect.x + rect.width / 2; }
    
    // 获取挡板顶部Y坐标
    // 用途：计算小球发射时放置在挡板上方
    float GetTopY() const { return rect.y; }
    
    // 设置挡板宽度（用于存档加载）
    void SetWidth(float width);
};

#endif