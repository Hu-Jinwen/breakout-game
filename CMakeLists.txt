cmake_minimum_required(VERSION 3.10)
project(breakout_week2)

find_package(raylib REQUIRED)

add_executable(breakout_week2
    main.cpp
    Ball.cpp
    Paddle.cpp
    Brick.cpp
)

target_link_libraries(breakout_week2 raylib)
