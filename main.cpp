#include "raylib.h"
#include "Game.h"

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Breakout - Enhanced");
    SetTargetFPS(60);
    
    Game game;
    game.Init();
    
    while (!WindowShouldClose()) {
        game.Update();
        game.Draw();
    }
    
    game.Shutdown();
    CloseWindow();
    
    return 0;
}