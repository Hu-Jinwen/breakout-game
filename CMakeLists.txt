cmake_minimum_required(VERSION 3.10)
project(breakout_week2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(raylib REQUIRED)

# ENet 配置
find_package(PkgConfig REQUIRED)
pkg_check_modules(ENET REQUIRED libenet)

# 添加 include 目录
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# ========== 收集源文件 ==========
set(SRC_FILES
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/src/Game.cpp
    ${CMAKE_SOURCE_DIR}/src/Ball.cpp
    ${CMAKE_SOURCE_DIR}/src/Paddle.cpp
    ${CMAKE_SOURCE_DIR}/src/Brick.cpp
    ${CMAKE_SOURCE_DIR}/src/PowerUp.cpp
)

# ========== 创建可执行文件 ==========
add_executable(breakout_game ${SRC_FILES})

# ========== 链接库（添加 pthread） ==========
target_include_directories(breakout_game PRIVATE ${ENET_INCLUDE_DIRS})
target_link_libraries(breakout_game raylib ${ENET_LIBRARIES} pthread)

# ========== 测试可执行文件 ==========
add_executable(test_game
    ${CMAKE_SOURCE_DIR}/tests/test.cpp
    ${CMAKE_SOURCE_DIR}/src/Ball.cpp
    ${CMAKE_SOURCE_DIR}/src/Paddle.cpp
    ${CMAKE_SOURCE_DIR}/src/Brick.cpp
    ${CMAKE_SOURCE_DIR}/src/PowerUp.cpp
)

target_link_libraries(test_game raylib pthread)
target_include_directories(test_game PRIVATE ${CMAKE_SOURCE_DIR}/include)