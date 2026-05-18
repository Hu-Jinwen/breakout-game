#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include "PowerUp.h"
#include <vector>
#include <string>
#include <memory>
#include <enet/enet.h>
#include "AsyncResourceLoader.h"
#include <fstream>      
#include <nlohmann/json.hpp>  

using json = nlohmann::json;  

enum class GameState {
    MENU,
    LEVEL_SELECT,  
    PLAYING,
    PAUSED,
    GAMEOVER,
    VICTORY,
    LEADERBOARD
};

// 关卡数据结构
struct LevelConfig {
    int levelNumber;
    std::string levelName;
    std::string difficulty;
    float ballSpeedMultiplier;      // 球速倍率
    float paddleSpeedMultiplier;    // 球拍速度倍率
    int brickRows;
    int brickCols;
    int scoreMultiplier;             // 分数倍率
    float powerUpDropRate;           // 道具掉落率
    int maxLives;                    // 初始生命值
    std::vector<std::pair<int, int>> brickPositions; // 自定义砖块位置（用于特殊布局）
    int layoutType;                  // 0=标准, 1=菱形, 2=金字塔, 3=波浪, 4=城堡
};

// 网络同步用的结构体（改个名字，避免冲突）
struct NetworkGameState {
    float ballX, ballY;
    float ballSpeedX, ballSpeedY;
    float paddle1X;
    float paddle2X;
    int score1, score2;
};

class Game {
private:
    int screenWidth;
    int screenHeight;
    
    Ball ball;
    Paddle paddle;
    std::vector<Brick> bricks;
    
    GameState currentState;      // 枚举（MENU, PLAYING...）
    GameState previousState;
    int lives;
    int score;
    float gameTime;
    bool showLeaderboard;
    int playerRank;
    
    float brickWidth;
    float brickHeight;
    float startX;
    float startY;
    float spacing;
    Color brickColors[8];
    
    float ballRadius;
    float gravity;
    float maxSpeed;
    float bounceForce;
    float launchSpeed;
    float paddleSpeed;
    float paddleBoostSpeed;
    float paddleWidth;
    float paddleHeight;
    int initialLives;
    int scorePerBrick;
    int brickRows;
    int brickCols;
    int deathPenalty;
    float powerUpDropRate;
    
    // 关卡系统
    int currentLevel;
    LevelConfig levels[3];
    int selectedLevel;
    
    // 道具系统
    std::vector<PowerUp> powerUps;
    std::vector<std::unique_ptr<PowerUpEffect>> activeEffects;
    std::vector<Ball> extraBalls;
    float ballSpeedMultiplier;
    bool isSlowed;
    
    // ========== 优化：对象池粒子系统（替换原std::vector<Particle>） ==========
    static constexpr int MAX_PARTICLES = 500;  // 最大粒子数
    
    struct ParticlePooled {
        Vector2 position;
        Vector2 velocity;
        Color color;
        float life;
        float maxLife;
        bool active;  // 是否激活
    };
    
    ParticlePooled pooledParticles[MAX_PARTICLES];  // 静态数组，无动态分配
    int activeParticleCount;
    
    // ========== 优化：空间划分（网格法） ==========
    struct GridCell {
        std::vector<int> brickIndices;  // 存储砖块索引
    };
    
    static constexpr int GRID_COLS = 12;   // 网格列数
    static constexpr int GRID_ROWS = 8;    // 网格行数
    static constexpr float CELL_WIDTH = 800.0f / GRID_COLS;   // 约66.7px
    static constexpr float CELL_HEIGHT = 600.0f / GRID_ROWS;  // 75px
    
    GridCell grid[GRID_COLS][GRID_ROWS];
    bool useSpatialPartition;  // 是否启用空间划分（可用于对比测试）
    
    // ========== 优化：性能测量 ==========
    double lastFrameTime;
    float collisionTimeMs;
    float particleTimeMs;
    float totalFrameTimeMs;

    // 排行榜
    struct ScoreEntry {
        char name[32];
        int score;
        time_t timestamp;
    };
    ScoreEntry leaderboardEntries[10];
    int leaderboardCount;

    // 网络相关
    ENetHost* netHost;
    ENetPeer* netPeer;
    bool isHost;
    bool isConnected;
    float lastSendTime;
    float lastRecvTime;
    
    // 状态同步与插值（使用新名字）
    NetworkGameState netCurrentState;    // 改名
    NetworkGameState netTargetState;     // 改名
    double lastStateTime;
    double nextStateTime;
    
    float opponentPaddleX;
    int opponentScore;

    // 异步加载相关
    AsyncResourceLoader* asyncLoader;
    TextureCache textureCache;
    Texture2D loadedDemoTexture;
    bool showLoadedTexture;
    float textureDisplayTimer;
    bool isLoadingRequested;

     // ========== 新增：存档系统 ==========
    bool SaveGame(const std::string& filename = "savegame.json");
    bool LoadGame(const std::string& filename = "savegame.json");
    bool SaveExists() const;
    json LoadJSONFromFile(const std::string& path);
    void InitBricksFromJSON(const json& config);
    
    // 新增：游戏启动时的存档检测方法
    void CheckForSaveFile();

public:
    Game();
    ~Game();
    
    void Init();
    void Update();
    void Draw();
    void Shutdown();
    void InitNetwork(bool asHost, const char* serverIP = nullptr);
    
    // 道具效果接口
    Paddle& GetPaddle() { return paddle; }
    void AddExtraBalls(int count);
    void SlowDownBalls(float factor);
    void RestoreBallSpeed();

    // 任务相关方法
    void RequestAsyncLoad(const std::string& texturePath);
    bool IsAsyncLoading() const;
    float GetAsyncLoadProgress() const;

    // 网络同步相关方法
    void SendGameStateToClient();
    void ReceiveGameStateFromHost();
    float GetOpponentPaddleX() const { return opponentPaddleX; }
    int GetOpponentScore() const { return opponentScore; }
    bool IsNetworkGame() const { return netHost != nullptr && isConnected; }
    bool IsHost() const { return isHost; }

    // 关卡系统方法
    void InitLevels();
    void LoadLevel(int level);
    void DrawLevelSelect();

private:
    void LoadConfig(const std::string& path);
    void InitBricks();
    void InitBricksByLayout(int layoutType);  // 新增：根据布局类型创建砖块
    void LoadLeaderboard();
    void SaveLeaderboard();
    
    void HandleInput();
    void UpdateGame();
    void CheckCollisions();
    void CheckWinCondition();
    void ResetGame();
    
    void DrawUI();
    void DrawMenu();
    void DrawLeaderboard();
    void DrawPaused();
    void DrawGameOver();
    void DrawVictory();
    
    void ChangeState(GameState newState);
    void OnEnterState(GameState state);
    void OnExitState(GameState state);
    
    bool CanEnterLeaderboard(int score);
    int AddToLeaderboard(const char* name, int score);
    
    float CalculateMultiplier();
    
    // 道具系统方法
    void AddPowerUp(float x, float y, PowerUpType type);
    void ApplyPowerUpEffect(PowerUpType type);
    void CheckPowerUpCollisions();
    void UpdateEffects(float dt);
    void UpdateExtraBalls(float dt);
    
    // ========== 优化：对象池粒子系统方法（替换原粒子方法） ==========
    void SpawnParticlePooled(Vector2 pos, Vector2 vel, Color color, float lifetime);
    void UpdateParticlesPooled(float dt);
    void DrawParticlesPooled();
    
    // ========== 优化：空间划分方法 ==========
    void BuildSpatialGrid();                    // 构建空间网格
    void GetNearbyBricks(const Ball& ball, std::vector<int>& outIndices);  // 获取相邻网格的砖块
    
    // ========== 优化：性能测量 ==========
    void BeginPerformanceMeasure();
    void EndPerformanceMeasure(const char* operationName);
    void DrawPerformanceUI();  // 在屏幕上显示性能数据

    // 原粒子系统方法（已废弃，保留声明以防编译错误，实际不再使用）
    // void SpawnBrickParticles(Rectangle brickRect, Color brickColor);
    // void SpawnPowerUpGlow(float x, float y, Color color);
    // void UpdateParticles(float dt);
    // void DrawParticles();
    
    // 新版本粒子生成方法（使用对象池）
    void SpawnBrickParticles(Rectangle brickRect, Color brickColor);
    void SpawnPowerUpGlow(float x, float y, Color color);
    void DrawExtraBalls();

    void UpdateNetwork();

    void UpdateAsyncLoading();
    void DrawAsyncLoadingUI();

    // 网络同步定时器
    float networkSendTimer;
    float networkReceiveTimeout;
    
    // 客户端插值相关
    float interpolatedBallX;
    float interpolatedBallY;
    float interpolationAlpha;
};

#endif