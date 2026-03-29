#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>
#include <string>
#include <cmath> 

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "打砖块2D - 彩色版");
    
    // 游戏状态
    int lives = 3;
    int score = 0;
    bool gameRunning = true;
    bool gameWin = false;
    
    // 创建对象
    Ball ball({screenWidth/2, screenHeight/2}, {3.5f, 3.5f}, 8);
    Paddle paddle(screenWidth/2 - 50, screenHeight - 50, 100, 15);
    
    // 砖块参数
    float brickWidth = 70;
    float brickHeight = 25;
    float startX = 45;
    float startY = 80;
    float spacing = 5;
    
    std::vector<Brick> bricks;
    
    // 定义颜色数组（彩虹色）
    Color brickColors[] = {
        RED, ORANGE, YELLOW, GREEN, SKYBLUE, BLUE, PURPLE, PINK
    };
    
    // 创建彩色砖块（8行，每行不同颜色）
    for (int row = 0; row < 8; row++) {
        Color rowColor = brickColors[row % 8];
        for (int col = 0; col < 10; col++) {
            bricks.emplace_back(
                startX + col * (brickWidth + spacing),
                startY + row * (brickHeight + spacing),
                brickWidth, brickHeight,
                rowColor  // 每行使用不同颜色
            );
        }
    }
    
    SetTargetFPS(60);
    
    while (!WindowShouldClose()) {
        // 更新逻辑
        if (gameRunning && !gameWin) {
            // 移动球
            ball.Move();
            
            // 边界碰撞（不包括底部）
            ball.BounceEdge(screenWidth, screenHeight);
            
            // 检查球是否掉出底部
            if (ball.GetPosition().y + ball.GetRadius() >= screenHeight) {
                lives--;
                if (lives <= 0) {
                    gameRunning = false;
                } else {
                    ball.Reset({screenWidth/2, screenHeight/2}, {3.5f, 3.5f});
                    paddle = Paddle(screenWidth/2 - 50, screenHeight - 50, 100, 15);
                }
            }
            
            // 移动板
            if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(6);
            if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(6);
            
            // 球与板的碰撞
            if (CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), paddle.GetRect())) {
                if (ball.GetSpeed().y > 0) {
                    // 根据碰撞点偏移改变水平速度
                    float offset = ball.GetPosition().x - (paddle.GetRect().x + paddle.GetRect().width/2);
                    float newSpeedX = ball.GetSpeed().x + offset * 0.12f;
                    
                    // 限制速度范围
                    if (newSpeedX > 5.5f) newSpeedX = 5.5f;
                    if (newSpeedX < -5.5f) newSpeedX = -5.5f;
                    
                    ball.SetSpeed({newSpeedX, -fabs(ball.GetSpeed().y)});
                    ball.BounceFromRect(paddle.GetRect());  // 使用精确碰撞
                }
            }
            
            // 球与砖的碰撞（改进版）
            for (auto& brick : bricks) {
                if (brick.IsActive() && 
                    CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), brick.GetRect())) {
                    
                    brick.SetActive(false);
                    score += 10;
                    
                    // 使用精确的反弹计算
                    ball.BounceFromRect(brick.GetRect());
                    
                    // 避免一次碰撞多个砖块，跳出循环
                    break;
                }
            }
            
            // 检查胜利条件
            bool allBricksDestroyed = true;
            for (auto& brick : bricks) {
                if (brick.IsActive()) {
                    allBricksDestroyed = false;
                    break;
                }
            }
            if (allBricksDestroyed) {
                gameWin = true;
                gameRunning = false;
            }
        }
        
        // 绘制
        BeginDrawing();
        ClearBackground(BLACK);  // 黑色背景更酷
        
        // 绘制墙壁（渐变效果）
        DrawRectangle(0, 0, screenWidth, 5, GRAY);
        DrawRectangle(0, 0, 5, screenHeight, GRAY);
        DrawRectangle(screenWidth-5, 0, 5, screenHeight, GRAY);
        
        // 游戏对象
        ball.Draw();
        paddle.Draw();
        for (auto& brick : bricks) brick.Draw();
        
        // 绘制UI（带阴影效果）
        DrawText(TextFormat("Score: %d", score), 15, 12, 20, WHITE);
        DrawText(TextFormat("Lives: %d", lives), screenWidth - 110, 12, 20, WHITE);
        
        // 绘制提示信息
        DrawText("Use LEFT/RIGHT arrows to move paddle", screenWidth/2 - 200, screenHeight - 30, 15, GRAY);
        
        // 游戏结束画面
        if (!gameRunning) {
            // 半透明背景
            DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.7f));
            
            if (gameWin) {
                DrawText("YOU WIN!", screenWidth/2 - 80, screenHeight/2 - 40, 50, GREEN);
                DrawText(TextFormat("Final Score: %d", score), screenWidth/2 - 100, screenHeight/2 + 20, 30, YELLOW);
            } else {
                DrawText("GAME OVER!", screenWidth/2 - 100, screenHeight/2 - 40, 50, RED);
                DrawText(TextFormat("Final Score: %d", score), screenWidth/2 - 100, screenHeight/2 + 20, 30, YELLOW);
            }
            DrawText("Press R to Restart", screenWidth/2 - 110, screenHeight/2 + 80, 25, WHITE);
            
            if (IsKeyPressed(KEY_R)) {
                // 重置游戏
                lives = 3;
                score = 0;
                gameRunning = true;
                gameWin = false;
                ball.Reset({screenWidth/2, screenHeight/2}, {3.5f, 3.5f});
                paddle = Paddle(screenWidth/2 - 50, screenHeight - 50, 100, 15);
                
                // 重新创建彩色砖块
                bricks.clear();
                for (int row = 0; row < 8; row++) {
                    Color rowColor = brickColors[row % 8];
                    for (int col = 0; col < 10; col++) {
                        bricks.emplace_back(
                            startX + col * (brickWidth + spacing),
                            startY + row * (brickHeight + spacing),
                            brickWidth, brickHeight,
                            rowColor
                        );
                    }
                }
            }
        }
        
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}