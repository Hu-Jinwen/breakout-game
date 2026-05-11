#pragma once
#include "raylib.h"

struct GameState {
    float ballX, ballY;
    float ballSpeedX, ballSpeedY;
    float paddle1X;  // 本地玩家板
    float paddle2X;  // 对手板
    int score1, score2;
};