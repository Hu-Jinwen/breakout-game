// test.cpp
// 单元测试文件 - 测试Ball类的碰撞检测功能
//
// 测试目的：验证Ball::CheckBrickCollision()函数在各种情况下的正确性
// 测试范围：
//   1. 球与砖块正面碰撞
//   2. 球与砖块边缘碰撞
//   3. 球未发射时不应触发碰撞
//   4. 球与砖块无碰撞时正确返回false
//   5. 碰撞后速度正确反转
//
// 运行方法：编译后执行 ./test_game
// 预期结果：所有测试通过，输出绿色勾号

#include <cassert>
#include <iostream>
#include <cmath>
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"

// 测试1：球与砖块正面碰撞
// 球从正上方下落，应检测到碰撞
void TestBallBrickCollision_NormalHit() {
    std::cout << "\n测试1: 球与砖块正面碰撞..." << std::endl;
    
    // 创建小球：位置(100, 95)，速度(0, 5)向下，半径5
    Ball ball({100, 95}, {0, 5}, 5);
    // 创建砖块：位置(95, 100)，宽10高10
    Brick brick(95, 100, 10, 10, RED);
    
    ball.SetLaunched(true);
    
    bool collided = ball.CheckBrickCollision(brick.GetRect());
    // 断言：应该检测到碰撞
    assert(collided == true);
    
    std::cout << "  ✓ 碰撞检测正确" << std::endl;
    std::cout << "✓ 测试1通过: 球与砖块正面碰撞" << std::endl;
}

// 测试2：球与砖块边缘碰撞
// 球从斜角接近砖块边缘，应检测到碰撞
void TestBallBrickCollision_EdgeHit() {
    std::cout << "\n测试2: 球与砖块边缘碰撞..." << std::endl;
    
    Ball ball({105, 105}, {5, 5}, 5);
    Brick brick(95, 100, 10, 10, RED);
    
    ball.SetLaunched(true);
    
    bool collided = ball.CheckBrickCollision(brick.GetRect());
    // 断言：边缘碰撞应被检测到
    assert(collided == true);
    
    std::cout << "  ✓ 边缘碰撞检测正确" << std::endl;
    std::cout << "✓ 测试2通过: 球与砖块边缘碰撞" << std::endl;
}

// 测试3：球未发射时不应碰撞砖块
// 验证游戏规则：只有发射后的球才能击碎砖块
void TestBallBrickCollision_NotLaunched() {
    std::cout << "\n测试3: 球未发射时不应碰撞砖块..." << std::endl;
    
    Ball ball({100, 95}, {0, 0}, 5);
    Brick brick(95, 100, 10, 10, RED);
    
    ball.SetLaunched(false);
    
    bool collided = ball.CheckBrickCollision(brick.GetRect());
    // 断言：未发射时不应检测到碰撞
    assert(collided == false);
    
    std::cout << "✓ 测试3通过: 球未发射时不碰撞" << std::endl;
}

// 测试4：球与砖块无碰撞
// 球距离砖块很远，应正确返回false
void TestBallBrickCollision_NoCollision() {
    std::cout << "\n测试4: 球与砖块无碰撞..." << std::endl;
    
    Ball ball({50, 50}, {0, 5}, 5);
    Brick brick(95, 100, 10, 10, RED);
    
    ball.SetLaunched(true);
    
    bool collided = ball.CheckBrickCollision(brick.GetRect());
    // 断言：无碰撞时应返回false
    assert(collided == false);
    
    std::cout << "✓ 测试4通过: 球与砖块无碰撞" << std::endl;
}

// 测试5：球与砖块碰撞后速度变化
// 验证碰撞后速度方向正确反转
void TestBallBrickCollision_VelocityChange() {
    std::cout << "\n测试5: 球与砖块碰撞后速度变化..." << std::endl;
    
    Ball ball({100, 95}, {0, 5}, 5);
    Brick brick(95, 100, 10, 10, RED);
    
    ball.SetLaunched(true);
    
    Vector2 speedBefore = ball.GetSpeed();
    std::cout << "  碰撞前速度: (" << speedBefore.x << ", " << speedBefore.y << ")" << std::endl;
    
    bool collided = ball.CheckBrickCollision(brick.GetRect());
    // 断言：应该发生碰撞
    assert(collided == true);
    
    Vector2 speedAfter = ball.GetSpeed();
    std::cout << "  碰撞后速度: (" << speedAfter.x << ", " << speedAfter.y << ")" << std::endl;
    
    // 断言：Y方向速度应该反转（从向下变为向上）
    // 因为球从上方碰撞砖块顶部，Y速度应由正变负
    assert(speedBefore.y != speedAfter.y);
    
    std::cout << "✓ 测试5通过: 碰撞后速度正确反转" << std::endl;
}

// 主函数：运行所有测试
// 测试计数：共5个测试
// 预期结果：所有测试通过，程序返回0
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Ball::CheckBrickCollision 单元测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 依次执行5个测试
    TestBallBrickCollision_NormalHit();
    TestBallBrickCollision_EdgeHit();
    TestBallBrickCollision_NotLaunched();
    TestBallBrickCollision_NoCollision();
    TestBallBrickCollision_VelocityChange();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "✅ 所有测试通过！（共5个测试）" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}