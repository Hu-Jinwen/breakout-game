// Game.cpp
#include "Game.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <cmath>
#include <unordered_set> 
#include <random>

static constexpr float SPLIT_ANGLE_OFFSET = 35.0f;
static constexpr float SPLIT_SPEED_BOOST = 1.15f;
static constexpr float MERGE_SPEED_FACTOR = 0.8f;
static constexpr float PORTAL_COOLDOWN_DURATION = 0.3f;

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
    , netHost(nullptr)
    , netPeer(nullptr)
    , isHost(false)
    , isConnected(false)
    , lastSendTime(0)
    , lastRecvTime(0)
    , netCurrentState{}
    , netTargetState{}
    , lastStateTime(0)
    , nextStateTime(0)
    , opponentPaddleX(0)
    , opponentScore(0)
    , asyncLoader(nullptr)
    , showLoadedTexture(false)
    , textureDisplayTimer(0.0f)
    , isLoadingRequested(false)
    , networkSendTimer(0)
    , networkReceiveTimeout(0)
    , interpolatedBallX(0)
    , interpolatedBallY(0)
    , interpolationAlpha(0)
    , currentLevel(1)
    , selectedLevel(1)
    , activeParticleCount(0)
    , useSpatialPartition(true)
    , lastFrameTime(0)
    , collisionTimeMs(0)
    , particleTimeMs(0)
    , totalFrameTimeMs(0)
    , portalPairs()
    , levelCompleted(false)
    , showVictoryMenu(false)
    , isFrenzyMode(false)
    , optimizedParticlePool(500)  
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
    loadedDemoTexture = Texture2D{0, 0, 0, 0, 0};
    
    for (int i = 0; i < MAX_PARTICLES; i++) {
        pooledParticles[i].active = false;
    }

    optimizedParticlePool.SetGravity(200.0f);   // 重力加速度（像素/秒²）
    optimizedParticlePool.SetDrag(0.98f);       // 空气阻力（每帧速度衰减）
    
    InitLevels();
}

Game::~Game() {
}

void Game::LoadConfig(const std::string& path) {
    TraceLog(LOG_INFO, "Loading config from: %s", path.c_str());
    TraceLog(LOG_INFO, "Configuration loaded");
}

void Game::Init() {
    LoadConfig("config.json");
    asyncLoader = new AsyncResourceLoader(textureCache);
    ball = Ball({(float)screenWidth/2, (float)screenHeight/2}, {0, 0}, ballRadius);
    paddle = Paddle(screenWidth/2 - paddleWidth/2, screenHeight - 50, paddleWidth, paddleHeight);
    LoadLeaderboard();
    loadedDemoTexture = Texture2D{0, 0, 0, 0, 0};
    srand((unsigned int)time(nullptr));
    CheckForSaveFile();
    TraceLog(LOG_INFO, "Game initialized");
}

void Game::InitNetwork(bool asHost, const char* serverIP) {
    static bool enetInitialized = false;
    if (!enetInitialized) {
        if (enet_initialize() != 0) {
            TraceLog(LOG_ERROR, "ENet initialization failed!");
            return;
        }
        enetInitialized = true;
        TraceLog(LOG_INFO, "ENet initialized");
    }
    
    isHost = asHost;
    
    if (asHost) {
        ENetAddress address;
        enet_address_set_host(&address, "0.0.0.0");
        address.port = 12345;
        netHost = enet_host_create(&address, 1, 2, 0, 0);
        if (!netHost) {
            TraceLog(LOG_ERROR, "Failed to create ENet host!");
            return;
        }
        TraceLog(LOG_INFO, "Server started on port 12345");
        isConnected = false;
    } else {
        netHost = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!netHost) {
            TraceLog(LOG_ERROR, "Failed to create ENet client!");
            return;
        }
        ENetAddress address;
        enet_address_set_host(&address, serverIP);
        address.port = 12345;
        netPeer = enet_host_connect(netHost, &address, 2, 0);
        if (!netPeer) {
            TraceLog(LOG_ERROR, "Failed to connect to server!");
            return;
        }
        TraceLog(LOG_INFO, "Connecting to server %s:12345...", serverIP);
        isConnected = false;
    }
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

void Game::InitLevels() {
    levels[0] = {1, "Forest Valley", "Easy", 0.8f, 1.0f, 5, 8, 1, 0.25f, 4, {}, 0};
    levels[1] = {2, "Pyramid Peak", "Normal", 1.0f, 1.0f, 7, 10, 2, 0.35f, 3, {}, 1};
    levels[2] = {3, "Dark Castle", "Hard", 1.25f, 1.2f, 9, 12, 3, 0.45f, 2, {}, 4};
}

void Game::InitBricksByLayout(int layoutType) {
    bricks.clear();
    
    switch (layoutType) {
        case 0:
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
            break;
        case 1:
            for (int row = 0; row < brickRows; row++) {
                Color rowColor = brickColors[row % 8];
                int bricksInRow = brickCols - row;
                int offsetX = (brickCols - bricksInRow) * (brickWidth + spacing) / 2;
                for (int col = 0; col < bricksInRow; col++) {
                    bricks.emplace_back(
                        startX + offsetX + col * (brickWidth + spacing),
                        startY + row * (brickHeight + spacing),
                        brickWidth, brickHeight,
                        rowColor
                    );
                }
            }
            break;
        case 2:
            for (int row = 0; row < brickRows; row++) {
                Color rowColor = brickColors[row % 8];
                int bricksInRow;
                int offsetX;
                int halfRows = brickRows / 2;
                if (row <= halfRows) {
                    bricksInRow = 3 + row * 2;
                } else {
                    bricksInRow = 3 + (brickRows - row - 1) * 2;
                }
                if (bricksInRow > brickCols) bricksInRow = brickCols;
                offsetX = (brickCols - bricksInRow) * (brickWidth + spacing) / 2;
                for (int col = 0; col < bricksInRow; col++) {
                    bricks.emplace_back(
                        startX + offsetX + col * (brickWidth + spacing),
                        startY + row * (brickHeight + spacing),
                        brickWidth, brickHeight,
                        rowColor
                    );
                }
            }
            break;
        default:
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
            break;
    }
}

void Game::BuildPortalPairs() {
    portalPairs.clear();
}

void Game::SplitBall(Ball& ball, const Brick& splitBrick) {
    if (ball.GetSplitCount() >= MAX_SPLIT_COUNT) return;
    
    Rectangle brickRect = splitBrick.GetRect();
    Vector2 splitPos = {
        brickRect.x + brickRect.width / 2,
        brickRect.y + brickRect.height / 2
    };
    
    Vector2 originalSpeed = ball.GetSpeed();
    float speedMagnitude = std::sqrt(originalSpeed.x * originalSpeed.x + originalSpeed.y * originalSpeed.y);
    
    float angle = std::atan2(originalSpeed.y, originalSpeed.x);
    float angleRad1 = angle + (SPLIT_ANGLE_OFFSET * 3.14159f / 180.0f);
    float angleRad2 = angle - (SPLIT_ANGLE_OFFSET * 3.14159f / 180.0f);
    
    Ball newBall1(splitPos, {0, 0}, ball.GetRadius());
    newBall1.SetLaunched(true);
    newBall1.SetMainBall(false);
    newBall1.SetHeavyBall(false);
    newBall1.IncrementSplitCount();
    
    float newSpeed1 = speedMagnitude * SPLIT_SPEED_BOOST;
    newBall1.SetSpeed({
        newSpeed1 * std::cos(angleRad1),
        newSpeed1 * std::sin(angleRad1)
    });
    
    Ball newBall2(splitPos, {0, 0}, ball.GetRadius());
    newBall2.SetLaunched(true);
    newBall2.SetMainBall(false);
    newBall2.SetHeavyBall(false);
    newBall2.IncrementSplitCount();
    
    float newSpeed2 = speedMagnitude * SPLIT_SPEED_BOOST;
    newBall2.SetSpeed({
        newSpeed2 * std::cos(angleRad2),
        newSpeed2 * std::sin(angleRad2)
    });
    
    extraBalls.push_back(newBall1);
    extraBalls.push_back(newBall2);
    ball.SetLaunched(false);
    
    for (int i = 0; i < 20; i++) {
        Vector2 vel = {
            ((rand() % 100) - 50) / 3.0f,
            ((rand() % 100) - 50) / 3.0f
        };
        SpawnParticlePooled(splitPos, vel, ORANGE, 0.5f);
    }
}

void Game::SplitBallIntoTwo(Ball& ball, Vector2 splitPosition) {
    if (ball.GetSplitCount() >= MAX_SPLIT_COUNT) return;
    
    Vector2 originalSpeed = ball.GetSpeed();
    float speedMagnitude = std::sqrt(originalSpeed.x * originalSpeed.x + originalSpeed.y * originalSpeed.y);
    
    float angle = std::atan2(originalSpeed.y, originalSpeed.x);
    float angleRad1 = angle + (SPLIT_ANGLE_OFFSET * 3.14159f / 180.0f);
    float angleRad2 = angle - (SPLIT_ANGLE_OFFSET * 3.14159f / 180.0f);
    
    Ball newBall1(splitPosition, {0, 0}, ball.GetRadius());
    newBall1.SetLaunched(true);
    newBall1.SetMainBall(false);
    newBall1.SetHeavyBall(false);
    newBall1.IncrementSplitCount();
    
    float newSpeed1 = speedMagnitude * SPLIT_SPEED_BOOST;
    newBall1.SetSpeed({
        newSpeed1 * std::cos(angleRad1),
        newSpeed1 * std::sin(angleRad1)
    });
    
    Ball newBall2(splitPosition, {0, 0}, ball.GetRadius());
    newBall2.SetLaunched(true);
    newBall2.SetMainBall(false);
    newBall2.SetHeavyBall(false);
    newBall2.IncrementSplitCount();
    
    float newSpeed2 = speedMagnitude * SPLIT_SPEED_BOOST;
    newBall2.SetSpeed({
        newSpeed2 * std::cos(angleRad2),
        newSpeed2 * std::sin(angleRad2)
    });
    
    extraBalls.push_back(newBall1);
    extraBalls.push_back(newBall2);
    ball.SetLaunched(false);
}

Vector2 Game::GetPairedPortalPosition(int portalId) {
    auto it = portalPairs.find(portalId);
    if (it != portalPairs.end()) {
        return it->second.second;
    }
    return {-100, -100};
}

void Game::HandlePortalTeleport(Ball& ball, int portalId) {
    float now = GetTime();
    auto cooldownIt = portalCooldowns.find(portalId);
    if (cooldownIt != portalCooldowns.end()) {
        if (now - cooldownIt->second < PORTAL_COOLDOWN_DURATION) {
            return;
        }
    }
    
    Vector2 targetPos = GetPairedPortalPosition(portalId);
    if (targetPos.x < 0) return;
    
    ball.SetPosition(targetPos);
    portalCooldowns[portalId] = now;
    
    for (int i = 0; i < 15; i++) {
        Vector2 vel = {
            ((rand() % 100) - 50) / 5.0f,
            ((rand() % 100) - 50) / 5.0f
        };
        SpawnParticlePooled(targetPos, vel, PURPLE, 0.5f);
    }
}

std::vector<Ball*> Game::GetAllActiveBalls() {
    std::vector<Ball*> activeBalls;
    if (ball.IsLaunched()) activeBalls.push_back(&ball);
    for (auto& b : extraBalls) {
        if (b.IsLaunched()) activeBalls.push_back(&b);
    }
    return activeBalls;
}

void Game::CheckBallMerge() {
    std::vector<Ball*> activeBalls = GetAllActiveBalls();
    
    for (size_t i = 0; i < activeBalls.size(); i++) {
        Ball* ballA = activeBalls[i];
        if (!ballA->IsLaunched()) continue;
        if (ballA->IsHeavyBall()) continue;
        
        for (size_t j = i + 1; j < activeBalls.size(); j++) {
            Ball* ballB = activeBalls[j];
            if (!ballB->IsLaunched()) continue;
            if (ballB->IsHeavyBall()) continue;
            
            if (ballA->CheckBallCollision(*ballB)) {
                Vector2 posA = ballA->GetPosition();
                Vector2 posB = ballB->GetPosition();
                Vector2 mergePos = {
                    (posA.x + posB.x) / 2,
                    (posA.y + posB.y) / 2
                };
                
                Vector2 speedA = ballA->GetSpeed();
                Vector2 speedB = ballB->GetSpeed();
                Vector2 mergedSpeed = {
                    (speedA.x + speedB.x) * MERGE_SPEED_FACTOR,
                    (speedA.y + speedB.y) * MERGE_SPEED_FACTOR
                };
                
                float mergedMag = std::sqrt(mergedSpeed.x * mergedSpeed.x + mergedSpeed.y * mergedSpeed.y);
                if (mergedMag > maxSpeed) {
                    mergedSpeed.x = (mergedSpeed.x / mergedMag) * maxSpeed;
                    mergedSpeed.y = (mergedSpeed.y / mergedMag) * maxSpeed;
                }
                
                if (mergedMag < 4.0f) {
                    mergedSpeed.x = (mergedSpeed.x > 0 ? 4.0f : -4.0f);
                    mergedSpeed.y = -6.0f;
                }
                
                ballA->SetHeavyBall(true);
                ballA->SetPosition(mergePos);
                ballA->SetSpeed(mergedSpeed);
                ballB->SetLaunched(false);
                
                for (int k = 0; k < 25; k++) {
                    Vector2 vel = {
                        ((rand() % 100) - 50) / 4.0f,
                        ((rand() % 100) - 50) / 4.0f
                    };
                    SpawnParticlePooled(mergePos, vel, GOLD, 0.6f);
                }
                return;
            }
        }
    }
}

bool Game::HandleHeavyBallCollision(Ball& ball, std::vector<int>& hitBrickIndices) {
    if (!ball.IsLaunched()) return false;
    if (!ball.IsHeavyBall()) return false;
    
    bool hitAny = false;
    
    for (int idx : hitBrickIndices) {
        if (idx < 0 || idx >= (int)bricks.size()) continue;
        
        Brick& brick = bricks[idx];
        if (!brick.IsActive()) continue;
        
        if (ball.HeavyBallBounceFromRect(brick.GetRect())) {
            brick.SetActive(false);
            hitAny = true;
            int addScore = (int)(scorePerBrick * CalculateMultiplier());
            score += addScore;
            SpawnBrickParticles(brick.GetRect(), brick.GetColor());
            
            if (brick.IsSplit()) {
                HandleSplitBrickHit(brick, ball);
            }
            
            float randomValue = (rand() % 100) / 100.0f;
            if (randomValue < powerUpDropRate) {
                int r = rand() % 100;
                PowerUpType type;
                if (r < 35) type = PowerUpType::PADDLE_EXTEND;
                else if (r < 65) type = PowerUpType::MULTI_BALL;
                else type = PowerUpType::SLOW_BALL;
                AddPowerUp(brick.GetRect().x + brickWidth/2, brick.GetRect().y + brickHeight/2, type);
            }
        }
    }
    return hitAny;
}

void Game::HandleSplitBrickHit(Brick& brick, Ball& hittingBall) {
    if (hittingBall.GetSplitCount() >= MAX_SPLIT_COUNT) return;
    
    SplitBall(hittingBall, brick);
    
    int bonusScore = (int)(50 * CalculateMultiplier());
    score += bonusScore;
    
    Rectangle brickRect = brick.GetRect();
    Vector2 brickCenter = {
        brickRect.x + brickRect.width / 2,
        brickRect.y + brickRect.height / 2
    };
    
    for (int i = 0; i < 30; i++) {
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        float speed = (rand() % 200) / 10.0f;
        Vector2 vel = {
            cosf(angle) * speed,
            sinf(angle) * speed
        };
        SpawnParticlePooled(brickCenter, vel, ORANGE, 0.7f);
    }
}

bool Game::SaveGame(const std::string& filename) {
    TraceLog(LOG_INFO, "SaveGame called: %s", filename.c_str());
    return true;
}

bool Game::LoadGame(const std::string& filename) {
    TraceLog(LOG_INFO, "LoadGame called: %s", filename.c_str());
    return false;
}

void Game::LoadLevel(int level) {
    if (level < 1 || level > 3) return;
    
    currentLevel = level;
    const LevelConfig& cfg = levels[level - 1];
    
    initialLives = cfg.maxLives;
    lives = initialLives;
    score = 0;
    gameTime = 0;
    
    brickRows = cfg.brickRows;
    brickCols = cfg.brickCols;
    
    float totalWidth = brickCols * brickWidth + (brickCols - 1) * spacing;
    if (totalWidth > screenWidth - 100) {
        brickWidth = (screenWidth - 100 - (brickCols - 1) * spacing) / brickCols;
    }
    startX = (screenWidth - brickCols * brickWidth - (brickCols - 1) * spacing) / 2;
    
    ballSpeedMultiplier = cfg.ballSpeedMultiplier;
    paddleSpeed = 9.0f * cfg.paddleSpeedMultiplier;
    paddleBoostSpeed = 15.0f * cfg.paddleSpeedMultiplier;
    scorePerBrick = 10 * cfg.scoreMultiplier;
    powerUpDropRate = cfg.powerUpDropRate;
    
    InitBricksByLayout(cfg.layoutType);
    
    ball = Ball({(float)screenWidth/2, (float)screenHeight/2}, {0, 0}, ballRadius);
    ball.SetLaunched(false);
    ball.SetMainBall(true);
    ball.SetHeavyBall(false);
    ball.ResetSplitCount();
    
    paddle = Paddle(screenWidth/2 - paddleWidth/2, screenHeight - 50, paddleWidth, paddleHeight);
    
    powerUps.clear();
    activeEffects.clear();
    extraBalls.clear();
    portalCooldowns.clear();
    
    // ========== 清理原有粒子系统 ==========
    // 遍历原有粒子数组，将所有粒子标记为未激活
    for (int i = 0; i < MAX_PARTICLES; i++) {
        pooledParticles[i].active = 0;
    }
    activeParticleCount = 0;
    
    // ========== 清理优化版粒子池 ==========
    // 优化说明：Clear()方法会清空所有粒子并重建空闲索引栈
    // 时间复杂度：O(MAX_PARTICLES)，仅在关卡切换时执行，不影响游戏性能
    optimizedParticlePool.Clear();
    
    // 重新设置优化版粒子池的物理参数（确保配置与当前关卡匹配）
    // 重力加速度：200像素/秒²，模拟真实下落效果
    // 空气阻力：0.98，每帧速度衰减2%，使粒子逐渐减速
    optimizedParticlePool.SetGravity(200.0f);
    optimizedParticlePool.SetDrag(0.98f);
    
    // ========== 初始化脏标记空间划分（修复碰撞问题） ==========
    // 设置砖块数组指针（关键！没有这一步，碰撞检测将无法工作）
    dirtySpatialGrid.SetBrickList(&bricks);
    // 标记全部脏，等待重建
    dirtySpatialGrid.MarkDirty();
    // 立即重建一次（maxRebuildFrames=1强制重建）
    dirtySpatialGrid.RebuildIfNeeded(1);
    
    // ========== 原有空间划分网格清理（保留兼容） ==========
    for (int x = 0; x < GRID_COLS; x++) {
        for (int y = 0; y < GRID_ROWS; y++) {
            grid[x][y].brickIndices.clear();
        }
    }
    BuildSpatialGrid();
    BuildPortalPairs();
    
    for (auto& brick : bricks) {
        if (brick.IsMoving()) {
            brick.SetMoveParams(60.0f, 15.0f);
        }
    }

    levelCompleted = false;
    showVictoryMenu = false;
    
    TraceLog(LOG_INFO, "Loaded Level %d with optimized systems (Dirty Grid + Particle Pool)", level);
}

void Game::HandleInput() {
    if (IsKeyPressed(KEY_R)) {
        ResetGame();
        ChangeState(GameState::PLAYING);
        return;
    }
    
    float speed = 0.0f;
    
    switch (currentState) {
        case GameState::MENU:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (SaveExists()) {
                    if (LoadGame("savegame.json")) {
                        ChangeState(GameState::PLAYING);
                    } else {
                        ChangeState(GameState::LEVEL_SELECT);
                    }
                } else {
                    ChangeState(GameState::LEVEL_SELECT);
                }
            }
            if (IsKeyPressed(KEY_L)) {
                if (SaveExists()) {
                    if (LoadGame("savegame.json")) {
                        ChangeState(GameState::PLAYING);
                        TraceLog(LOG_INFO, "Loaded save game!");
                    }
                } else {
                    ChangeState(GameState::LEADERBOARD);
                    if (asyncLoader) {
                        asyncLoader->ForceRestart();
                        asyncLoader->StartLoadTexture("demo_texture.png");
                    }
                }
            }
            if (IsKeyPressed(KEY_F)) {
                isFrenzyMode = true;
                ChangeState(GameState::LEVEL_SELECT);
                TraceLog(LOG_INFO, "Frenzy Mode ENABLED!");
            }
            break;
        
        case GameState::LEVEL_SELECT:
            if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) {
                selectedLevel = 1;
                LoadLevel(selectedLevel);
                ChangeState(GameState::PLAYING);
            }
            if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) {
                selectedLevel = 2;
                LoadLevel(selectedLevel);
                ChangeState(GameState::PLAYING);
            }
            if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) {
                selectedLevel = 3;
                LoadLevel(selectedLevel);
                ChangeState(GameState::PLAYING);
            }
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
                ChangeState(GameState::MENU);
            }
            break;    

        case GameState::PLAYING:
            if (IsKeyPressed(KEY_P)) {
                ChangeState(GameState::PAUSED);
            }
            if (IsKeyPressed(KEY_L)) {
                ChangeState(GameState::LEADERBOARD);
            }
            if (IsKeyPressed(KEY_G)) {
                useSpatialPartition = !useSpatialPartition;
                TraceLog(LOG_INFO, "Spatial partition %s", useSpatialPartition ? "ENABLED" : "DISABLED");
            }
    
            speed = paddleSpeed;
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

            if (IsKeyPressed(KEY_F5)) {
                SaveGame("savegame.json");
                TraceLog(LOG_INFO, "Game manually saved!");
            }
            break;
            
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
            
        case GameState::VICTORY_MENU:
            if (IsKeyPressed(KEY_R)) {
                LoadLevel(currentLevel);
                levelCompleted = false;
                showVictoryMenu = false;
                ChangeState(GameState::PLAYING);
                TraceLog(LOG_INFO, "Restarting level %d", currentLevel);
            } else if (IsKeyPressed(KEY_N)) {
                if (currentLevel < 3) {
                    currentLevel++;
                    selectedLevel = currentLevel;
                    LoadLevel(currentLevel);
                    levelCompleted = false;
                    showVictoryMenu = false;
                    ChangeState(GameState::PLAYING);
                    TraceLog(LOG_INFO, "Loading next level: %d", currentLevel);
                } else {
                    ChangeState(GameState::VICTORY);
                }
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                levelCompleted = false;
                showVictoryMenu = false;
                ChangeState(GameState::LEVEL_SELECT);
                TraceLog(LOG_INFO, "Returning to level select");
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
        case GameState::LEVEL_SELECT:
            TraceLog(LOG_INFO, "Entering LEVEL_SELECT state");
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
            remove("savegame.json");
            break;
        case GameState::VICTORY:
            TraceLog(LOG_INFO, "Victory! Final score: %d", score);
            if (CanEnterLeaderboard(score)) {
                playerRank = AddToLeaderboard("Player", score);
            }
            remove("savegame.json");
            break;
        case GameState::VICTORY_MENU:
            TraceLog(LOG_INFO, "Entering VICTORY_MENU state for level %d", currentLevel);
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
    
    // 疯狂模式球速加成
    if (isFrenzyMode && ball.IsLaunched()) {
        Vector2 speed = ball.GetSpeed();
        float maxFrenzySpeed = 20.0f;
        if (fabs(speed.x) < maxFrenzySpeed && fabs(speed.y) < maxFrenzySpeed) {
            ball.SetSpeed({speed.x * 1.01f, speed.y * 1.01f});
        }
    }
    
    ball.Move();
    ball.ApplyGravity();
    CheckCollisions();      // 碰撞检测内部已使用脏标记空间划分
    CheckWinCondition();
    
    // ========== 注意：原有的 BuildSpatialGrid() 已移除 ==========
    // 空间划分的更新现在在 Update() 函数中通过 dirtySpatialGrid.RebuildIfNeeded() 处理
    // 这样可以将重建操作分散到多帧，避免单帧卡顿
}

void Game::UpdateNetwork() {
    if (!netHost) return;
}

void Game::CheckCollisions() {
    if (!ball.IsLaunched() && extraBalls.empty()) return;
    
    double startTime = GetTime();
    
    // ========== 主球边界碰撞 ==========
    if (ball.IsLaunched()) {
        ball.BounceEdge(screenWidth, screenHeight);
    }
    
    // ========== 主球掉落处理 ==========
    if (ball.IsLaunched() && ball.GetPosition().y + ball.GetRadius() >= screenHeight) {
        if (isFrenzyMode) {
            // 疯狂模式：球掉落时不扣命，直接重置位置
            ball.SetLaunched(false);
        } else {
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
    }
    
    // ========== 主球与挡板碰撞 ==========
    if (ball.IsLaunched() && CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), paddle.GetRect())) {
        if (ball.GetSpeed().y > 0) {
            ball.BouncePaddle(paddle.GetRect());
        }
    }
    
    // ========== 主球与砖块碰撞（使用脏标记空间划分优化） ==========
    if (ball.IsLaunched()) {
        std::vector<int> nearbyBrickIndices;
        
        if (useSpatialPartition) {
            // ========== 优化版：使用脏标记空间划分 ==========
            // 每帧调用RebuildIfNeeded，限制重建频率（每3帧重建一次）
            dirtySpatialGrid.RebuildIfNeeded(3);
            
            // 获取小球附近的砖块索引（只检测周围9个网格单元）
            dirtySpatialGrid.GetNearbyBricks(ball, nearbyBrickIndices);
            
            // 只检测附近的砖块
            for (int idx : nearbyBrickIndices) {
                if (idx >= 0 && idx < (int)bricks.size()) {
                    auto& brick = bricks[idx];
                    if (brick.IsActive() && ball.CheckBrickCollision(brick.GetRect())) {
                        brick.SetActive(false);
                        
                        // 标记受影响的单元格为脏（增量更新）
                        dirtySpatialGrid.MarkBrickDirty(idx, brick.GetRect());
                        
                        int addScore = (int)(scorePerBrick * CalculateMultiplier());
                        score += addScore;
                        
                        // 使用优化版粒子池生成砖块破碎粒子
                        SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                        
                        if (brick.IsSplit()) HandleSplitBrickHit(brick, ball);
                        if (brick.IsPortal()) HandlePortalTeleport(ball, brick.GetPortalId());
                        
                        // 疯狂模式：生成新砖块和额外小球
                        if (isFrenzyMode) {
                            float newX = GetRandomValue(50, screenWidth - (int)brickWidth - 50);
                            int newY = GetRandomValue(60, 200);
                            Brick newBrick(newX, newY, brickWidth, brickHeight, RED);
                            bricks.push_back(newBrick);
                            
                            // 标记新砖块影响的单元格为脏
                            dirtySpatialGrid.MarkBrickDirty((int)bricks.size() - 1, newBrick.GetRect());
                            
                            Vector2 brickCenter = { brick.GetRect().x + brickWidth / 2, 
                                                    brick.GetRect().y + brickHeight / 2 };
                            for (int i = 0; i < 2; i++) {
                                Ball newBall(brickCenter, {0, 0}, ballRadius);
                                newBall.SetLaunched(true);
                                newBall.SetMainBall(false);
                                float angle = (rand() % 360) * 3.14159f / 180.0f;
                                float speed = 9.0f;
                                newBall.SetSpeed({cosf(angle) * speed, sinf(angle) * speed});
                                extraBalls.push_back(newBall);
                            }
                        }
                        
                        // 道具掉落
                        float randomValue = (rand() % 100) / 100.0f;
                        if (randomValue < powerUpDropRate) {
                            int r = rand() % 100;
                            PowerUpType type;
                            if (r < 35) type = PowerUpType::PADDLE_EXTEND;
                            else if (r < 65) type = PowerUpType::MULTI_BALL;
                            else type = PowerUpType::SLOW_BALL;
                            AddPowerUp(brick.GetRect().x + brickWidth/2, brick.GetRect().y + brickHeight/2, type);
                        }
                        break;
                    }
                }
            }
        } else {
            // ========== 原有暴力检测模式（保留兼容） ==========
            // 遍历所有砖块进行碰撞检测
            for (auto& brick : bricks) {
                if (brick.IsActive() && ball.CheckBrickCollision(brick.GetRect())) {
                    brick.SetActive(false);
                    int addScore = (int)(scorePerBrick * CalculateMultiplier());
                    score += addScore;
                    SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                    
                    if (brick.IsSplit()) HandleSplitBrickHit(brick, ball);
                    if (brick.IsPortal()) HandlePortalTeleport(ball, brick.GetPortalId());
                    
                    if (isFrenzyMode) {
                        float newX = GetRandomValue(50, screenWidth - (int)brickWidth - 50);
                        int newY = GetRandomValue(60, 200);
                        Brick newBrick(newX, newY, brickWidth, brickHeight, RED);
                        bricks.push_back(newBrick);
                        
                        Vector2 brickCenter = { brick.GetRect().x + brickWidth / 2, 
                                                brick.GetRect().y + brickHeight / 2 };
                        for (int i = 0; i < 2; i++) {
                            Ball newBall(brickCenter, {0, 0}, ballRadius);
                            newBall.SetLaunched(true);
                            newBall.SetMainBall(false);
                            float angle = (rand() % 360) * 3.14159f / 180.0f;
                            float speed = 9.0f;
                            newBall.SetSpeed({cosf(angle) * speed, sinf(angle) * speed});
                            extraBalls.push_back(newBall);
                        }
                    }
                    
                    float randomValue = (rand() % 100) / 100.0f;
                    if (randomValue < powerUpDropRate) {
                        int r = rand() % 100;
                        PowerUpType type;
                        if (r < 35) type = PowerUpType::PADDLE_EXTEND;
                        else if (r < 65) type = PowerUpType::MULTI_BALL;
                        else type = PowerUpType::SLOW_BALL;
                        AddPowerUp(brick.GetRect().x + brickWidth/2, brick.GetRect().y + brickHeight/2, type);
                    }
                    break;
                }
            }
        }
    }
    
    // ========== 额外小球碰撞检测 ==========
    for (auto& b : extraBalls) {
        if (!b.IsLaunched()) continue;
        
        b.BounceEdge(screenWidth, screenHeight);
        
        // 额外小球与挡板碰撞
        if (CheckCollisionCircleRec(b.GetPosition(), b.GetRadius(), paddle.GetRect())) {
            if (b.GetSpeed().y > 0) {
                b.BouncePaddle(paddle.GetRect());
            }
        }
        
        // 额外小球掉落处理
        if (b.GetPosition().y + b.GetRadius() >= screenHeight) {
            b.SetLaunched(false);
            continue;
        }
        
        // 额外小球与砖块碰撞
        for (auto& brick : bricks) {
            if (brick.IsActive() && b.CheckBrickCollision(brick.GetRect())) {
                brick.SetActive(false);
                
                // 如果使用空间划分，标记受影响的单元格为脏
                if (useSpatialPartition) {
                    // 需要找到砖块索引，这里简化处理：直接标记全局脏
                    dirtySpatialGrid.MarkDirty();
                }
                
                score += (int)(scorePerBrick * CalculateMultiplier());
                SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                
                if (isFrenzyMode) {
                    Vector2 brickCenter = { brick.GetRect().x + brickWidth / 2, 
                                            brick.GetRect().y + brickHeight / 2 };
                    for (int i = 0; i < 2; i++) {
                        Ball newBall(brickCenter, {0, 0}, ballRadius);
                        newBall.SetLaunched(true);
                        newBall.SetMainBall(false);
                        float angle = (rand() % 360) * 3.14159f / 180.0f;
                        float speed = 9.0f;
                        newBall.SetSpeed({cosf(angle) * speed, sinf(angle) * speed});
                        extraBalls.push_back(newBall);
                    }
                }
                break;
            }
        }
    }
    
    // ========== 疯狂模式：检查是否有任何球活跃 ==========
    if (isFrenzyMode) {
        bool anyBallActive = ball.IsLaunched();
        for (auto& b : extraBalls) {
            if (b.IsLaunched()) anyBallActive = true;
        }
        
        if (!anyBallActive) {
            lives--;
            score = std::max(0, score - deathPenalty);
            
            if (lives <= 0) {
                ChangeState(GameState::GAMEOVER);
            } else {
                float paddleCenterX = paddle.GetRect().x + paddle.GetRect().width / 2;
                float paddleTopY = paddle.GetRect().y;
                ball.ResetToPaddle(paddleCenterX, paddleTopY);
                ball.SetLaunched(false);
                ball.SetMainBall(true);
                ball.SetSpeed({0, 0});
                extraBalls.clear();
            }
            return;
        }
    }
    
    // ========== 检测小球合并（重球生成） ==========
    CheckBallMerge();
    
    collisionTimeMs = (GetTime() - startTime) * 1000;
}

void Game::CheckWinCondition() {
    bool allBricksDestroyed = true;
    for (auto& brick : bricks) {
        if (brick.IsActive()) {
            allBricksDestroyed = false;
            break;
        }
    }
    
    if (allBricksDestroyed && !levelCompleted) {
        levelCompleted = true;
        showVictoryMenu = true;
        ChangeState(GameState::VICTORY_MENU);
        TraceLog(LOG_INFO, "Level %d completed!", currentLevel);
    }
}

void Game::ResetGame() {
    levelCompleted = false;
    showVictoryMenu = false;
    LoadLevel(selectedLevel);
    ChangeState(GameState::PLAYING);
    remove("savegame.json");
}

float Game::CalculateMultiplier() {
    float multiplier = 3.0f - gameTime * 0.03f;
    if (multiplier < 1.0f) multiplier = 1.0f;
    return multiplier;
}

void Game::AddPowerUp(float x, float y, PowerUpType type) {
    powerUps.emplace_back(x, y, type);
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
        default:
            break;
    }
    activeEffects.back()->Apply(*this);
}

void Game::CheckPowerUpCollisions() {
    for (auto& powerUp : powerUps) {
        if (!powerUp.IsActive()) continue;
        
        if (CheckCollisionRecs(powerUp.GetRect(), paddle.GetRect())) {
            ApplyPowerUpEffect(powerUp.GetType());
            powerUp.SetActive(false);
        }
    }
    
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
        
        float newSpeedX = mainSpeed.x * cosf(rad) - mainSpeed.y * sinf(rad);
        float newSpeedY = mainSpeed.x * sinf(rad) + mainSpeed.y * cosf(rad);
        
        newBall.SetSpeed({ newSpeedX, newSpeedY });
        
        extraBalls.push_back(newBall);
    }
}

void Game::SlowDownBalls(float factor) {
    ballSpeedMultiplier = factor;
    isSlowed = true;
    
    Vector2 speed = ball.GetSpeed();
    ball.SetSpeed({ speed.x * factor, speed.y * factor });
    
    for (auto& b : extraBalls) {
        Vector2 s = b.GetSpeed();
        b.SetSpeed({ s.x * factor, s.y * factor });
    }
}

void Game::RestoreBallSpeed() {
    if (!isSlowed) return;
    
    Vector2 speed = ball.GetSpeed();
    ball.SetSpeed({ speed.x / ballSpeedMultiplier, speed.y / ballSpeedMultiplier });
    
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
        
        b.BounceEdge(screenWidth, screenHeight);
        
        if (CheckCollisionCircleRec(b.GetPosition(), b.GetRadius(), paddle.GetRect())) {
            if (b.GetSpeed().y > 0) {
                b.BouncePaddle(paddle.GetRect());
            }
        }
        
        for (auto& brick : bricks) {
            if (brick.IsActive() && b.CheckBrickCollision(brick.GetRect())) {
                brick.SetActive(false);
                score += (int)(scorePerBrick * CalculateMultiplier());
                SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                break;
            }
        }
        
        if (b.GetPosition().y + b.GetRadius() >= screenHeight) {
            b.SetLaunched(false);
        }
    }
    
    extraBalls.erase(
        std::remove_if(extraBalls.begin(), extraBalls.end(),
            [](const Ball& b) { return !b.IsLaunched(); }),
        extraBalls.end()
    );
}

void Game::DrawExtraBalls() {
    for (auto& b : extraBalls) {
        b.Draw();
    }
}

void Game::DrawUI() {
    // ========== 绘制屏幕边框 ==========
    DrawRectangle(0, 0, screenWidth, 5, GRAY);
    DrawRectangle(0, 0, 5, screenHeight, GRAY);
    DrawRectangle(screenWidth-5, 0, 5, screenHeight, GRAY);
    
    // ========== 基础信息（分数、生命） ==========
    DrawText(TextFormat("Score: %d", score), 15, 12, 20, WHITE);
    DrawText(TextFormat("Lives: %d", lives), screenWidth - 110, 12, 20, lives > 1 ? GREEN : RED);
    
    // ========== 分数倍率（随时间递减） ==========
    float multiplier = CalculateMultiplier();
    DrawText(TextFormat("Time: %.1f", gameTime), 15, 38, 16, Fade(WHITE, 0.7f));
    DrawText(TextFormat("x%.1f", multiplier), 120, 38, 16, multiplier > 1.5f ? GREEN : YELLOW);
    
    // ========== 疯狂模式标识 ==========
    if (isFrenzyMode) {
        DrawText("FRENZY MODE ACTIVE!", screenWidth/2 - 100, 55, 18, ORANGE);
    }
    
    // ========== 道具效果状态 ==========
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
        DrawText(TextFormat("POWER: Multi Ball (%d)", (int)extraBalls.size() + 1), 
                 15, yOffset, 14, ORANGE);
    }
    
    // ========== 发射提示 ==========
    if (!ball.IsLaunched() && currentState == GameState::PLAYING) {
        DrawText("Press SPACE to launch!", screenWidth/2 - 110, screenHeight - 60, 15, YELLOW);
    }
    
    // ========== 操作说明 ==========
    DrawText("L/R arrows | Shift+Arrow=BOOST | P=Pause | R=Restart | L=Leaderboard | G=Grid", 
             screenWidth/2 - 380, screenHeight - 30, 13, GRAY);
    
    // ========== 性能统计显示（优化版） ==========
    float fps = 1.0f / GetFrameTime();
    
    // 碰撞检测耗时（毫秒）
    DrawText(TextFormat("Collision: %.2f ms", collisionTimeMs), 
             15, screenHeight - 95, 12, 
             collisionTimeMs > 2.0f ? RED : (collisionTimeMs > 1.0f ? YELLOW : GREEN));
    
    // 粒子统计（使用优化版粒子池的活跃数量）
    DrawText(TextFormat("Particles: %d/500", optimizedParticlePool.GetActiveCount()), 
             15, screenHeight - 80, 12, 
             optimizedParticlePool.GetActiveCount() > 400 ? RED : GRAY);
    
    // 空间划分状态（显示当前使用的是优化版还是暴力检测）
    if (useSpatialPartition) {
        // 显示脏标记网格的状态
        DrawText(useSpatialPartition ? "[OPT: Dirty Grid]" : "[OPT: Brute Force]", 
                 15, screenHeight - 65, 12, 
                 useSpatialPartition ? SKYBLUE : ORANGE);
        
        // 显示当前网格是否脏（是否需要重建）
        if (dirtySpatialGrid.IsDirty()) {
            DrawText("[Grid: Dirty - Rebuild pending]", 15, screenHeight - 50, 10, YELLOW);
        } else {
            DrawText("[Grid: Clean]", 15, screenHeight - 50, 10, GREEN);
        }
    } else {
        DrawText("[Mode: Brute Force - No optimization]", 15, screenHeight - 65, 12, ORANGE);
    }
    
    // FPS显示
    DrawText(TextFormat("FPS: %.0f", fps), 
             screenWidth - 90, screenHeight - 50, 14,
             fps >= 58 ? GREEN : (fps >= 45 ? YELLOW : RED));
    
    // 剩余砖块数量
    int activeBricks = 0;
    for (auto& brick : bricks) if (brick.IsActive()) activeBricks++;
    DrawText(TextFormat("Bricks: %d", activeBricks), 
             screenWidth - 90, screenHeight - 30, 12, GRAY);
}

void Game::DrawMenu() {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.9f));
    
    DrawText("BREAKOUT", screenWidth/2 - 120, screenHeight/2 - 100, 50, YELLOW);
    DrawText("Brick Breaker Game", screenWidth/2 - 100, screenHeight/2 - 40, 25, WHITE);
    
    DrawText("Press ENTER to Start", screenWidth/2 - 110, screenHeight/2 + 20, 20, GREEN);
    DrawText("Press L for Leaderboard", screenWidth/2 - 110, screenHeight/2 + 60, 20, SKYBLUE);
    DrawText("Press F for Frenzy Mode", screenWidth/2 - 120, screenHeight/2 + 100, 18, ORANGE);
    DrawText("Press ESC to Exit", screenWidth/2 - 90, screenHeight/2 + 140, 20, RED);
    
    DrawText("Controls:", screenWidth/2 - 60, screenHeight/2 + 190, 18, GRAY);
    DrawText("Left/Right Arrows - Move Paddle", screenWidth/2 - 150, screenHeight/2 + 220, 14, GRAY);
    DrawText("Shift + Arrow - Boost Speed", screenWidth/2 - 140, screenHeight/2 + 245, 14, GRAY);
    DrawText("Space - Launch Ball", screenWidth/2 - 100, screenHeight/2 + 270, 14, GRAY);
    
    DrawText("PowerUps:", screenWidth/2 - 50, screenHeight/2 + 320, 16, GOLD);
    DrawText("Green - Extend Paddle", screenWidth/2 - 120, screenHeight/2 + 345, 12, GREEN);
    DrawText("Orange - Multi Ball", screenWidth/2 - 120, screenHeight/2 + 365, 12, ORANGE);
    DrawText("Blue - Slow Ball", screenWidth/2 - 120, screenHeight/2 + 385, 12, SKYBLUE);
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

void Game::DrawVictoryMenu() {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.85f));
    DrawText("YOU WIN!", screenWidth/2 - 100, screenHeight/2 - 100, 50, GREEN);
    DrawText(TextFormat("Level %d Completed!", currentLevel), screenWidth/2 - 120, screenHeight/2 - 40, 25, YELLOW);
    DrawText(TextFormat("Score: %d", score), screenWidth/2 - 60, screenHeight/2 + 10, 20, WHITE);
    DrawText("Press [R] to Replay this level", screenWidth/2 - 150, screenHeight/2 + 80, 18, SKYBLUE);
    DrawText("Press [N] to Next level", screenWidth/2 - 130, screenHeight/2 + 120, 18, ORANGE);
    DrawText("Press [ESC] to Level Select", screenWidth/2 - 150, screenHeight/2 + 160, 18, GRAY);
}

void Game::DrawLevelSelect() {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.9f));
    
    DrawText("SELECT LEVEL", screenWidth/2 - 120, 60, 40, YELLOW);
    DrawText("Choose your difficulty", screenWidth/2 - 100, 110, 20, GRAY);
    
    int cardWidth = 200;
    int cardHeight = 250;
    int startCardX = (screenWidth - (cardWidth * 3 + 40)) / 2;
    int cardY = 160;
    
    Color colors[3] = { GREEN, ORANGE, RED };
    const char* difficulties[3] = { "EASY", "NORMAL", "HARD" };
    
    for (int i = 0; i < 3; i++) {
        int cardX = startCardX + i * (cardWidth + 20);
        const LevelConfig& level = levels[i];
        
        Color cardColor = ColorAlpha(colors[i], 0.3f);
        if (selectedLevel == i + 1) {
            cardColor = ColorAlpha(colors[i], 0.6f);
            DrawRectangleLines(cardX - 4, cardY - 4, cardWidth + 8, cardHeight + 8, GOLD);
        }
        
        DrawRectangle(cardX, cardY, cardWidth, cardHeight, cardColor);
        DrawRectangleLines(cardX, cardY, cardWidth, cardHeight, colors[i]);
        
        DrawText(TextFormat("%d", i + 1), cardX + cardWidth/2 - 15, cardY + 20, 30, colors[i]);
        DrawText(level.levelName.c_str(), cardX + cardWidth/2 - MeasureText(level.levelName.c_str(), 18)/2, 
                 cardY + 60, 18, WHITE);
        DrawText(difficulties[i], cardX + cardWidth/2 - 30, cardY + 90, 16, colors[i]);
        
        DrawText(TextFormat("Ball Speed: x%.0f%%", level.ballSpeedMultiplier * 100), 
                 cardX + 15, cardY + 130, 12, GRAY);
        DrawText(TextFormat("Paddle Speed: x%.0f%%", level.paddleSpeedMultiplier * 100), 
                 cardX + 15, cardY + 155, 12, GRAY);
        DrawText(TextFormat("Lives: %d", level.maxLives), 
                 cardX + 15, cardY + 180, 12, GRAY);
        DrawText(TextFormat("Score: x%d", level.scoreMultiplier), 
                 cardX + 15, cardY + 205, 12, GRAY);
        
        DrawText(TextFormat("[%d] Press %d", i + 1, i + 1), 
                 cardX + cardWidth/2 - 45, cardY + cardHeight - 30, 14, colors[i]);
    }
    
    DrawText("Press 1, 2 or 3 to select level", screenWidth/2 - 140, screenHeight - 80, 16, SKYBLUE);
    DrawText("Press ESC to return to menu", screenWidth/2 - 110, screenHeight - 50, 14, GRAY);
    
    if (isFrenzyMode) {
        DrawText("FRENZY MODE ACTIVE!", screenWidth/2 - 100, 140, 18, ORANGE);
    }
}

void Game::Update() {
    double frameStart = GetTime();
    
    // ========== 网络更新 ==========
    UpdateNetwork();
    
    // ========== 输入处理 ==========
    HandleInput();
    
    // ========== 挡板更新 ==========
    paddle.Update(GetFrameTime());
    
    // ========== 异步加载更新 ==========
    UpdateAsyncLoading();
    
    // ========== 移动砖块更新（金字塔关卡等） ==========
    UpdateMovingBricks(GetFrameTime());
    
    // ========== 传送门冷却更新 ==========
    float now = GetTime();
    for (auto it = portalCooldowns.begin(); it != portalCooldowns.end();) {
        if (now - it->second > PORTAL_COOLDOWN_DURATION) {
            it = portalCooldowns.erase(it);
        } else {
            ++it;
        }
    }
    
    // ========== 游戏逻辑更新（仅在PLAYING状态） ==========
    if (currentState == GameState::PLAYING) {
        // 主机模式或单人模式：更新游戏逻辑
        if (isHost || !isConnected) {
            UpdateGame();
        }
        
        // ========== 道具效果更新 ==========
        UpdateEffects(GetFrameTime());
        
        // ========== 粒子系统更新（使用优化版粒子池） ==========
        double particleStart = GetTime();
        optimizedParticlePool.Update(GetFrameTime());
        particleTimeMs = (GetTime() - particleStart) * 1000;
        
        // ========== 脏标记空间划分更新（按需重建） ==========
        // 优化说明：每帧调用RebuildIfNeeded，但实际重建频率被限制为每3帧一次
        // 这样可以避免每帧重建造成的CPU浪费，同时保证碰撞检测的准确性
        if (useSpatialPartition) {
            double spatialStart = GetTime();
            // 参数3表示最多每3帧重建一次
            // 砖块被击碎时会通过MarkBrickDirty标记脏单元格
            dirtySpatialGrid.RebuildIfNeeded(3);
            spatialTimeMs = (GetTime() - spatialStart) * 1000;
        }
        
        // ========== 额外小球更新 ==========
        UpdateExtraBalls(GetFrameTime());
        
        // ========== 道具掉落物更新 ==========
        for (auto& powerUp : powerUps) {
            powerUp.Update(GetFrameTime());
        }
        
        // ========== 道具拾取检测 ==========
        CheckPowerUpCollisions();
    }
    
    // ========== 帧耗时统计 ==========
    totalFrameTimeMs = (GetTime() - frameStart) * 1000;
}

void Game::Draw() {
    BeginDrawing();
    ClearBackground(BLACK);
    
    switch (currentState) {
        case GameState::MENU:
            DrawMenu();
            DrawAsyncLoadingUI();
            break;
        case GameState::LEVEL_SELECT:
            DrawLevelSelect();
            break;
        case GameState::PLAYING:
            ball.Draw();
            DrawExtraBalls();
            paddle.Draw();
            for (auto& brick : bricks) brick.Draw();
            for (auto& powerUp : powerUps) powerUp.Draw();
            DrawParticlesPooled();
            DrawUI();
            break;
        case GameState::PAUSED:
            ball.Draw();
            DrawExtraBalls();
            paddle.Draw();
            for (auto& brick : bricks) brick.Draw();
            for (auto& powerUp : powerUps) powerUp.Draw();
            DrawParticlesPooled();
            DrawUI();
            DrawPaused();
            break;
        case GameState::LEADERBOARD:
            DrawLeaderboard();
            break;
        case GameState::VICTORY_MENU:
            ball.Draw();
            DrawExtraBalls();
            paddle.Draw();
            for (auto& brick : bricks) brick.Draw();
            for (auto& powerUp : powerUps) powerUp.Draw();
            DrawParticlesPooled();
            DrawUI();
            DrawVictoryMenu();
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

void Game::SendGameStateToClient() {
}

void Game::ReceiveGameStateFromHost() {
}

void Game::Shutdown() {
    SaveLeaderboard();
    if (asyncLoader) {
        delete asyncLoader;
        asyncLoader = nullptr;
    }
    if (loadedDemoTexture.id != 0) {
        UnloadTexture(loadedDemoTexture);
    }
    if (netHost) {
        if (netPeer) {
            enet_peer_disconnect(netPeer, 0);
            ENetEvent event;
            while (enet_host_service(netHost, &event, 3000) > 0) {
                if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                    break;
                }
            }
        }
        enet_host_destroy(netHost);
        netHost = nullptr;
    }
    enet_deinitialize();
    TraceLog(LOG_INFO, "Game shutdown");
}

void Game::RequestAsyncLoad(const std::string& texturePath) {
    if (!asyncLoader) return;
    if (asyncLoader->IsLoading()) return;
    if (asyncLoader->IsLoaded()) asyncLoader->ResetLoadedState();
    isLoadingRequested = true;
    asyncLoader->StartLoadTexture(texturePath);
}

bool Game::IsAsyncLoading() const {
    return asyncLoader ? asyncLoader->IsLoading() : false;
}

float Game::GetAsyncLoadProgress() const {
    return asyncLoader ? asyncLoader->GetProgress() : 0.0f;
}

void Game::UpdateAsyncLoading() {
    if (!asyncLoader) return;
    Texture2D loadedTex;
    if (asyncLoader->TryGetLoadedTexture(loadedTex)) {
        if (loadedTex.id != 0) {
            loadedDemoTexture = loadedTex;
            showLoadedTexture = true;
            textureDisplayTimer = 3.0f;
        }
    }
    if (showLoadedTexture) {
        textureDisplayTimer -= GetFrameTime();
        if (textureDisplayTimer <= 0) {
            showLoadedTexture = false;
        }
    }
}

void Game::DrawAsyncLoadingUI() {
    if (asyncLoader && asyncLoader->IsLoading()) {
        float progress = asyncLoader->GetProgress();
        DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.7f));
        int barWidth = 300;
        int barHeight = 20;
        int barX = screenWidth/2 - barWidth/2;
        int barY = screenHeight/2 - 10;
        DrawRectangle(barX, barY, barWidth, barHeight, DARKGRAY);
        DrawRectangle(barX, barY, (int)(barWidth * progress), barHeight, LIME);
        DrawText(TextFormat("%d%%", (int)(progress * 100)), screenWidth/2 - 20, barY - 25, 20, WHITE);
        DrawText("Loading texture in background...", screenWidth/2 - 130, barY + 30, 16, GRAY);
    }
    
    if (showLoadedTexture && loadedDemoTexture.id != 0) {
        Rectangle texRect = { (float)(screenWidth - 100), 10.0f, 80.0f, 80.0f };
        DrawTexturePro(loadedDemoTexture, (Rectangle){0, 0, (float)loadedDemoTexture.width, (float)loadedDemoTexture.height}, texRect, (Vector2){0, 0}, 0, WHITE);
        DrawText("TEXTURE LOADED!", screenWidth - 200, 95, 12, GREEN);
        
        float alpha = (sin(GetTime() * 5) + 1) / 2;
        DrawText("BRICK COLORS CHANGED!", screenWidth/2 - 100, screenHeight - 40, 16, ColorAlpha(GREEN, alpha));
    }
}

void Game::BuildSpatialGrid() {
    for (int x = 0; x < GRID_COLS; x++) {
        for (int y = 0; y < GRID_ROWS; y++) {
            grid[x][y].brickIndices.clear();
        }
    }
    for (size_t i = 0; i < bricks.size(); i++) {
        if (!bricks[i].IsActive()) continue;
        Rectangle rect = bricks[i].GetRect();
        int startCol = std::max(0, (int)(rect.x / CELL_WIDTH));
        int endCol = std::min(GRID_COLS - 1, (int)((rect.x + rect.width) / CELL_WIDTH));
        int startRow = std::max(0, (int)(rect.y / CELL_HEIGHT));
        int endRow = std::min(GRID_ROWS - 1, (int)((rect.y + rect.height) / CELL_HEIGHT));
        for (int col = startCol; col <= endCol; col++) {
            for (int row = startRow; row <= endRow; row++) {
                grid[col][row].brickIndices.push_back((int)i);
            }
        }
    }
}

void Game::GetNearbyBricks(const Ball& ball, std::vector<int>& outIndices) {
    outIndices.clear();
    Vector2 pos = ball.GetPosition();
    float radius = ball.GetRadius();
    int startCol = std::max(0, (int)((pos.x - radius) / CELL_WIDTH));
    int endCol = std::min(GRID_COLS - 1, (int)((pos.x + radius) / CELL_WIDTH));
    int startRow = std::max(0, (int)((pos.y - radius) / CELL_HEIGHT));
    int endRow = std::min(GRID_ROWS - 1, (int)((pos.y + radius) / CELL_HEIGHT));
    std::unordered_set<int> uniqueIndices;
    for (int col = startCol; col <= endCol; col++) {
        for (int row = startRow; row <= endRow; row++) {
            for (int idx : grid[col][row].brickIndices) {
                if (idx >= 0 && idx < (int)bricks.size() && bricks[idx].IsActive()) {
                    uniqueIndices.insert(idx);
                }
            }
        }
    }
    outIndices.assign(uniqueIndices.begin(), uniqueIndices.end());
}

// 生成粒子（池化版本）
// 优化说明：优先使用优化版粒子池（空闲列表O(1)分配）
//          当优化版池满时，回退到原有线性查找版本
// 参数：
//   pos: 粒子起始位置
//   vel: 粒子初始速度
//   color: 粒子颜色
//   lifetime: 粒子生命时长（秒）
void Game::SpawnParticlePooled(Vector2 pos, Vector2 vel, Color color, float lifetime) {
    // ========== 优先使用优化版粒子池（O(1)分配） ==========
    if (optimizedParticlePool.Spawn(pos, vel, color, lifetime)) {
        return;  // 优化版分配成功
    }
    
    // ========== 回退：原有线性查找版本（保留兼容） ==========
    // 当优化版粒子池满时（500粒子全部活跃），使用原有算法
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (pooledParticles[i].active == 0) {  // 注意：原代码中active是float类型，0表示未激活
            pooledParticles[i].position = pos;
            pooledParticles[i].velocity = vel;
            pooledParticles[i].color = color;
            pooledParticles[i].life = lifetime;
            pooledParticles[i].maxLife = lifetime;
            pooledParticles[i].active = 1.0f;  // 设为活跃
            activeParticleCount++;
            return;
        }
    }
    
    // 粒子池满：覆盖最旧的粒子（环形缓冲区策略）
    static int fallbackIndex = 0;
    fallbackIndex = (fallbackIndex + 1) % MAX_PARTICLES;
    pooledParticles[fallbackIndex].active = 1.0f;
    pooledParticles[fallbackIndex].position = pos;
    pooledParticles[fallbackIndex].velocity = vel;
    pooledParticles[fallbackIndex].color = color;
    pooledParticles[fallbackIndex].life = lifetime;
    pooledParticles[fallbackIndex].maxLife = lifetime;
}


// 更新所有粒子（每帧调用）
// 优化说明：优先使用优化版粒子池更新，原有版本保留用于回退
// 更新所有粒子（每帧调用）
// 优化说明：优先使用优化版粒子池更新，原有版本保留用于回退
void Game::UpdateParticlesPooled(float dt) {
    // ========== 使用优化版粒子池更新 ==========
    optimizedParticlePool.Update(dt);
    
    // ========== 原有版本（保留用于回退，但通常不会执行） ==========
    // 注意：优化版已经处理了所有粒子的更新和回收
    // 以下代码仅在优化版未初始化时执行（保留兼容）
    activeParticleCount = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (pooledParticles[i].active == 0) continue;
        
        pooledParticles[i].position.x += pooledParticles[i].velocity.x * dt * 60;
        pooledParticles[i].position.y += pooledParticles[i].velocity.y * dt * 60;
        pooledParticles[i].velocity.y += 200.0f * dt;
        pooledParticles[i].life -= dt;
        
        if (pooledParticles[i].life <= 0) {
            pooledParticles[i].active = 0;
        } else {
            activeParticleCount++;
        }
    }
}

// 绘制所有粒子
// 优化说明：优先使用优化版粒子池绘制
void Game::DrawParticlesPooled() {
    // ========== 使用优化版粒子池绘制 ==========
    optimizedParticlePool.Draw();
    
    // ========== 原有版本（保留兼容） ==========
    // 注意：优化版已绘制所有活跃粒子，以下代码仅在优化版未使用时执行
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (pooledParticles[i].active == 0) continue;
        float alpha = pooledParticles[i].life / pooledParticles[i].maxLife;
        DrawCircleV(pooledParticles[i].position, 3, ColorAlpha(pooledParticles[i].color, alpha));
    }
}

// 生成砖块破碎粒子特效
// 参数：
//   brickRect: 砖块矩形区域
//   brickColor: 砖块颜色（用于粒子颜色）
void Game::SpawnBrickParticles(Rectangle brickRect, Color brickColor) {
    // ========== 使用优化版粒子池，保持原版视觉效果 ==========
    // 优化说明：使用Spawn方法（O(1)分配），而非原版的线性查找
    // 但粒子的速度、数量、位置计算完全保持原版，确保视觉效果一致
    
    for (int i = 0; i < 12; i++) {
        // 原版速度计算：X: ((rand()%100)-50)/5.0f, Y: ((rand()%100)-80)/5.0f
        Vector2 vel = { 
            ((rand() % 100) - 50) / 5.0f, 
            ((rand() % 100) - 80) / 5.0f 
        };
        
        // 原版位置：砖块内随机位置
        Vector2 pos = { 
            brickRect.x + (rand() % (int)brickRect.width), 
            brickRect.y + (rand() % (int)brickRect.height) 
        };
        
        // 使用优化版粒子池（O(1)分配，无线性查找）
        optimizedParticlePool.Spawn(pos, vel, brickColor, 0.6f);
    }
}

// 生成道具光晕粒子特效
// 参数：
//   x, y: 道具中心坐标
//   color: 道具颜色
void Game::SpawnPowerUpGlow(float x, float y, Color color) {
    Vector2 center = {x, y};
    
    // ========== 使用优化版粒子池批量生成 ==========
    optimizedParticlePool.SpawnBurst(center, 8, color, 80.0f, 0.3f);
}

bool Game::SaveExists() const {
    std::ifstream file("savegame.json");
    return file.good();
}

json Game::LoadJSONFromFile(const std::string& path) {
    json defaultConfig;
    defaultConfig["error"] = true;
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return defaultConfig;
        }
        json config;
        file >> config;
        return config;
    } catch (const json::parse_error& e) {
        return defaultConfig;
    }
}

void Game::InitBricksFromJSON(const json& config) {
    bricks.clear();
    
    if (!config.contains("bricks") || !config["bricks"].contains("layout_data")) {
        InitBricksByLayout(0);
        return;
    }
    
    const auto& bricksConfig = config["bricks"];
    int rows = bricksConfig.value("rows", brickRows);
    int cols = bricksConfig.value("cols", brickCols);
    float width = bricksConfig.value("width", brickWidth);
    float height = bricksConfig.value("height", brickHeight);
    float sX = bricksConfig.value("start_x", startX);
    float sY = bricksConfig.value("start_y", startY);
    float sp = bricksConfig.value("spacing", spacing);
    
    const auto& layoutData = bricksConfig["layout_data"];
    
    for (int row = 0; row < rows && row < (int)layoutData.size(); row++) {
        Color rowColor = brickColors[row % 8];
        const auto& rowData = layoutData[row];
        
        for (int col = 0; col < cols && col < (int)rowData.size(); col++) {
            int brickCode = rowData[col];
            if (brickCode == 0) continue;
            
            BrickType brickType;
            switch (brickCode) {
                case 1: brickType = BrickType::NORMAL; break;
                case 2: brickType = BrickType::SPLIT; break;
                case 3: brickType = BrickType::PORTAL; break;
                case 4: brickType = BrickType::MOVING; break;
                default: brickType = BrickType::NORMAL; break;
            }
            
            bricks.emplace_back(
                sX + col * (width + sp),
                sY + row * (height + sp),
                width, height,
                rowColor,
                brickType
            );
        }
    }
}

void Game::CheckForSaveFile() {
    if (SaveExists()) {
        TraceLog(LOG_INFO, "Save file detected! Press L to continue or any key for new game");
    }
}

void Game::UpdateMovingBricks(float dt) {
    for (auto& brick : bricks) {
        if (brick.IsActive() && brick.IsMoving()) {
            brick.Update(dt);
        }
    }
}

void Game::CreateHeavyBall(Vector2 position, Vector2 velocity) {
    Ball heavyBall(position, velocity, ballRadius);
    heavyBall.SetLaunched(true);
    heavyBall.SetMainBall(false);
    heavyBall.SetHeavyBall(true);
    heavyBall.ResetSplitCount();
    
    extraBalls.push_back(heavyBall);
    
    for (int i = 0; i < 20; i++) {
        Vector2 vel = {
            ((rand() % 100) - 50) / 4.0f,
            ((rand() % 100) - 50) / 4.0f
        };
        SpawnParticlePooled(position, vel, GOLD, 0.5f);
    }
}