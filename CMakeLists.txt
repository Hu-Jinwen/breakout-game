cmake_minimum_required(VERSION 3.10)
project(breakout_week2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(raylib REQUIRED)

# ENet 配置
find_package(PkgConfig REQUIRED)
pkg_check_modules(ENET REQUIRED libenet)

include_directories(${CMAKE_SOURCE_DIR}/include)

# ========== 1. 先创建目标 ==========
add_executable(breakout_game
    main.cpp
    src/Game.cpp
    src/Ball.cpp
    src/Paddle.cpp
    src/Brick.cpp
    src/PowerUp.cpp
)

# ========== 2. 再给目标添加属性和链接库 ==========
target_include_directories(breakout_game PRIVATE ${ENET_INCLUDE_DIRS})
target_link_libraries(breakout_game raylib ${ENET_LIBRARIES})

# 测试可执行文件（可选）
add_executable(test_game
    tests/test.cpp
    src/Ball.cpp
    src/Paddle.cpp
    src/Brick.cpp
    src/PowerUp.cpp
)

target_link_libraries(test_game raylib)
target_include_directories(test_game PRIVATE ${CMAKE_SOURCE_DIR}/include)