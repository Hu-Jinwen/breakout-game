#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>
#include <string>

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
};

#endif