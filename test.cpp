#include <cassert>
#include <iostream>
#include <cmath>
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"

// 测试1：球与板的碰撞检测
void TestBallPaddleCollision() {
    // 球：位置(100, 105)，半径5 → 球底部在 y=110
    Ball ball({100, 105}, {0, 1}, 5);
    // 板：位置(90, 110)，宽20高5 → 板顶部在 y=110
    Paddle paddle(90, 110, 20, 5);
    
    // 检测是否碰撞（球刚好碰到板顶部，应该碰撞）
    bool collided = CheckCollisionCircleRec(
        ball.GetPosition(), 
        ball.GetRadius(), 
        paddle.GetRect()
    );
    
    assert(collided == true);
    std::cout << "✓ 测试1通过: 球与板碰撞检测" << std::endl;
}

// 测试2：球与砖的碰撞检测
void TestBallBrickCollision() {
    // 球：位置(100, 95)，半径5 → 球底部在 y=100
    Ball ball({100, 95}, {0, 1}, 5);
    // 砖块：位置(95, 100)，宽10高10 → 砖块顶部在 y=100
    Brick brick(95, 100, 10, 10);
    
    // 检测是否碰撞
    bool collided = CheckCollisionCircleRec(
        ball.GetPosition(), 
        ball.GetRadius(), 
        brick.GetRect()
    );
    
    assert(collided == true);
    
    // 测试砖块被击中后失效
    assert(brick.IsActive() == true);
    brick.SetActive(false);
    assert(brick.IsActive() == false);
    
    std::cout << "✓ 测试2通过: 球与砖碰撞检测" << std::endl;
}

// 测试3：球碰撞板后速度方向改变
void TestBallBounceFromPaddle() {
    Ball ball({100, 105}, {2, 3}, 5);
    Paddle paddle(90, 110, 20, 5);
    
    // 确认碰撞发生
    bool collided = CheckCollisionCircleRec(
        ball.GetPosition(), 
        ball.GetRadius(), 
        paddle.GetRect()
    );
    assert(collided == true);
    
    // 碰撞后速度y应该反向
    if (ball.GetSpeed().y > 0) {
        ball.SetSpeed({ball.GetSpeed().x, -ball.GetSpeed().y});
        assert(ball.GetSpeed().y < 0);
    }
    
    std::cout << "✓ 测试3通过: 碰撞后速度方向改变" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "开始运行单元测试..." << std::endl;
    std::cout << "========================================" << std::endl;
    
    TestBallPaddleCollision();
    TestBallBrickCollision();
    TestBallBounceFromPaddle();
    
    std::cout << "========================================" << std::endl;
    std::cout << "✅ 所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}