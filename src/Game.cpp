#include "Game.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <cmath>

Game::Game() 
    : screenWidth(800), screenHeight(600)
    , ball({0, 0}, {0, 0}, 8)
    , paddle(0, 0, 100, 15)
    , currentState(GameState::MENU)
    , previousState(GameState::MENU)
    , lives(3)
    , score(0)
    , gameTime(0)
    , showLeaderboard(false)
    , playerRank(0)
    , brickWidth(70)
    , brickHeight(25)
    , startX(45)
    , startY(80)
    , spacing(5)
    , ballRadius(8)
    , gravity(0.05f)
    , maxSpeed(12.0f)
    , bounceForce(0.3f)
    , launchSpeed(6.5f)
    , paddleSpeed(9.0f)
    , paddleBoostSpeed(15.0f)
    , paddleWidth(100)
    , paddleHeight(15)
    , initialLives(3)
    , scorePerBrick(10)
    , brickRows(8)
    , brickCols(10)
    , deathPenalty(50)
    , leaderboardCount(0)
{
    brickColors[0] = RED;
    brickColors[1] = ORANGE;
    brickColors[2] = YELLOW;
    brickColors[3] = GREEN;
    brickColors[4] = SKYBLUE;
    brickColors[5] = BLUE;
    brickColors[6] = PURPLE;
    brickColors[7] = PINK;
    
    memset(leaderboardEntries, 0, sizeof(leaderboardEntries));
}

Game::~Game() {
}

void Game::LoadConfig(const std::string& path) {
    TraceLog(LOG_INFO, "Using default configuration values");
}

void Game::Init() {
    LoadConfig("config.json");
    
    ball = Ball({(float)screenWidth/2, (float)screenHeight/2}, {0, 0}, ballRadius);
    paddle = Paddle(screenWidth/2 - paddleWidth/2, screenHeight - 50, paddleWidth, paddleHeight);
    
    InitBricks();
    LoadLeaderboard();
    
    srand((unsigned int)time(nullptr));
    
    TraceLog(LOG_INFO, "Game initialized. Initial state: MENU");
}

void Game::InitBricks() {
    bricks.clear();
    for (int row = 0; row < brickRows; row++) {
        Color rowColor = brickColors[row % 8];
        for (int col = 0; col < brickCols; col++) {
            bricks.emplace_back(
                startX + col * (brickWidth + spacing),
                startY + row * (brickHeight + spacing),
                brickWidth, brickHeight,
                rowColor
            );
        }
    }
}

void Game::HandleInput() {
    if (IsKeyPressed(KEY_R)) {
        ResetGame();
        ChangeState(GameState::PLAYING);
        return;
    }
    
    switch (currentState) {
        case GameState::MENU:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                ChangeState(GameState::PLAYING);
                ResetGame();
            }
            if (IsKeyPressed(KEY_L)) {
                ChangeState(GameState::LEADERBOARD);
            }
            break;
            
        case GameState::PLAYING: {
            if (IsKeyPressed(KEY_P)) {
                ChangeState(GameState::PAUSED);
            }
            if (IsKeyPressed(KEY_L)) {
                ChangeState(GameState::LEADERBOARD);
            }
            
            float speed = paddleSpeed;
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                speed = paddleBoostSpeed;
            }
            if (IsKeyDown(KEY_LEFT)) {
                paddle.MoveLeft(speed);
            }
            if (IsKeyDown(KEY_RIGHT)) {
                paddle.MoveRight(speed);
            }
            
            if (IsKeyPressed(KEY_SPACE) && !ball.IsLaunched()) {
                float paddleCenterX = paddle.GetRect().x + paddle.GetRect().width / 2;
                float paddleTopY = paddle.GetRect().y;
                ball.Launch(paddleCenterX, paddleTopY, paddle.GetRect().width);
                TraceLog(LOG_INFO, "Ball launched!");
            }
            break;
        }
            
        case GameState::PAUSED:
            if (IsKeyPressed(KEY_P)) {
                ChangeState(GameState::PLAYING);
            }
            if (IsKeyPressed(KEY_L)) {
                ChangeState(GameState::LEADERBOARD);
            }
            break;
            
        case GameState::LEADERBOARD:
            if (IsKeyPressed(KEY_L) || IsKeyPressed(KEY_ESCAPE)) {
                ChangeState(previousState);
            }
            break;
            
        case GameState::GAMEOVER:
        case GameState::VICTORY:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                ChangeState(GameState::MENU);
            }
            break;
            
        default:
            break;
    }
}

void Game::ChangeState(GameState newState) {
    if (currentState == newState) return;
    
    OnExitState(currentState);
    previousState = currentState;
    TraceLog(LOG_INFO, "State transition: %d -> %d", (int)currentState, (int)newState);
    currentState = newState;
    OnEnterState(currentState);
}

void Game::OnEnterState(GameState state) {
    switch (state) {
        case GameState::MENU:
            TraceLog(LOG_INFO, "Entering MENU state");
            break;
        case GameState::PLAYING:
            TraceLog(LOG_INFO, "Entering PLAYING state");
            break;
        case GameState::PAUSED:
            TraceLog(LOG_INFO, "Game paused");
            break;
        case GameState::LEADERBOARD:
            TraceLog(LOG_INFO, "Showing leaderboard");
            break;
        case GameState::GAMEOVER:
            TraceLog(LOG_INFO, "Game Over! Final score: %d", score);
            if (CanEnterLeaderboard(score)) {
                playerRank = AddToLeaderboard("Player", score);
            }
            break;
        case GameState::VICTORY:
            TraceLog(LOG_INFO, "Victory! Final score: %d", score);
            if (CanEnterLeaderboard(score)) {
                playerRank = AddToLeaderboard("Player", score);
            }
            break;
        default:
            break;
    }
}

void Game::OnExitState(GameState state) {
    switch (state) {
        case GameState::PLAYING:
            break;
        case GameState::PAUSED:
            TraceLog(LOG_INFO, "Resuming game");
            break;
        default:
            break;
    }
}

void Game::UpdateGame() {
    if (currentState != GameState::PLAYING) return;
    
    if (ball.IsLaunched()) {
        gameTime += GetFrameTime();
    }
    
    if (!ball.IsLaunched()) {
        float paddleCenterX = paddle.GetRect().x + paddle.GetRect().width / 2;
        float paddleTopY = paddle.GetRect().y;
        ball.FollowPaddle(paddleCenterX, paddleTopY);
    }
    
    ball.Move();
    ball.ApplyGravity();
    CheckCollisions();
    CheckWinCondition();
}

void Game::CheckCollisions() {
    if (!ball.IsLaunched()) return;
    
    ball.BounceEdge(screenWidth, screenHeight);
    
    if (ball.GetPosition().y + ball.GetRadius() >= screenHeight) {
        lives--;
        score = std::max(0, score - deathPenalty);
        
        if (lives <= 0) {
            ChangeState(GameState::GAMEOVER);
        } else {
            float paddleCenterX = paddle.GetRect().x + paddle.GetRect().width / 2;
            float paddleTopY = paddle.GetRect().y;
            ball.ResetToPaddle(paddleCenterX, paddleTopY);
            ball.SetLaunched(false);
            ball.SetSpeed({0, 0});
        }
        return;
    }
    
    if (CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), paddle.GetRect())) {
        if (ball.GetSpeed().y > 0) {
            ball.BouncePaddle(paddle.GetRect());
        }
    }
    
    for (auto& brick : bricks) {
        if (brick.IsActive()) {
            if (ball.CheckBrickCollision(brick.GetRect())) {
                brick.SetActive(false);
                int addScore = (int)(scorePerBrick * CalculateMultiplier());
                score += addScore;
                break;
            }
        }
    }
}

void Game::CheckWinCondition() {
    bool allBricksDestroyed = true;
    for (auto& brick : bricks) {
        if (brick.IsActive()) {
            allBricksDestroyed = false;
            break;
        }
    }
    
    if (allBricksDestroyed) {
        ChangeState(GameState::VICTORY);
    }
}

void Game::ResetGame() {
    lives = initialLives;
    score = 0;
    gameTime = 0;
    playerRank = 0;
    
    ball = Ball({(float)screenWidth/2, (float)screenHeight/2}, {0, 0}, ballRadius);
    ball.SetLaunched(false);
    
    paddle = Paddle(screenWidth/2 - paddleWidth/2, screenHeight - 50, paddleWidth, paddleHeight);
    
    InitBricks();
    
    TraceLog(LOG_INFO, "Game reset");
}

float Game::CalculateMultiplier() {
    float multiplier = 3.0f - gameTime * 0.03f;
    if (multiplier < 1.0f) multiplier = 1.0f;
    return multiplier;
}

void Game::DrawUI() {
    DrawRectangle(0, 0, screenWidth, 5, GRAY);
    DrawRectangle(0, 0, 5, screenHeight, GRAY);
    DrawRectangle(screenWidth-5, 0, 5, screenHeight, GRAY);
    
    DrawText(TextFormat("Score: %d", score), 15, 12, 20, WHITE);
    DrawText(TextFormat("Lives: %d", lives), screenWidth - 110, 12, 20, lives > 1 ? GREEN : RED);
    
    float multiplier = CalculateMultiplier();
    DrawText(TextFormat("Time: %.1f", gameTime), 15, 38, 16, Fade(WHITE, 0.7f));
    DrawText(TextFormat("x%.1f", multiplier), 120, 38, 16, multiplier > 1.5f ? GREEN : YELLOW);
    
    if (!ball.IsLaunched() && currentState == GameState::PLAYING) {
        DrawText("Press SPACE to launch!", screenWidth/2 - 110, screenHeight - 60, 15, YELLOW);
    }
    DrawText("L/R arrows | Shift+Arrow=BOOST | P=Pause | R=Restart | L=Leaderboard", 
             screenWidth/2 - 350, screenHeight - 30, 13, GRAY);
}

void Game::DrawMenu() {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.9f));
    
    DrawText("BREAKOUT", screenWidth/2 - 120, screenHeight/2 - 100, 50, YELLOW);
    DrawText("Brick Breaker Game", screenWidth/2 - 100, screenHeight/2 - 40, 25, WHITE);
    
    DrawText("Press ENTER to Start", screenWidth/2 - 110, screenHeight/2 + 20, 20, GREEN);
    DrawText("Press L for Leaderboard", screenWidth/2 - 110, screenHeight/2 + 60, 20, SKYBLUE);
    DrawText("Press ESC to Exit", screenWidth/2 - 90, screenHeight/2 + 100, 20, RED);
    
    DrawText("Controls:", screenWidth/2 - 60, screenHeight/2 + 160, 18, GRAY);
    DrawText("Left/Right Arrows - Move Paddle", screenWidth/2 - 150, screenHeight/2 + 190, 14, GRAY);
    DrawText("Shift + Arrow - Boost Speed", screenWidth/2 - 140, screenHeight/2 + 215, 14, GRAY);
    DrawText("Space - Launch Ball", screenWidth/2 - 100, screenHeight/2 + 240, 14, GRAY);
}

void Game::DrawLeaderboard() {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.85f));
    DrawText("=== LEADERBOARD ===", screenWidth/2 - 130, 40, 28, GOLD);
    
    DrawText("RANK", 150, 90, 20, WHITE);
    DrawText("NAME", 280, 90, 20, WHITE);
    DrawText("SCORE", 450, 90, 20, WHITE);
    DrawText("DATE", 600, 90, 20, WHITE);
    DrawLine(100, 115, 700, 115, GRAY);
    
    for (int i = 0; i < leaderboardCount && i < 10; i++) {
        int y = 130 + i * 35;
        Color rowColor = (i == 0) ? GOLD : (i == 1) ? ColorAlpha(GOLD, 0.7f) : (i == 2) ? ColorAlpha(ORANGE, 0.7f) : WHITE;
        
        DrawText(TextFormat("%d", i + 1), 150, y, 20, rowColor);
        DrawText(leaderboardEntries[i].name, 280, y, 20, rowColor);
        DrawText(TextFormat("%d", leaderboardEntries[i].score), 450, y, 20, rowColor);
        
        char dateStr[32];
        strftime(dateStr, sizeof(dateStr), "%m/%d", localtime(&leaderboardEntries[i].timestamp));
        DrawText(dateStr, 600, y, 18, Fade(rowColor, 0.7f));
    }
    
    if (leaderboardCount == 0) {
        DrawText("No records yet!", screenWidth/2 - 80, screenHeight/2, 20, GRAY);
    }
    
    DrawText("Press L to Return", screenWidth/2 - 80, screenHeight - 50, 18, WHITE);
}

void Game::DrawPaused() {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.7f));
    DrawText("PAUSED", screenWidth/2 - 60, screenHeight/2 - 20, 40, YELLOW);
    DrawText("Press P to Resume", screenWidth/2 - 100, screenHeight/2 + 30, 20, WHITE);
}

void Game::DrawGameOver() {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.7f));
    DrawText("GAME OVER!", screenWidth/2 - 100, screenHeight/2 - 50, 50, RED);
    DrawText(TextFormat("Final Score: %d", score), screenWidth/2 - 100, screenHeight/2 + 10, 30, YELLOW);
    
    if (playerRank > 0) {
        DrawText(TextFormat("Rank #%d on Leaderboard!", playerRank), screenWidth/2 - 130, screenHeight/2 + 50, 20, GOLD);
    }
    
    DrawText("Press ENTER to Return to Menu", screenWidth/2 - 150, screenHeight/2 + 120, 20, WHITE);
}

void Game::DrawVictory() {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.7f));
    DrawText("VICTORY!", screenWidth/2 - 80, screenHeight/2 - 50, 50, GREEN);
    DrawText(TextFormat("Final Score: %d", score), screenWidth/2 - 100, screenHeight/2 + 10, 30, YELLOW);
    
    if (playerRank > 0) {
        DrawText(TextFormat("Rank #%d on Leaderboard!", playerRank), screenWidth/2 - 130, screenHeight/2 + 50, 20, GOLD);
    }
    
    DrawText("Press ENTER to Return to Menu", screenWidth/2 - 150, screenHeight/2 + 120, 20, WHITE);
}

void Game::Update() {
    HandleInput();
    
    if (currentState == GameState::PLAYING) {
        UpdateGame();
    }
}

void Game::Draw() {
    BeginDrawing();
    ClearBackground(BLACK);
    
    switch (currentState) {
        case GameState::MENU:
            DrawMenu();
            break;
        case GameState::PLAYING:
            ball.Draw();
            paddle.Draw();
            for (auto& brick : bricks) brick.Draw();
            DrawUI();
            break;
        case GameState::PAUSED:
            ball.Draw();
            paddle.Draw();
            for (auto& brick : bricks) brick.Draw();
            DrawUI();
            DrawPaused();
            break;
        case GameState::LEADERBOARD:
            DrawLeaderboard();
            break;
        case GameState::GAMEOVER:
            DrawGameOver();
            break;
        case GameState::VICTORY:
            DrawVictory();
            break;
        default:
            break;
    }
    
    EndDrawing();
}

void Game::LoadLeaderboard() {
    FILE* f = fopen("scores.txt", "r");
    if (f) {
        leaderboardCount = 0;
        while (leaderboardCount < 10 && 
               fscanf(f, "%31s %d %ld", 
                      leaderboardEntries[leaderboardCount].name, 
                      &leaderboardEntries[leaderboardCount].score, 
                      &leaderboardEntries[leaderboardCount].timestamp) == 3) {
            leaderboardCount++;
        }
        fclose(f);
    }
}

void Game::SaveLeaderboard() {
    FILE* f = fopen("scores.txt", "w");
    if (f) {
        for (int i = 0; i < leaderboardCount; i++) {
            fprintf(f, "%s %d %ld\n", 
                    leaderboardEntries[i].name, 
                    leaderboardEntries[i].score, 
                    leaderboardEntries[i].timestamp);
        }
        fclose(f);
    }
}

bool Game::CanEnterLeaderboard(int score) {
    return leaderboardCount < 10 || score > leaderboardEntries[leaderboardCount - 1].score;
}

int Game::AddToLeaderboard(const char* name, int score) {
    if (!CanEnterLeaderboard(score)) return 0;
    
    ScoreEntry newEntry;
    strncpy(newEntry.name, name, 31);
    newEntry.name[31] = '\0';
    newEntry.score = score;
    newEntry.timestamp = time(nullptr);
    
    int pos = 0;
    while (pos < leaderboardCount && leaderboardEntries[pos].score >= score) pos++;
    
    if (leaderboardCount < 10) leaderboardCount++;
    
    for (int i = leaderboardCount - 1; i > pos; i--) {
        leaderboardEntries[i] = leaderboardEntries[i - 1];
    }
    
    leaderboardEntries[pos] = newEntry;
    SaveLeaderboard();
    
    return pos + 1;
}

void Game::Shutdown() {
    SaveLeaderboard();
    TraceLog(LOG_INFO, "Game shutdown");
}