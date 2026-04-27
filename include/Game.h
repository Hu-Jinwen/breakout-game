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

enum class GameState {
    MENU,
    PLAYING,
    PAUSED,
    GAMEOVER,
    VICTORY,
    LEADERBOARD
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
    
    // 道具系统
    std::vector<PowerUp> powerUps;
    std::vector<std::unique_ptr<PowerUpEffect>> activeEffects;
    std::vector<Ball> extraBalls;
    float ballSpeedMultiplier;
    bool isSlowed;
    
    // 粒子系统
    struct Particle {
        Vector2 position;
        Vector2 velocity;
        Color color;
        float life;
        float maxLife;
    };
    std::vector<Particle> particles;
    
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

private:
    void LoadConfig(const std::string& path);
    void InitBricks();
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
    
    // 粒子系统方法
    void SpawnBrickParticles(Rectangle brickRect, Color brickColor);
    void SpawnPowerUpGlow(float x, float y, Color color);
    void UpdateParticles(float dt);
    void DrawParticles();
    void DrawExtraBalls();

    void UpdateNetwork();

    void UpdateAsyncLoading();
    void DrawAsyncLoadingUI();


};

#endif