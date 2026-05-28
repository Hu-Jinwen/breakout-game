// main.cpp
// 游戏主入口文件
//
// 职责：
//   - 初始化Raylib窗口
//   - 创建Game游戏实例
//   - 解析命令行参数（单人/主机/客户端模式）
//   - 运行主游戏循环（Update/Draw）
//   - 清理资源
//
// 命令行参数：
//   ./breakout_game              # 单人模式
//   ./breakout_game --host       # 多人模式-作为主机
//   ./breakout_game --client <IP> # 多人模式-作为客户端
//   ./breakout_game --help       # 显示帮助信息
//
// 注意事项：
//   - 多人模式需要ENet库支持，如果编译时未启用则自动降级为单人模式
//   - 主循环保持在60FPS（由SetTargetFPS(60)控制）

#include "raylib.h"
#include "Game.h"

// 条件编译：仅在定义了USE_MULTIPLAYER宏时才包含网络相关头文件
// 这样即使没有ENet库，游戏也能正常编译（只是没有多人功能）
#ifdef USE_MULTIPLAYER
#include <cstring>
#endif

// 主函数
// 程序入口点，负责初始化和主循环
int main(int argc, char* argv[]) {
    // 窗口尺寸常量
    const int screenWidth = 800;
    const int screenHeight = 600;
    
    // 初始化Raylib窗口
    // 参数：宽度, 高度, 窗口标题
    InitWindow(screenWidth, screenHeight, "Breakout - Classic Arcade Game");
    // 设置目标帧率为60FPS，使游戏速度在不同配置的电脑上保持一致
    SetTargetFPS(60);
    
    // 创建游戏实例
    Game game;
    // 初始化游戏（加载配置、初始化状态等）
    game.Init();
    
    // ===== 解析命令行参数 =====
    bool isHost = false;
    const char* serverIP = nullptr;
    
#ifdef USE_MULTIPLAYER
    // 遍历命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0) {
            // 主机模式
            isHost = true;
        } else if (strcmp(argv[i], "--client") == 0 && i + 1 < argc) {
            // 客户端模式，需要指定服务器IP
            serverIP = argv[i + 1];
            i++;  // 跳过IP参数
        } else if (strcmp(argv[i], "--help") == 0) {
            // 显示帮助信息
            TraceLog(LOG_INFO, "========================================");
            TraceLog(LOG_INFO, "BREAKOUT GAME - Usage:");
            TraceLog(LOG_INFO, "========================================");
            TraceLog(LOG_INFO, "  Single player:   ./breakout_game");
            TraceLog(LOG_INFO, "  Host mode:       ./breakout_game --host");
            TraceLog(LOG_INFO, "  Client mode:     ./breakout_game --client <server_ip>");
            TraceLog(LOG_INFO, "========================================");
            CloseWindow();
            return 0;
        }
    }
    
    // 根据命令行参数初始化网络
    if (isHost) {
        // 主机模式：创建房间，等待客户端连接
        game.InitNetwork(true);
        TraceLog(LOG_INFO, "========================================");
        TraceLog(LOG_INFO, "Starting as HOST - Waiting for client...");
        TraceLog(LOG_INFO, "========================================");
    } else if (serverIP) {
        // 客户端模式：连接到指定服务器
        game.InitNetwork(false, serverIP);
        TraceLog(LOG_INFO, "========================================");
        TraceLog(LOG_INFO, "Starting as CLIENT - Connected to %s", serverIP);
        TraceLog(LOG_INFO, "========================================");
    } else {
        // 无参数：单人模式
        TraceLog(LOG_INFO, "========================================");
        TraceLog(LOG_INFO, "Starting in SINGLE PLAYER mode");
        TraceLog(LOG_INFO, "Use --host or --client <ip> for multiplayer");
        TraceLog(LOG_INFO, "========================================");
    }
#else
    // 多人模式未编译（ENet未找到）
    // 显示提示信息，但游戏仍可正常运行（仅单人模式）
    if (argc > 1) {
        TraceLog(LOG_WARNING, "========================================");
        TraceLog(LOG_WARNING, "Multiplayer mode not available in this build");
        TraceLog(LOG_WARNING, "========================================");
    }
    
    TraceLog(LOG_INFO, "========================================");
    TraceLog(LOG_INFO, "Starting BREAKOUT GAME - Single Player");
    TraceLog(LOG_INFO, "========================================");
#endif
    
    // ===== 主游戏循环 =====
    // 循环条件：窗口未被关闭（用户未点击关闭按钮或按ESC）
    while (!WindowShouldClose()) {
        // 更新游戏逻辑（输入处理、碰撞检测、状态更新等）
        game.Update();
        // 绘制所有游戏内容（每帧调用）
        game.Draw();
    }
    
    // ===== 清理资源 =====
    // 释放网络资源、保存排行榜、卸载纹理等
    game.Shutdown();
    // 关闭Raylib窗口
    CloseWindow();
    
    // 程序正常退出
    return 0;
}