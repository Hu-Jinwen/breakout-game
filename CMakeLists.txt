cmake_minimum_required(VERSION 3.10)
project(breakout_week2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(raylib REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)

# 主游戏可执行文件
add_executable(breakout_game
    main.cpp
    src/Game.cpp
    src/Ball.cpp
    src/Paddle.cpp
    src/Brick.cpp
    src/PowerUp.cpp
)

target_link_libraries(breakout_game raylib)

# 测试可执行文件
add_executable(test_game
    tests/test.cpp
    src/Ball.cpp
    src/Paddle.cpp
    src/Brick.cpp
    src/PowerUp.cpp
)

target_link_libraries(test_game raylib)
target_include_directories(test_game PRIVATE ${CMAKE_SOURCE_DIR}/include)