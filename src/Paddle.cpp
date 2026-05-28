#include "Paddle.h"

// 构造函数
// 初始化挡板的位置、尺寸，设置加长效果状态为未激活
Paddle::Paddle(float x, float y, float w, float h) {
    rect = { x, y, w, h };
    originalWidth = w;
    currentWidth = w;
    effectRemainingTime = 0;
    isExtended = false;
}

// 绘制挡板
// 正常状态：蓝色矩形 + 深蓝色边框
// 加长状态：半透明绿色矩形 + 深绿色边框 + 顶部绿色进度条
void Paddle::Draw() {
    // 根据是否有buff改变颜色
    // 加长效果期间使用半透明绿色，视觉反馈更明显
    Color paddleColor = isExtended ? ColorAlpha(GREEN, 0.8f) : BLUE;
    DrawRectangleRec(rect, paddleColor);
    DrawRectangleLinesEx(rect, 2, isExtended ? DARKGREEN : DARKBLUE);
    
    // 显示buff剩余时间进度条（仅在加长状态且剩余时间>0时显示）
    // 进度条位于挡板顶部上方5px处，宽度按剩余时间比例计算
    if (isExtended && effectRemainingTime > 0) {
        // 假设总持续时间为5秒（由Extend函数传入）
        // 进度条宽度 = 当前挡板宽度 * (剩余时间 / 5.0)
        int barWidth = (int)(rect.width * (effectRemainingTime / 5.0f));
        DrawRectangle(rect.x, rect.y - 5, barWidth, 3, GREEN);
    }
}

// 向左移动
// 速度参数speed单位：像素/秒
// 自动限制左边界为5px（防止超出屏幕）
void Paddle::MoveLeft(float speed) {
    rect.x -= speed;
    // 左边界限制：挡板左侧不能超出屏幕左边界5px
    if (rect.x < 5) rect.x = 5;
}

// 向右移动
// 速度参数speed单位：像素/秒
// 自动限制右边界为screenWidth - 5px（防止超出屏幕）
void Paddle::MoveRight(float speed) {
    rect.x += speed;
    // 右边界限制：挡板右侧不能超出屏幕右边界5px
    if (rect.x + rect.width > GetScreenWidth() - 5)
        rect.x = GetScreenWidth() - rect.width - 5;
}

// 应用加长效果（道具）
// 临时增加挡板宽度，持续duration秒
// 如果效果期间再次调用，会重置计时器（重新计时）
//
// 实现说明：
//   - 首次调用时保存originalWidth
//   - 每次调用都会重置effectRemainingTime为duration
//   - 调用后自动检查是否超出屏幕右边界，如超出则修正位置
void Paddle::Extend(float extraWidth, float duration) {
    // 首次调用时保存原始宽度
    if (!isExtended) {
        originalWidth = rect.width;
    }
    // 增加宽度
    rect.width = originalWidth + extraWidth;
    currentWidth = rect.width;
    effectRemainingTime = duration;
    isExtended = true;
    
    // 确保扩展后不超出屏幕右边界
    // 如果超出，将挡板向左移动使其贴合右边界
    if (rect.x + rect.width > GetScreenWidth() - 5) {
        rect.x = GetScreenWidth() - rect.width - 5;
    }
}

// 恢复原始宽度
// 将挡板宽度恢复为originalWidth，清除加长状态
// 在Extend()效果到期时自动调用
void Paddle::ResetWidth() {
    rect.width = originalWidth;
    currentWidth = originalWidth;
    effectRemainingTime = 0;
    isExtended = false;
}

// 更新状态（每帧调用）
// 减少effectRemainingTime，到期时自动调用ResetWidth()
// 必须在每帧调用，以正确处理道具效果计时
void Paddle::Update(float dt) {
    if (isExtended) {
        effectRemainingTime -= dt;
        if (effectRemainingTime <= 0) {
            ResetWidth();
        }
    }
}

// 设置挡板宽度（用于存档加载）
// 参数width：新的挡板宽度
void Paddle::SetWidth(float width) {
    rect.width = width;
}