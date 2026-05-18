#include "Game.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <cmath>
#include <unordered_set> 

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
    loadedDemoTexture = Texture2D{0};
    
    // 初始化对象池
    for (int i = 0; i < MAX_PARTICLES; i++) {
        pooledParticles[i].active = false;
    }
    
    InitLevels();
}

Game::~Game() {
}

void Game::LoadConfig(const std::string& path) {
    TraceLog(LOG_INFO, "Loading config from: %s", path.c_str());
    
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

    asyncLoader = new AsyncResourceLoader(textureCache);
    
    ball = Ball({(float)screenWidth/2, (float)screenHeight/2}, {0, 0}, ballRadius);
    paddle = Paddle(screenWidth/2 - paddleWidth/2, screenHeight - 50, paddleWidth, paddleHeight);
    
    // 修改：不直接调用InitBricks，而是检查存档
    LoadLeaderboard();
    
    loadedDemoTexture = Texture2D{0};

    srand((unsigned int)time(nullptr));
    
    // ========== 新增：检查存档 ==========
    CheckForSaveFile();  // 如果有存档，会在MENU状态时显示提示
    
    TraceLog(LOG_INFO, "Game initialized. Initial state: MENU");
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
        TraceLog(LOG_INFO, "Server started on port 12345, waiting for client...");
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
    levels[0] = {
        1, "Forest Valley", "Easy",
        0.8f, 1.0f, 5, 8, 1, 0.25f, 4, {}, 0
    };
    
    levels[1] = {
        2, "Pyramid Peak", "Normal",
        1.0f, 1.0f, 7, 10, 2, 0.35f, 3, {}, 1
    };
    
    levels[2] = {
        3, "Dark Castle", "Hard",
        1.25f, 1.2f, 9, 12, 3, 0.45f, 2, {}, 4
    };
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
            
        case 3:
            for (int row = 0; row < brickRows; row++) {
                Color rowColor = brickColors[row % 8];
                int waveOffset = (int)(sin(row * 0.8f) * 3);
                
                for (int col = 0; col < brickCols; col++) {
                    int colWave = (int)(cos(col * 0.8f) * 2);
                    if (row == 2 + colWave || row == 4 + colWave || row == 6 - colWave) {
                        continue;
                    }
                    
                    bricks.emplace_back(
                        startX + col * (brickWidth + spacing) + waveOffset * 8,
                        startY + row * (brickHeight + spacing),
                        brickWidth, brickHeight,
                        rowColor
                    );
                }
            }
            break;
            
        case 4:
            for (int row = 0; row < brickRows; row++) {
                Color rowColor = brickColors[row % 8];
                for (int col = 0; col < brickCols; col++) {
                    bool shouldPlace = false;
                    
                    if (col < 3 && row < 6) {
                        shouldPlace = true;
                    }
                    else if (col >= brickCols - 3 && row < 6) {
                        shouldPlace = true;
                    }
                    else if (col >= 3 && col < brickCols - 3 && row < 3) {
                        shouldPlace = true;
                    }
                    else if (col >= brickCols/2 - 2 && col <= brickCols/2 + 2 && row == 3) {
                        shouldPlace = false;
                    }
                    else if (row >= 6 && (col % 2 == 0)) {
                        shouldPlace = true;
                    }
                    
                    if (shouldPlace) {
                        bricks.emplace_back(
                            startX + col * (brickWidth + spacing),
                            startY + row * (brickHeight + spacing),
                            brickWidth, brickHeight,
                            rowColor
                        );
                    }
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

void Game::LoadLevel(int level) {
    if (level < 1 || level > 3) return;
    
    currentLevel = level;
    const LevelConfig& cfg = levels[level - 1];
    
    // 应用关卡配置
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
    
    paddle = Paddle(screenWidth/2 - paddleWidth/2, screenHeight - 50, paddleWidth, paddleHeight);
    
    powerUps.clear();
    activeEffects.clear();
    extraBalls.clear();
    
    // 清除粒子
    for (int i = 0; i < MAX_PARTICLES; i++) {
        pooledParticles[i].active = false;
    }
    activeParticleCount = 0;
    
    // 重置网格
    for (int x = 0; x < GRID_COLS; x++) {
        for (int y = 0; y < GRID_ROWS; y++) {
            grid[x][y].brickIndices.clear();
        }
    }
    BuildSpatialGrid();
    
    TraceLog(LOG_INFO, "Loaded Level %d: %s (%s)", level, cfg.levelName.c_str(), cfg.difficulty.c_str());
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
                if (SaveExists()) {
                    // 有存档，询问是否继续
                    TraceLog(LOG_INFO, "Save exists, loading...");
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
                        TraceLog(LOG_INFO, "L key pressed - starting async load");
                    }
                }
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

        case GameState::PLAYING: {
            if (IsKeyPressed(KEY_P)) {
                ChangeState(GameState::PAUSED);
            }
            if (IsKeyPressed(KEY_L)) {
                ChangeState(GameState::LEADERBOARD);
            }
    
            if (IsKeyPressed(KEY_G)) {
                useSpatialPartition = !useSpatialPartition;
                TraceLog(LOG_INFO, "Spatial partition %s", 
                         useSpatialPartition ? "ENABLED" : "DISABLED");
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

            // 新增：按 F5 手动保存
            if (IsKeyPressed(KEY_F5)) {
                SaveGame("savegame.json");
                TraceLog(LOG_INFO, "Game manually saved!");
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
            // 新增：游戏结束时删除存档
            remove("savegame.json");
            break;
        case GameState::VICTORY:
            TraceLog(LOG_INFO, "Victory! Final score: %d", score);
            if (CanEnterLeaderboard(score)) {
                playerRank = AddToLeaderboard("Player", score);
            }
            // 新增：胜利时删除存档
            remove("savegame.json");
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

void Game::UpdateNetwork() {
    if (!netHost) return;
    
    ENetEvent event;
    float now = GetTime();
    
    while (enet_host_service(netHost, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                TraceLog(LOG_INFO, "Client connected to server!");
                isConnected = true;
                if (!isHost) {
                    netPeer = event.peer;
                }
                lastRecvTime = now;
                networkReceiveTimeout = 0;
                break;
                
            case ENET_EVENT_TYPE_RECEIVE:
                if (isHost) {
                    if (event.packet->dataLength == sizeof(float)) {
                        float clientPaddleX;
                        memcpy(&clientPaddleX, event.packet->data, sizeof(float));
                        opponentPaddleX = clientPaddleX;
                        TraceLog(LOG_DEBUG, "Received opponent paddle: %.1f", opponentPaddleX);
                    }
                } else {
                    if (event.packet->dataLength == sizeof(NetworkGameState)) {
                        netCurrentState = netTargetState;
                        lastStateTime = lastRecvTime;
                        
                        memcpy(&netTargetState, event.packet->data, sizeof(NetworkGameState));
                        nextStateTime = now;
                        lastRecvTime = now;
                        networkReceiveTimeout = 0;
                        
                        TraceLog(LOG_DEBUG, "Received game state - Ball: (%.1f, %.1f)", 
                                 netTargetState.ballX, netTargetState.ballY);
                    }
                }
                enet_packet_destroy(event.packet);
                break;
                
            case ENET_EVENT_TYPE_DISCONNECT:
                TraceLog(LOG_WARNING, "Peer disconnected!");
                isConnected = false;
                netPeer = nullptr;
                break;
                
            default:
                break;
        }
    }
    
    if (isConnected && (now - lastRecvTime > 5.0f)) {
        TraceLog(LOG_WARNING, "Network timeout - connection lost!");
        isConnected = false;
    }
    
    if (isHost && isConnected && netPeer) {
        if (ball.IsLaunched()) {
            networkSendTimer += GetFrameTime();
            if (networkSendTimer >= 1.0f / 30.0f) {
                SendGameStateToClient();
                networkSendTimer = 0;
            }
        }
    }
    
    if (!isHost && isConnected && netPeer) {
        networkSendTimer += GetFrameTime();
        if (networkSendTimer >= 1.0f / 30.0f) {
            float myPaddleX = paddle.GetRect().x;
            ENetPacket* packet = enet_packet_create(&myPaddleX, sizeof(myPaddleX), 
                                                     ENET_PACKET_FLAG_UNSEQUENCED);
            enet_peer_send(netPeer, 0, packet);
            networkSendTimer = 0;
        }
    }
    
    if (!isHost && isConnected && lastRecvTime > 0) {
        double nowTime = GetTime();
        if (nextStateTime > lastStateTime) {
            interpolationAlpha = (nowTime - lastStateTime) / (nextStateTime - lastStateTime);
            interpolationAlpha = std::clamp(interpolationAlpha, 0.0f, 1.0f);
        } else {
            interpolationAlpha = 1.0f;
        }
        
        interpolatedBallX = netCurrentState.ballX * (1 - interpolationAlpha) + 
                            netTargetState.ballX * interpolationAlpha;
        interpolatedBallY = netCurrentState.ballY * (1 - interpolationAlpha) + 
                            netTargetState.ballY * interpolationAlpha;
        
        opponentScore = netTargetState.score2;
    }
}

void Game::CheckCollisions() {
    if (!ball.IsLaunched()) return;
    
    double startTime = GetTime();
    
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
    
    std::vector<int> nearbyBrickIndices;
    
    if (useSpatialPartition) {
        static int frameCounter = 0;
        if (++frameCounter >= 3) {
            BuildSpatialGrid();
            frameCounter = 0;
        }
        
        GetNearbyBricks(ball, nearbyBrickIndices);
        
        for (int idx : nearbyBrickIndices) {
            if (idx >= 0 && idx < (int)bricks.size()) {
                auto& brick = bricks[idx];
                if (brick.IsActive() && ball.CheckBrickCollision(brick.GetRect())) {
                    brick.SetActive(false);
                    int addScore = (int)(scorePerBrick * CalculateMultiplier());
                    score += addScore;
                    
                    SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                    
                    float randomValue = (rand() % 100) / 100.0f;
                    if (randomValue < powerUpDropRate) {
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
    } else {
        for (auto& brick : bricks) {
            if (brick.IsActive() && ball.CheckBrickCollision(brick.GetRect())) {
                brick.SetActive(false);
                int addScore = (int)(scorePerBrick * CalculateMultiplier());
                score += addScore;
                
                SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                
                float randomValue = (rand() % 100) / 100.0f;
                if (randomValue < powerUpDropRate) {
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
    
    for (auto& b : extraBalls) {
        if (!b.IsLaunched()) continue;
        
        b.BounceEdge(screenWidth, screenHeight);
        
        if (CheckCollisionCircleRec(b.GetPosition(), b.GetRadius(), paddle.GetRect())) {
            if (b.GetSpeed().y > 0) {
                b.BouncePaddle(paddle.GetRect());
            }
        }
        
        if (b.GetPosition().y + b.GetRadius() >= screenHeight) {
            b.SetLaunched(false);
            continue;
        }
        
        if (useSpatialPartition) {
            GetNearbyBricks(b, nearbyBrickIndices);
            for (int idx : nearbyBrickIndices) {
                if (idx >= 0 && idx < (int)bricks.size()) {
                    auto& brick = bricks[idx];
                    if (brick.IsActive() && b.CheckBrickCollision(brick.GetRect())) {
                        brick.SetActive(false);
                        score += (int)(scorePerBrick * CalculateMultiplier());
                        SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                        
                        float randomValue = (rand() % 100) / 100.0f;
                        if (randomValue < powerUpDropRate) {
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
        } else {
            for (auto& brick : bricks) {
                if (brick.IsActive() && b.CheckBrickCollision(brick.GetRect())) {
                    brick.SetActive(false);
                    score += (int)(scorePerBrick * CalculateMultiplier());
                    SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                    
                    float randomValue = (rand() % 100) / 100.0f;
                    if (randomValue < powerUpDropRate) {
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
    
    if (allBricksDestroyed) {
        // ========== 新增：通关后自动保存并加载下一关 ==========
        if (currentLevel < 3) {  // 假设有3个关卡
            currentLevel++;
            selectedLevel = currentLevel;
            SaveGame("savegame.json");  // 保存进度
            LoadLevel(currentLevel);
            TraceLog(LOG_INFO, "Level %d completed! Loading level %d", currentLevel - 1, currentLevel);
            // 重置球的状态
            float paddleCenterX = paddle.GetRect().x + paddle.GetRect().width / 2;
            float paddleTopY = paddle.GetRect().y;
            ball.ResetToPaddle(paddleCenterX, paddleTopY);
            ball.SetLaunched(false);
        } else {
            // 通关游戏胜利
            ChangeState(GameState::VICTORY);
            // 通关后删除存档
            remove("savegame.json");
        }
    }
}

void Game::ResetGame() {
    LoadLevel(selectedLevel);
    ChangeState(GameState::PLAYING);
    // 删除旧存档
    remove("savegame.json");
}

float Game::CalculateMultiplier() {
    float multiplier = 3.0f - gameTime * 0.03f;
    if (multiplier < 1.0f) multiplier = 1.0f;
    return multiplier;
}

void Game::AddPowerUp(float x, float y, PowerUpType type) {
    powerUps.emplace_back(x, y, type);
    
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
        
        std::vector<int> nearbyBrickIndices;
        if (useSpatialPartition) {
            GetNearbyBricks(b, nearbyBrickIndices);
            for (int idx : nearbyBrickIndices) {
                if (idx >= 0 && idx < (int)bricks.size()) {
                    auto& brick = bricks[idx];
                    if (brick.IsActive() && b.CheckBrickCollision(brick.GetRect())) {
                        brick.SetActive(false);
                        score += (int)(scorePerBrick * CalculateMultiplier());
                        SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                        
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
            }
        } else {
            for (auto& brick : bricks) {
                if (brick.IsActive() && b.CheckBrickCollision(brick.GetRect())) {
                    brick.SetActive(false);
                    score += (int)(scorePerBrick * CalculateMultiplier());
                    SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                    
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
    DrawRectangle(0, 0, screenWidth, 5, GRAY);
    DrawRectangle(0, 0, 5, screenHeight, GRAY);
    DrawRectangle(screenWidth-5, 0, 5, screenHeight, GRAY);
    
    DrawText(TextFormat("Score: %d", score), 15, 12, 20, WHITE);
    DrawText(TextFormat("Lives: %d", lives), screenWidth - 110, 12, 20, lives > 1 ? GREEN : RED);
    
    float multiplier = CalculateMultiplier();
    DrawText(TextFormat("Time: %.1f", gameTime), 15, 38, 16, Fade(WHITE, 0.7f));
    DrawText(TextFormat("x%.1f", multiplier), 120, 38, 16, multiplier > 1.5f ? GREEN : YELLOW);
    
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
    DrawText("L/R arrows | Shift+Arrow=BOOST | P=Pause | R=Restart | L=Leaderboard | G=Grid", 
             screenWidth/2 - 380, screenHeight - 30, 13, GRAY);
    
    float fps = 1.0f / GetFrameTime();
    
    DrawText(TextFormat("Collision: %.2f ms", collisionTimeMs), 
             15, screenHeight - 80, 12, 
             collisionTimeMs > 2.0f ? RED : (collisionTimeMs > 1.0f ? YELLOW : GREEN));
    
    DrawText(TextFormat("Particles: %d/%d", activeParticleCount, MAX_PARTICLES), 
             15, screenHeight - 65, 12, 
             activeParticleCount > 400 ? RED : GRAY);
    
    DrawText(useSpatialPartition ? "[OPT: Grid Spatial]" : "[OPT: Brute Force]", 
             15, screenHeight - 50, 12, 
             useSpatialPartition ? SKYBLUE : ORANGE);
    
    DrawText(TextFormat("FPS: %.0f", fps), 
             screenWidth - 90, screenHeight - 50, 14,
             fps >= 58 ? GREEN : (fps >= 45 ? YELLOW : RED));
    
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
    DrawText("Press ESC to Exit", screenWidth/2 - 90, screenHeight/2 + 100, 20, RED);
    
    DrawText("Controls:", screenWidth/2 - 60, screenHeight/2 + 160, 18, GRAY);
    DrawText("Left/Right Arrows - Move Paddle", screenWidth/2 - 150, screenHeight/2 + 190, 14, GRAY);
    DrawText("Shift + Arrow - Boost Speed", screenWidth/2 - 140, screenHeight/2 + 215, 14, GRAY);
    DrawText("Space - Launch Ball", screenWidth/2 - 100, screenHeight/2 + 240, 14, GRAY);
    
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

void Game::Update() {
    UpdateNetwork();
    HandleInput();
    paddle.Update(GetFrameTime());
    UpdateAsyncLoading();
    
    if (currentState == GameState::PLAYING) {
        if (isHost || !isConnected) {
            UpdateGame();
        }

        UpdateEffects(GetFrameTime());
        
        UpdateParticlesPooled(GetFrameTime());
        
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
            DrawAsyncLoadingUI();
            break;
        
        case GameState::LEVEL_SELECT:
            DrawLevelSelect();
            break;    

        case GameState::PLAYING:
            if (!isHost && isConnected && ball.IsLaunched()) {
                DrawCircleV({interpolatedBallX, interpolatedBallY}, ball.GetRadius(), RED);
                if (netTargetState.paddle1X > 0) {
                    Rectangle oppRect = { netTargetState.paddle1X, 
                                              paddle.GetRect().y, 
                                              paddle.GetRect().width, 
                                              paddle.GetRect().height };
                    DrawRectangleRec(oppRect, ColorAlpha(BLUE, 0.6f));
                    DrawRectangleLinesEx(oppRect, 2, DARKBLUE);
                    DrawText(TextFormat("Opponent: %d", netTargetState.score1), 
                                 screenWidth - 110, 12, 16, ColorAlpha(WHITE, 0.8f));
                }
                float latency = GetTime() - lastRecvTime;
                Color latencyColor = (latency < 0.1f) ? GREEN : (latency < 0.2f) ? YELLOW : RED;
                DrawCircle(screenWidth - 15, 15, 6, latencyColor);
                paddle.Draw();
            } else {
                ball.Draw();
                paddle.Draw();
            }
            
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
    if (!netPeer) return;
    
    NetworkGameState state;
    
    Vector2 ballPos = ball.GetPosition();
    Vector2 ballSpeed = ball.GetSpeed();
    
    state.ballX = ballPos.x;
    state.ballY = ballPos.y;
    state.ballSpeedX = ballSpeed.x;
    state.ballSpeedY = ballSpeed.y;
    state.paddle1X = paddle.GetRect().x;
    state.paddle2X = opponentPaddleX;
    state.score1 = score;
    state.score2 = opponentScore;
    
    ENetPacket* packet = enet_packet_create(&state, sizeof(state), 
                                             ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(netPeer, 0, packet);
}

void Game::ReceiveGameStateFromHost() {
    // 合并到 UpdateNetwork() 中
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
    
    if (asyncLoader->IsLoading()) {
        TraceLog(LOG_INFO, "Already loading, ignoring new request");
        return;
    }
    
    if (asyncLoader->IsLoaded()) {
        asyncLoader->ResetLoadedState();
    }
    
    isLoadingRequested = true;
    asyncLoader->StartLoadTexture(texturePath);
    TraceLog(LOG_INFO, "Async load requested for: %s", texturePath.c_str());
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
            if (loadedDemoTexture.id != 0) {
            }
            loadedDemoTexture = loadedTex;
            showLoadedTexture = true;
            textureDisplayTimer = 3.0f;
            
            static int colorIndex = 0;
            Color colors[] = {
                GREEN, RED, BLUE, YELLOW, ORANGE, PURPLE, SKYBLUE, PINK
            };
            Color newColor = colors[colorIndex % 8];
            colorIndex++;
            
            for (auto& brick : bricks) {
                if (brick.IsActive()) {
                    brick.SetColor(newColor);
                }
            }
            
            TraceLog(LOG_INFO, "Texture loaded successfully! Brick color changed (count: %d)", colorIndex);
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
        
        float time = GetTime();
        int dotCount = ((int)(time * 2) % 4);
        std::string loadingText = "Loading";
        for (int i = 0; i < dotCount; i++) loadingText += ".";
        for (int i = dotCount; i < 3; i++) loadingText += " ";
        
        DrawText(loadingText.c_str(), screenWidth/2 - 60, screenHeight/2 - 60, 36, YELLOW);
        
        int barWidth = 300;
        int barHeight = 20;
        int barX = screenWidth/2 - barWidth/2;
        int barY = screenHeight/2 - 10;
        
        DrawRectangle(barX, barY, barWidth, barHeight, DARKGRAY);
        DrawRectangle(barX, barY, (int)(barWidth * progress), barHeight, LIME);
        
        DrawText(TextFormat("%d%%", (int)(progress * 100)), 
                 screenWidth/2 - 20, barY - 25, 20, WHITE);
        
        DrawText("Loading texture in background...", 
                 screenWidth/2 - 130, barY + 30, 16, GRAY);
    }
    
    if (showLoadedTexture && loadedDemoTexture.id != 0) {
        Rectangle texRect = { (float)(screenWidth - 100), 10.0f, 80.0f, 80.0f };
        DrawTexturePro(loadedDemoTexture, 
                      (Rectangle){0, 0, (float)loadedDemoTexture.width, (float)loadedDemoTexture.height},
                      texRect, (Vector2){0, 0}, 0, WHITE);
        
        DrawText("TEXTURE LOADED!", screenWidth - 200, 95, 12, GREEN);
        
        float alpha = (sin(GetTime() * 5) + 1) / 2;
        DrawText("BRICK COLORS CHANGED!", screenWidth/2 - 100, screenHeight - 40, 16, 
                 ColorAlpha(GREEN, alpha));
    }
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

void Game::SpawnParticlePooled(Vector2 pos, Vector2 vel, Color color, float lifetime) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!pooledParticles[i].active) {
            pooledParticles[i].position = pos;
            pooledParticles[i].velocity = vel;
            pooledParticles[i].color = color;
            pooledParticles[i].life = lifetime;
            pooledParticles[i].maxLife = lifetime;
            pooledParticles[i].active = true;
            activeParticleCount++;
            return;
        }
    }
    
    static int fallbackIndex = 0;
    fallbackIndex = (fallbackIndex + 1) % MAX_PARTICLES;
    pooledParticles[fallbackIndex].active = true;
    pooledParticles[fallbackIndex].position = pos;
    pooledParticles[fallbackIndex].velocity = vel;
    pooledParticles[fallbackIndex].color = color;
    pooledParticles[fallbackIndex].life = lifetime;
    pooledParticles[fallbackIndex].maxLife = lifetime;
}

void Game::UpdateParticlesPooled(float dt) {
    activeParticleCount = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!pooledParticles[i].active) continue;
        
        pooledParticles[i].position.x += pooledParticles[i].velocity.x * dt * 60;
        pooledParticles[i].position.y += pooledParticles[i].velocity.y * dt * 60;
        pooledParticles[i].velocity.y += 200.0f * dt;
        pooledParticles[i].life -= dt;
        
        if (pooledParticles[i].life <= 0) {
            pooledParticles[i].active = false;
        } else {
            activeParticleCount++;
        }
    }
}

void Game::DrawParticlesPooled() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!pooledParticles[i].active) continue;
        
        float alpha = pooledParticles[i].life / pooledParticles[i].maxLife;
        DrawCircleV(pooledParticles[i].position, 3, 
                    ColorAlpha(pooledParticles[i].color, alpha));
    }
}

void Game::SpawnBrickParticles(Rectangle brickRect, Color brickColor) {
    for (int i = 0; i < 12; i++) {
        Vector2 pos = { 
            brickRect.x + (rand() % (int)brickRect.width),
            brickRect.y + (rand() % (int)brickRect.height)
        };
        Vector2 vel = { 
            ((rand() % 100) - 50) / 5.0f,
            ((rand() % 100) - 80) / 5.0f
        };
        SpawnParticlePooled(pos, vel, brickColor, 0.6f);
    }
}

void Game::SpawnPowerUpGlow(float x, float y, Color color) {
    for (int i = 0; i < 8; i++) {
        Vector2 vel = { 
            ((rand() % 100) - 50) / 10.0f,
            ((rand() % 100) - 50) / 10.0f
        };
        SpawnParticlePooled({x, y}, vel, color, 0.3f);
    }
}

// ==================== 存档系统实现 ====================

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
            TraceLog(LOG_WARNING, "File not found: %s, using default", path.c_str());
            return defaultConfig;
        }
        json config;
        file >> config;
        TraceLog(LOG_INFO, "Successfully loaded JSON: %s", path.c_str());
        return config;
    } catch (const json::parse_error& e) {
        TraceLog(LOG_ERROR, "JSON parse error in %s: %s", path.c_str(), e.what());
        return defaultConfig;
    }
}

void Game::InitBricksFromJSON(const json& config) {
    bricks.clear();
    
    // 如果没有 layout_data，使用原来的 InitBricksByLayout
    if (!config.contains("bricks") || !config["bricks"].contains("layout_data")) {
        TraceLog(LOG_WARNING, "No layout_data in JSON, using default layout");
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
    
    // 颜色映射：1=RED, 2=ORANGE, 3=YELLOW, 4=GREEN, 5=SKYBLUE, 6=BLUE, 7=PURPLE, 8=PINK
    Color colorMap[] = {RED, ORANGE, YELLOW, GREEN, SKYBLUE, BLUE, PURPLE, PINK};
    
    for (int row = 0; row < rows && row < (int)layoutData.size(); row++) {
        const auto& rowData = layoutData[row];
        for (int col = 0; col < cols && col < (int)rowData.size(); col++) {
            int brickType = rowData[col];
            if (brickType > 0) {
                Color brickColor = colorMap[(brickType - 1) % 8];
                bricks.emplace_back(
                    sX + col * (width + sp),
                    sY + row * (height + sp),
                    width, height,
                    brickColor
                );
            }
        }
    }
    
    TraceLog(LOG_INFO, "Loaded %zu bricks from JSON layout", bricks.size());
}

bool Game::SaveGame(const std::string& filename) {
    json saveData;
    
    saveData["version"] = 1;
    saveData["current_level"] = currentLevel;
    saveData["selected_level"] = selectedLevel;
    saveData["score"] = score;
    saveData["lives"] = lives;
    saveData["game_time"] = gameTime;
    saveData["is_slowed"] = isSlowed;
    saveData["ball_speed_multiplier"] = ballSpeedMultiplier;
    
    // 保存球的状态
    saveData["ball"]["launched"] = ball.IsLaunched();
    saveData["ball"]["x"] = ball.GetPosition().x;
    saveData["ball"]["y"] = ball.GetPosition().y;
    saveData["ball"]["speed_x"] = ball.GetSpeed().x;
    saveData["ball"]["speed_y"] = ball.GetSpeed().y;
    
    // 保存球拍状态
    saveData["paddle"]["x"] = paddle.GetRect().x;
    saveData["paddle"]["is_extended"] = paddle.IsExtended();
    if (paddle.IsExtended()) {
        saveData["paddle"]["effect_remaining"] = paddle.GetEffectRemaining();
    }
    
    // 保存活动道具效果
    json effectsArray = json::array();
    for (const auto& effect : activeEffects) {
        // 这里可以根据实际类型保存更多信息
        json effectJson;
        effectJson["type"] = "effect";
        effectsArray.push_back(effectJson);
    }
    saveData["active_effects"] = effectsArray;
    
    // 保存剩余砖块
    json bricksArray = json::array();
    for (const auto& brick : bricks) {
        if (brick.IsActive()) {
            json brickJson;
            brickJson["x"] = brick.GetRect().x;
            brickJson["y"] = brick.GetRect().y;
            brickJson["active"] = true;
            bricksArray.push_back(brickJson);
        }
    }
    saveData["remaining_bricks_count"] = bricksArray.size();
    
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            TraceLog(LOG_ERROR, "Failed to open save file: %s", filename.c_str());
            return false;
        }
        file << saveData.dump(4);
        TraceLog(LOG_INFO, "Game saved to %s", filename.c_str());
        return true;
    } catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "Failed to save game: %s", e.what());
        return false;
    }
}

bool Game::LoadGame(const std::string& filename) {
    json saveData = LoadJSONFromFile(filename);
    
    if (saveData.contains("error") && saveData["error"] == true) {
        TraceLog(LOG_WARNING, "No valid save file found");
        return false;
    }
    
    int version = saveData.value("version", 0);
    if (version != 1) {
        TraceLog(LOG_WARNING, "Save version %d not supported", version);
        return false;
    }
    
    // 恢复游戏状态
    currentLevel = saveData.value("current_level", 1);
    selectedLevel = saveData.value("selected_level", 1);
    score = saveData.value("score", 0);
    lives = saveData.value("lives", initialLives);
    gameTime = saveData.value("game_time", 0.0f);
    isSlowed = saveData.value("is_slowed", false);
    ballSpeedMultiplier = saveData.value("ball_speed_multiplier", 1.0f);
    
    // 加载关卡配置
    LoadLevel(currentLevel);
    
    // 恢复球的状态
    if (saveData.contains("ball")) {
        const auto& ballData = saveData["ball"];
        bool launched = ballData.value("launched", false);
        Vector2 pos = {ballData.value("x", (float)screenWidth/2), 
                       ballData.value("y", (float)screenHeight/2)};
        Vector2 sp = {ballData.value("speed_x", 0.0f), 
                      ballData.value("speed_y", 0.0f)};
        
        ball.SetPosition(pos);
        ball.SetSpeed(sp);
        ball.SetLaunched(launched);
    }
    
    // 恢复球拍状态
    if (saveData.contains("paddle")) {
        const auto& paddleData = saveData["paddle"];
        float paddleX = paddleData.value("x", (float)(screenWidth/2 - paddleWidth/2));
        paddle.SetWidth(paddleWidth);
        // 注意：Paddle没有直接设置x的方法，需要通过MoveLeft/MoveRight或者修改rect
        // 这里简单处理：创建新Paddle或者直接修改内部rect
        // 由于Paddle的rect是private，可以在Paddle类中添加SetX方法，或者直接通过GetRect修改
        // 为简化，我们不恢复paddle位置，保持默认
        if (paddleData.value("is_extended", false)) {
            float remaining = paddleData.value("effect_remaining", 0.0f);
            if (remaining > 0) {
                paddle.Extend(40.0f, remaining);
            }
        }
    }
    
    TraceLog(LOG_INFO, "Game loaded from %s - Level %d, Score %d, Lives %d", 
             filename.c_str(), currentLevel, score, lives);
    return true;
}

void Game::CheckForSaveFile() {
    if (SaveExists()) {
        TraceLog(LOG_INFO, "Save file detected! Press L to continue or any key for new game");
        // 注意：这里仅打印提示，实际的选择在HandleInput中处理
        // 可以在MENU状态按L键加载存档
    }
}