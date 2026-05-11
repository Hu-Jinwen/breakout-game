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
        } else if (strcmp(argv[i], "--help") == 0) {
            TraceLog(LOG_INFO, "Usage:");
            TraceLog(LOG_INFO, "  Single player: ./breakout_game");
            TraceLog(LOG_INFO, "  Host mode:     ./breakout_game --host");
            TraceLog(LOG_INFO, "  Client mode:   ./breakout_game --client <server_ip>");
            CloseWindow();
            return 0;
        }
    }
    
    if (isHost) {
        game.InitNetwork(true);
        TraceLog(LOG_INFO, "========================================");
        TraceLog(LOG_INFO, "Starting as HOST");
        TraceLog(LOG_INFO, "Wait for client to connect...");
        TraceLog(LOG_INFO, "========================================");
    } else if (serverIP) {
        game.InitNetwork(false, serverIP);
        TraceLog(LOG_INFO, "========================================");
        TraceLog(LOG_INFO, "Starting as CLIENT, connecting to %s", serverIP);
        TraceLog(LOG_INFO, "========================================");
    } else {
        TraceLog(LOG_INFO, "Starting in SINGLE PLAYER mode");
        TraceLog(LOG_INFO, "Use --host or --client <ip> for multiplayer");
    }
    
    while (!WindowShouldClose()) {
        game.Update();
        game.Draw();
    }
    
    game.Shutdown();
    CloseWindow();
    
    return 0;
}