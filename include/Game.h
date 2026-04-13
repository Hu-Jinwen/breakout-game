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

enum class GameState {
    MENU,
    PLAYING,
    PAUSED,
    GAMEOVER,
    VICTORY,
    LEADERBOARD
};

class Game {
private:
    int screenWidth;
    int screenHeight;
    
    Ball ball;
    Paddle paddle;
    std::vector<Brick> bricks;
    
    GameState currentState;
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
    
    // ========== 新增：道具系统 ==========
    std::vector<PowerUp> powerUps;
    std::vector<std::unique_ptr<PowerUpEffect>> activeEffects;
    std::vector<Ball> extraBalls;
    float ballSpeedMultiplier;
    bool isSlowed;
    
    // ========== 新增：粒子系统 ==========
    struct Particle {
        Vector2 position;
        Vector2 velocity;
        Color color;
        float life;
        float maxLife;
    };
    std::vector<Particle> particles;
    
    // ========== 排行榜 ==========
    struct ScoreEntry {
        char name[32];
        int score;
        time_t timestamp;
    };
    ScoreEntry leaderboardEntries[10];
    int leaderboardCount;
    
public:
    Game();
    ~Game();
    
    void Init();
    void Update();
    void Draw();
    void Shutdown();
    
    // ========== 新增：供道具效果调用的公共接口 ==========
    Paddle& GetPaddle() { return paddle; }
    void AddExtraBalls(int count);
    void SlowDownBalls(float factor);
    void RestoreBallSpeed();
    
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
    
    // ========== 新增：道具系统方法 ==========
    void AddPowerUp(float x, float y, PowerUpType type);
    void ApplyPowerUpEffect(PowerUpType type);
    void CheckPowerUpCollisions();
    void UpdateEffects(float dt);
    void UpdateExtraBalls(float dt);
    
    // ========== 新增：粒子系统方法 ==========
    void SpawnBrickParticles(Rectangle brickRect, Color brickColor);
    void SpawnPowerUpGlow(float x, float y, Color color);
    void UpdateParticles(float dt);
    void DrawParticles();
    void DrawExtraBalls();
};

#endif