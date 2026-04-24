#include "raylib.h"
#include "Game.h"
#include <cstring>

int main(int argc, char* argv[]) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Breakout - Multiplayer");
    SetTargetFPS(60);
    
    Game game;
    game.Init();
    
    // ===== 解析命令行参数 =====
    bool isHost = false;
    const char* serverIP = nullptr;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0) {
            isHost = true;
        } else if (strcmp(argv[i], "--client") == 0 && i + 1 < argc) {
            serverIP = argv[i + 1];
            i++;
        }
    }
    
    if (isHost) {
        game.InitNetwork(true);
        TraceLog(LOG_INFO, "Starting as HOST");
    } else if (serverIP) {
        game.InitNetwork(false, serverIP);
        TraceLog(LOG_INFO, "Starting as CLIENT, connecting to %s", serverIP);
    } else {
        TraceLog(LOG_WARNING, "No network mode specified. Use --host or --client <ip>");
        // 可以选择继续以单机模式运行
    }
    
    while (!WindowShouldClose()) {
        game.Update();
        game.Draw();
    }
    
    game.Shutdown();
    CloseWindow();
    
    return 0;
}