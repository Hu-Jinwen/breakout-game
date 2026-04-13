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
    , powerUpDropRate(0.35f)
    , ballSpeedMultiplier(1.0f)
    , isSlowed(false)
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
    TraceLog(LOG_INFO, "Loading config from: %s", path.c_str());
    
    // 实际项目中可以用 nlohmann/json 库读取
    // 这里手动设置配置值
    TraceLog(LOG_INFO, "Configuration loaded:");
    TraceLog(LOG_INFO, "  Ball radius: %.1f", ballRadius);
    TraceLog(LOG_INFO, "  Gravity: %.2f", gravity);
    TraceLog(LOG_INFO, "  Paddle speed: %.1f", paddleSpeed);
    TraceLog(LOG_INFO, "  Paddle boost speed: %.1f", paddleBoostSpeed);
    TraceLog(LOG_INFO, "  Initial lives: %d", initialLives);
    TraceLog(LOG_INFO, "  Score per brick: %d", scorePerBrick);
    TraceLog(LOG_INFO, "  PowerUp drop rate: %.2f", powerUpDropRate);
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
    
    // 砖块碰撞检测（生成道具和粒子）
    for (auto& brick : bricks) {
        if (brick.IsActive()) {
            if (ball.CheckBrickCollision(brick.GetRect())) {
                brick.SetActive(false);
                int addScore = (int)(scorePerBrick * CalculateMultiplier());
                score += addScore;
                
                // 生成粒子特效
                SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                
                // 随机生成道具
                float randomValue = (rand() % 100) / 100.0f;
                if (randomValue < powerUpDropRate) {
                    // 根据权重选择道具类型
                    int r = rand() % 100;
                    PowerUpType type;
                    if (r < 35) type = PowerUpType::PADDLE_EXTEND;
                    else if (r < 65) type = PowerUpType::MULTI_BALL;
                    else type = PowerUpType::SLOW_BALL;
                    
                    AddPowerUp(brick.GetRect().x + brickWidth/2, 
                              brick.GetRect().y + brickHeight/2, type);
                }
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
    ballSpeedMultiplier = 1.0f;
    isSlowed = false;
    
    // 清除道具和粒子
    powerUps.clear();
    activeEffects.clear();
    extraBalls.clear();
    particles.clear();
    
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

// ========== 道具系统实现 ==========

void Game::AddPowerUp(float x, float y, PowerUpType type) {
    powerUps.emplace_back(x, y, type);
    
    // 生成掉落光效
    Color glowColor;
    switch (type) {
        case PowerUpType::PADDLE_EXTEND: glowColor = GREEN; break;
        case PowerUpType::MULTI_BALL: glowColor = ORANGE; break;
        case PowerUpType::SLOW_BALL: glowColor = SKYBLUE; break;
        default: glowColor = WHITE;
    }
    SpawnPowerUpGlow(x, y, glowColor);
    
    TraceLog(LOG_INFO, "PowerUp spawned at (%.0f, %.0f)", x, y);
}

void Game::ApplyPowerUpEffect(PowerUpType type) {
    switch (type) {
        case PowerUpType::PADDLE_EXTEND:
            activeEffects.push_back(std::make_unique<ExtendPaddleEffect>(40.0f, 5.0f));
            break;
        case PowerUpType::MULTI_BALL:
            activeEffects.push_back(std::make_unique<MultiBallEffect>(2));
            break;
        case PowerUpType::SLOW_BALL:
            activeEffects.push_back(std::make_unique<SlowBallEffect>(0.6f, 4.0f));
            break;
        case PowerUpType::EXTRA_LIFE:
            lives++;
            TraceLog(LOG_INFO, "Extra life gained! Lives: %d", lives);
            break;
    }
    
    // 立即应用效果
    activeEffects.back()->Apply(*this);
}

void Game::CheckPowerUpCollisions() {
    for (auto& powerUp : powerUps) {
        if (!powerUp.IsActive()) continue;
        
        if (CheckCollisionRecs(powerUp.GetRect(), paddle.GetRect())) {
            ApplyPowerUpEffect(powerUp.GetType());
            powerUp.SetActive(false);
            SpawnPowerUpGlow(powerUp.GetRect().x, powerUp.GetRect().y, GOLD);
        }
    }
    
    // 移除无效道具
    powerUps.erase(
        std::remove_if(powerUps.begin(), powerUps.end(),
            [this](const PowerUp& p) { 
                return !p.IsActive() || p.IsOffScreen(screenHeight); 
            }),
        powerUps.end()
    );
}

void Game::UpdateEffects(float dt) {
    for (auto& effect : activeEffects) {
        effect->Update(*this, dt);
    }
    
    activeEffects.erase(
        std::remove_if(activeEffects.begin(), activeEffects.end(),
            [](const std::unique_ptr<PowerUpEffect>& e) { return e->IsExpired(); }),
        activeEffects.end()
    );
}

void Game::AddExtraBalls(int count) {
    Vector2 mainPos = ball.GetPosition();
    Vector2 mainSpeed = ball.GetSpeed();
    
    for (int i = 0; i < count; i++) {
        float angleOffset = (i + 1) * 45.0f;
        float rad = angleOffset * 3.14159f / 180.0f;
        
        Ball newBall(mainPos, {0, 0}, ballRadius);
        newBall.SetLaunched(true);
        
        float speedMagnitude = sqrt(mainSpeed.x * mainSpeed.x + mainSpeed.y * mainSpeed.y);
        if (speedMagnitude < 1.0f) speedMagnitude = 6.5f;
        
        // 使用 cosf 和 sinf 避免 narrowing conversion 警告
        float newSpeedX = mainSpeed.x * cosf(rad) - mainSpeed.y * sinf(rad);
        float newSpeedY = mainSpeed.x * sinf(rad) + mainSpeed.y * cosf(rad);
        
        newBall.SetSpeed({ newSpeedX, newSpeedY });
        
        extraBalls.push_back(newBall);
    }
}

void Game::SlowDownBalls(float factor) {
    ballSpeedMultiplier = factor;
    isSlowed = true;
    
    // 减速主球
    Vector2 speed = ball.GetSpeed();
    ball.SetSpeed({ speed.x * factor, speed.y * factor });
    
    // 减速额外球
    for (auto& b : extraBalls) {
        Vector2 s = b.GetSpeed();
        b.SetSpeed({ s.x * factor, s.y * factor });
    }
}

void Game::RestoreBallSpeed() {
    if (!isSlowed) return;
    
    // 恢复主球速度
    Vector2 speed = ball.GetSpeed();
    ball.SetSpeed({ speed.x / ballSpeedMultiplier, speed.y / ballSpeedMultiplier });
    
    // 恢复额外球速度
    for (auto& b : extraBalls) {
        Vector2 s = b.GetSpeed();
        b.SetSpeed({ s.x / ballSpeedMultiplier, s.y / ballSpeedMultiplier });
    }
    
    ballSpeedMultiplier = 1.0f;
    isSlowed = false;
}

void Game::UpdateExtraBalls(float dt) {
    for (auto& b : extraBalls) {
        b.Move();
        b.ApplyGravity();
        
        // 碰撞检测
        b.BounceEdge(screenWidth, screenHeight);
        
        if (CheckCollisionCircleRec(b.GetPosition(), b.GetRadius(), paddle.GetRect())) {
            if (b.GetSpeed().y > 0) {
                b.BouncePaddle(paddle.GetRect());
            }
        }
        
        // 砖块碰撞
        for (auto& brick : bricks) {
            if (brick.IsActive() && b.CheckBrickCollision(brick.GetRect())) {
                brick.SetActive(false);
                score += (int)(scorePerBrick * CalculateMultiplier());
                SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                
                // 道具生成
                if ((rand() % 100) / 100.0f < powerUpDropRate) {
                    int r = rand() % 100;
                    PowerUpType type;
                    if (r < 35) type = PowerUpType::PADDLE_EXTEND;
                    else if (r < 65) type = PowerUpType::MULTI_BALL;
                    else type = PowerUpType::SLOW_BALL;
                    AddPowerUp(brick.GetRect().x + brickWidth/2, 
                              brick.GetRect().y + brickHeight/2, type);
                }
                break;
            }
        }
        
        // 检查球是否出界
        if (b.GetPosition().y + b.GetRadius() >= screenHeight) {
            b.SetLaunched(false);
        }
    }
    
    // 移除失效的球
    extraBalls.erase(
        std::remove_if(extraBalls.begin(), extraBalls.end(),
            [](const Ball& b) { return !b.IsLaunched(); }),
        extraBalls.end()
    );
}

// ========== 粒子系统实现 ==========

void Game::SpawnBrickParticles(Rectangle brickRect, Color brickColor) {
    for (int i = 0; i < 12; i++) {
        Particle p;
        p.position = { 
            brickRect.x + (rand() % (int)brickRect.width),
            brickRect.y + (rand() % (int)brickRect.height)
        };
        p.velocity = { 
            ((rand() % 100) - 50) / 5.0f,
            ((rand() % 100) - 80) / 5.0f
        };
        p.color = brickColor;
        p.life = 0.6f;
        p.maxLife = 0.6f;
        particles.push_back(p);
    }
}

void Game::SpawnPowerUpGlow(float x, float y, Color color) {
    for (int i = 0; i < 8; i++) {
        Particle p;
        p.position = { x, y };
        p.velocity = { 
            ((rand() % 100) - 50) / 10.0f,
            ((rand() % 100) - 50) / 10.0f
        };
        p.color = color;
        p.life = 0.3f;
        p.maxLife = 0.3f;
        particles.push_back(p);
    }
}

void Game::UpdateParticles(float dt) {
    for (auto& p : particles) {
        p.position.x += p.velocity.x * dt * 60;
        p.position.y += p.velocity.y * dt * 60;
        p.velocity.y += 200.0f * dt; // 重力
        p.life -= dt;
    }
    
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) { return p.life <= 0; }),
        particles.end()
    );
}

void Game::DrawParticles() {
    for (const auto& p : particles) {
        float alpha = p.life / p.maxLife;
        DrawCircleV(p.position, 3, ColorAlpha(p.color, alpha));
    }
}

void Game::DrawExtraBalls() {
    for (auto& b : extraBalls) {
        b.Draw();
    }
}

// ========== UI 绘制 ==========

void Game::DrawUI() {
    DrawRectangle(0, 0, screenWidth, 5, GRAY);
    DrawRectangle(0, 0, 5, screenHeight, GRAY);
    DrawRectangle(screenWidth-5, 0, 5, screenHeight, GRAY);
    
    DrawText(TextFormat("Score: %d", score), 15, 12, 20, WHITE);
    DrawText(TextFormat("Lives: %d", lives), screenWidth - 110, 12, 20, lives > 1 ? GREEN : RED);
    
    float multiplier = CalculateMultiplier();
    DrawText(TextFormat("Time: %.1f", gameTime), 15, 38, 16, Fade(WHITE, 0.7f));
    DrawText(TextFormat("x%.1f", multiplier), 120, 38, 16, multiplier > 1.5f ? GREEN : YELLOW);
    
    // 显示活跃的道具效果
    int yOffset = 60;
    if (paddle.IsExtended()) {
        DrawText(TextFormat("POWER: Extended (%.1f)", paddle.GetEffectRemaining()), 
                 15, yOffset, 14, GREEN);
        yOffset += 20;
    }
    if (isSlowed) {
        DrawText("POWER: Slow Ball", 15, yOffset, 14, SKYBLUE);
        yOffset += 20;
    }
    if (extraBalls.size() > 0) {
        DrawText(TextFormat("POWER: Multi Ball (%d)", extraBalls.size() + 1), 
                 15, yOffset, 14, ORANGE);
    }
    
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
    
    // 道具说明
    DrawText("PowerUps:", screenWidth/2 - 50, screenHeight/2 + 290, 16, GOLD);
    DrawText("↔ Green - Extend Paddle", screenWidth/2 - 120, screenHeight/2 + 315, 12, GREEN);
    DrawText("● Orange - Multi Ball", screenWidth/2 - 120, screenHeight/2 + 335, 12, ORANGE);
    DrawText("🐌 Blue - Slow Ball", screenWidth/2 - 120, screenHeight/2 + 355, 12, SKYBLUE);
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

// ========== 主循环 ==========

void Game::Update() {
    HandleInput();
    paddle.Update(GetFrameTime());
    
    if (currentState == GameState::PLAYING) {
        UpdateGame();
        UpdateEffects(GetFrameTime());
        UpdateParticles(GetFrameTime());
        UpdateExtraBalls(GetFrameTime());
        
        for (auto& powerUp : powerUps) {
            powerUp.Update(GetFrameTime());
        }
        
        CheckPowerUpCollisions();
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
            DrawExtraBalls();
            paddle.Draw();
            for (auto& brick : bricks) brick.Draw();
            for (auto& powerUp : powerUps) powerUp.Draw();
            DrawParticles();
            DrawUI();
            break;
        case GameState::PAUSED:
            ball.Draw();
            DrawExtraBalls();
            paddle.Draw();
            for (auto& brick : bricks) brick.Draw();
            for (auto& powerUp : powerUps) powerUp.Draw();
            DrawParticles();
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

// ========== 排行榜 ==========

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