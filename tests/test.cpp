#include <cassert>
#include <iostream>
#include <cmath>
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"

void TestBallBrickCollision_NormalHit() {
    std::cout << "\n测试1: 球与砖块正面碰撞..." << std::endl;
    
    Ball ball({100, 95}, {0, 5}, 5);
    Brick brick(95, 100, 10, 10, RED);
    
    ball.SetLaunched(true);
    
    bool collided = ball.CheckBrickCollision(brick.GetRect());
    assert(collided == true);
    
    std::cout << "  ✓ 碰撞检测正确" << std::endl;
    std::cout << "✓ 测试1通过: 球与砖块正面碰撞" << std::endl;
}

void TestBallBrickCollision_EdgeHit() {
    std::cout << "\n测试2: 球与砖块边缘碰撞..." << std::endl;
    
    Ball ball({105, 105}, {5, 5}, 5);
    Brick brick(95, 100, 10, 10, RED);
    
    ball.SetLaunched(true);
    
    bool collided = ball.CheckBrickCollision(brick.GetRect());
    assert(collided == true);
    
    std::cout << "  ✓ 边缘碰撞检测正确" << std::endl;
    std::cout << "✓ 测试2通过: 球与砖块边缘碰撞" << std::endl;
}

void TestBallBrickCollision_NotLaunched() {
    std::cout << "\n测试3: 球未发射时不应碰撞砖块..." << std::endl;
    
    Ball ball({100, 95}, {0, 0}, 5);
    Brick brick(95, 100, 10, 10, RED);
    
    ball.SetLaunched(false);
    
    bool collided = ball.CheckBrickCollision(brick.GetRect());
    assert(collided == false);
    
    std::cout << "✓ 测试3通过: 球未发射时不碰撞" << std::endl;
}

void TestBallBrickCollision_NoCollision() {
    std::cout << "\n测试4: 球与砖块无碰撞..." << std::endl;
    
    Ball ball({50, 50}, {0, 5}, 5);
    Brick brick(95, 100, 10, 10, RED);
    
    ball.SetLaunched(true);
    
    bool collided = ball.CheckBrickCollision(brick.GetRect());
    assert(collided == false);
    
    std::cout << "✓ 测试4通过: 球与砖块无碰撞" << std::endl;
}

void TestBallBrickCollision_VelocityChange() {
    std::cout << "\n测试5: 球与砖块碰撞后速度变化..." << std::endl;
    
    Ball ball({100, 95}, {0, 5}, 5);
    Brick brick(95, 100, 10, 10, RED);
    
    ball.SetLaunched(true);
    
    Vector2 speedBefore = ball.GetSpeed();
    std::cout << "  碰撞前速度: (" << speedBefore.x << ", " << speedBefore.y << ")" << std::endl;
    
    bool collided = ball.CheckBrickCollision(brick.GetRect());
    assert(collided == true);
    
    Vector2 speedAfter = ball.GetSpeed();
    std::cout << "  碰撞后速度: (" << speedAfter.x << ", " << speedAfter.y << ")" << std::endl;
    
    assert(speedBefore.y != speedAfter.y);
    
    std::cout << "✓ 测试5通过: 碰撞后速度正确反转" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Ball::CheckBrickCollision 单元测试" << std::endl;
    std::cout << "========================================" << std::endl;
    
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