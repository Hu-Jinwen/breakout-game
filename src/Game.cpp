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
    // 网络成员初始化（注意不要重复初始化 currentState）
    , netHost(nullptr)
    , netPeer(nullptr)
    , isHost(false)
    , isConnected(false)
    , lastSendTime(0)
    , lastRecvTime(0)
    , netCurrentState{}      // 改名
    , netTargetState{}       // 改名
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
    , currentLevel(1)      // 新增
    , selectedLevel(1)   // 新增
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
    InitLevels();  // 新增：初始化关卡配置
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

        // 初始化异步加载器
    asyncLoader = new AsyncResourceLoader(textureCache);
    
    ball = Ball({(float)screenWidth/2, (float)screenHeight/2}, {0, 0}, ballRadius);
    paddle = Paddle(screenWidth/2 - paddleWidth/2, screenHeight - 50, paddleWidth, paddleHeight);
    
    InitBricks();
    LoadLeaderboard();
    
    loadedDemoTexture = Texture2D{0};

    srand((unsigned int)time(nullptr));
    
    TraceLog(LOG_INFO, "Game initialized. Initial state: MENU");
}

void Game::InitNetwork(bool asHost, const char* serverIP) {
    // 初始化 ENet 库（只初始化一次）
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
        // ===== 主机模式 =====
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
        // ===== 客户端模式 =====
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
    // ========== 第1关：简单 - 标准矩形布局 ==========
    levels[0] = {
        1,                          // levelNumber
        "Forest Valley",            // levelName
        "Easy",                     // difficulty
        0.8f,                       // ballSpeedMultiplier - 较慢的球速
        1.0f,                       // paddleSpeedMultiplier
        5,                          // brickRows - 5行
        8,                          // brickCols - 8列
        1,                          // scoreMultiplier
        0.25f,                      // powerUpDropRate - 道具掉落率25%
        4,                          // maxLives - 4条命
        {},                         // brickPositions
        0                           // layoutType - 标准布局
    };
    
    // ========== 第2关：中等 - 金字塔布局 ==========
    levels[1] = {
        2,                          // levelNumber
        "Pyramid Peak",             // levelName
        "Normal",                   // difficulty
        1.0f,                       // ballSpeedMultiplier - 正常球速
        1.0f,                       // paddleSpeedMultiplier
        7,                          // brickRows - 7行
        10,                         // brickCols - 10列
        2,                          // scoreMultiplier - 2倍分数
        0.35f,                      // powerUpDropRate - 道具掉落率35%
        3,                          // maxLives - 3条命
        {},                         // brickPositions
        1                           // layoutType - 金字塔布局
    };
    
    // ========== 第3关：困难 - 城堡/防御塔布局 ==========
    levels[2] = {
        3,                          // levelNumber
        "Dark Castle",              // levelName
        "Hard",                     // difficulty
        1.25f,                      // ballSpeedMultiplier - 更快的球速
        1.2f,                       // paddleSpeedMultiplier - 球拍更快
        9,                          // brickRows - 9行
        12,                         // brickCols - 12列
        3,                          // scoreMultiplier - 3倍分数
        0.45f,                      // powerUpDropRate - 道具掉落率45%
        2,                          // maxLives - 2条命
        {},                         // brickPositions
        4                           // layoutType - 城堡布局
    };
}

void Game::InitBricksByLayout(int layoutType) {
    bricks.clear();
    
    switch (layoutType) {
        case 0: // 标准矩形布局
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
            
        case 1: // 金字塔布局
            for (int row = 0; row < brickRows; row++) {
                Color rowColor = brickColors[row % 8];
                int bricksInRow = brickCols - row;  // 每行递减1个砖块
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
            
        case 2: // 菱形布局
            for (int row = 0; row < brickRows; row++) {
                Color rowColor = brickColors[row % 8];
                int bricksInRow;
                int offsetX;
                
                // 菱形：先增加后减少
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
            
        case 3: // 波浪布局
            for (int row = 0; row < brickRows; row++) {
                Color rowColor = brickColors[row % 8];
                int waveOffset = (int)(sin(row * 0.8f) * 3);
                
                for (int col = 0; col < brickCols; col++) {
                    // 波浪形空缺
                    int colWave = (int)(cos(col * 0.8f) * 2);
                    if (row == 2 + colWave || row == 4 + colWave || row == 6 - colWave) {
                        continue;  // 跳过某些位置形成波浪图案
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
            
        case 4: // 城堡/防御塔布局 - 两侧高中间低
            for (int row = 0; row < brickRows; row++) {
                Color rowColor = brickColors[row % 8];
                for (int col = 0; col < brickCols; col++) {
                    bool shouldPlace = false;
                    
                    // 左侧塔楼
                    if (col < 3 && row < 6) {
                        shouldPlace = true;
                    }
                    // 右侧塔楼
                    else if (col >= brickCols - 3 && row < 6) {
                        shouldPlace = true;
                    }
                    // 中间城墙 - 较低
                    else if (col >= 3 && col < brickCols - 3 && row < 3) {
                        shouldPlace = true;
                    }
                    // 城门缺口
                    else if (col >= brickCols/2 - 2 && col <= brickCols/2 + 2 && row == 3) {
                        shouldPlace = false;
                    }
                    // 上层砖块
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
            
        default: // 默认标准布局
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
    
    // 根据砖块数量调整间距
    float totalWidth = brickCols * brickWidth + (brickCols - 1) * spacing;
    if (totalWidth > screenWidth - 100) {
        brickWidth = (screenWidth - 100 - (brickCols - 1) * spacing) / brickCols;
    }
    startX = (screenWidth - brickCols * brickWidth - (brickCols - 1) * spacing) / 2;
    
    // 球速倍率
    ballSpeedMultiplier = cfg.ballSpeedMultiplier;
    
    // 球拍速度
    paddleSpeed = paddleSpeed * cfg.paddleSpeedMultiplier;
    paddleBoostSpeed = paddleBoostSpeed * cfg.paddleSpeedMultiplier;
    
    // 分数倍率
    scorePerBrick = 10 * cfg.scoreMultiplier;
    
    // 道具掉落率
    powerUpDropRate = cfg.powerUpDropRate;
    
    // 根据布局类型创建砖块
    InitBricksByLayout(cfg.layoutType);
    
    // 重置球和球拍
    ball = Ball({(float)screenWidth/2, (float)screenHeight/2}, {0, 0}, ballRadius);
    ball.SetLaunched(false);
    
    paddle = Paddle(screenWidth/2 - paddleWidth/2, screenHeight - 50, paddleWidth, paddleHeight);
    
    // 清除道具和效果
    powerUps.clear();
    activeEffects.clear();
    extraBalls.clear();
    particles.clear();
    
    TraceLog(LOG_INFO, "Loaded Level %d: %s (%s)", level, cfg.levelName.c_str(), cfg.difficulty.c_str());
    TraceLog(LOG_INFO, "  Rows: %d, Cols: %d, Lives: %d, Score Multiplier: %d", 
             brickRows, brickCols, initialLives, cfg.scoreMultiplier);
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
                ChangeState(GameState::LEVEL_SELECT);
            }
            if (IsKeyPressed(KEY_L)) {
                ChangeState(GameState::LEADERBOARD);
            }

            if (IsKeyPressed(KEY_L)) {
                if (asyncLoader) {
                    asyncLoader->ForceRestart();  // 强制重置
                    asyncLoader->StartLoadTexture("demo_texture.png");  // 直接开始加载
                    TraceLog(LOG_INFO, "L key pressed - starting async load");
                }
            }

            break;
        
        case GameState::LEVEL_SELECT:  // 新增关卡选择处理
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
        case GameState::LEVEL_SELECT:  // 新增
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

void Game::UpdateNetwork() {
    if (!netHost) return;
    
    ENetEvent event;
    float now = GetTime();
    
    // 处理网络事件
    while (enet_host_service(netHost, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                TraceLog(LOG_INFO, "Client connected to server!");
                isConnected = true;
                if (!isHost) {
                    netPeer = event.peer;
                }
                // 重置网络状态
                lastRecvTime = now;
                networkReceiveTimeout = 0;
                break;
                
            case ENET_EVENT_TYPE_RECEIVE:
                if (isHost) {
                    // 主机：接收客户端球拍位置
                    if (event.packet->dataLength == sizeof(float)) {
                        float clientPaddleX;
                        memcpy(&clientPaddleX, event.packet->data, sizeof(float));
                        opponentPaddleX = clientPaddleX;
                        TraceLog(LOG_DEBUG, "Received opponent paddle: %.1f", opponentPaddleX);
                    }
                } else {
                    // 客户端：接收游戏状态
                    if (event.packet->dataLength == sizeof(NetworkGameState)) {
                        // 保存当前状态作为插值起点
                        netCurrentState = netTargetState;
                        lastStateTime = lastRecvTime;
                        
                        // 读取新状态
                        memcpy(&netTargetState, event.packet->data, sizeof(NetworkGameState));
                        nextStateTime = now;
                        lastRecvTime = now;
                        networkReceiveTimeout = 0;
                        
                        TraceLog(LOG_DEBUG, "Received game state - Ball: (%.1f, %.1f), Speed: (%.1f, %.1f)", 
                                 netTargetState.ballX, netTargetState.ballY,
                                 netTargetState.ballSpeedX, netTargetState.ballSpeedY);
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
    
    // 检查连接超时（5秒无数据）
    if (isConnected && (now - lastRecvTime > 5.0f)) {
        TraceLog(LOG_WARNING, "Network timeout - connection lost!");
        isConnected = false;
    }
    
    // ----- 主机发送游戏状态 -----
    if (isHost && isConnected && netPeer) {
        if (ball.IsLaunched()) {
            networkSendTimer += GetFrameTime();
            if (networkSendTimer >= 1.0f / 30.0f) {  // 每秒30次
                SendGameStateToClient();
                networkSendTimer = 0;
            }
        }
    }
    
    // ----- 客户端发送球拍位置 -----
    if (!isHost && isConnected && netPeer) {
        networkSendTimer += GetFrameTime();
        if (networkSendTimer >= 1.0f / 30.0f) {
            float myPaddleX = paddle.GetRect().x;
            ENetPacket* packet = enet_packet_create(&myPaddleX, sizeof(myPaddleX), 
                                                     ENET_PACKET_FLAG_UNSEQUENCED);
            enet_peer_send(netPeer, 0, packet);
            networkSendTimer = 0;
            TraceLog(LOG_DEBUG, "Sent paddle position: %.1f", myPaddleX);
        }
    }
    
    // ----- 客户端插值更新 -----
    if (!isHost && isConnected && lastRecvTime > 0) {
        double nowTime = GetTime();
        if (nextStateTime > lastStateTime) {
            interpolationAlpha = (nowTime - lastStateTime) / (nextStateTime - lastStateTime);
            interpolationAlpha = std::clamp(interpolationAlpha, 0.0f, 1.0f);
        } else {
            interpolationAlpha = 1.0f;
        }
        
        // 线性插值球位置
        interpolatedBallX = netCurrentState.ballX * (1 - interpolationAlpha) + 
                            netTargetState.ballX * interpolationAlpha;
        interpolatedBallY = netCurrentState.ballY * (1 - interpolationAlpha) + 
                            netTargetState.ballY * interpolationAlpha;
        
        // 更新对手分数
        opponentScore = netTargetState.score2;
    }
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
    LoadLevel(selectedLevel);  // 使用选中的关卡
    ChangeState(GameState::PLAYING);
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
// ===== 新增：网络更新（放在最前面） =====
    UpdateNetwork();

    HandleInput();
    paddle.Update(GetFrameTime());

    // 任务3: 使用互斥锁保护共享数据（已在 AsyncResourceLoader 内部实现）
    UpdateAsyncLoading();
    
    if (currentState == GameState::PLAYING) {
        if (isHost || !isConnected) {
            UpdateGame();
        }

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
            DrawAsyncLoadingUI();
            break;
        
        case GameState::LEVEL_SELECT:  // 新增
            DrawLevelSelect();
            break;    

        case GameState::PLAYING:
        // 客户端插值绘制
        if (!isHost && isConnected && ball.IsLaunched()) {
            // 使用插值位置绘制球
            DrawCircleV({interpolatedBallX, interpolatedBallY}, ball.GetRadius(), RED);
        
            // 绘制对手球拍（使用接收到的主机球拍位置）
            if (netTargetState.paddle1X > 0) {
                Rectangle oppRect = { netTargetState.paddle1X, 
                                          paddle.GetRect().y, 
                                          paddle.GetRect().width, 
                                          paddle.GetRect().height };
                    DrawRectangleRec(oppRect, ColorAlpha(BLUE, 0.6f));
                    DrawRectangleLinesEx(oppRect, 2, DARKBLUE);
                    
                    // 显示对手分数
                    DrawText(TextFormat("Opponent: %d", netTargetState.score1), 
                                 screenWidth - 110, 12, 16, ColorAlpha(WHITE, 0.8f));
                }
        
                // 显示网络延迟指示器
                float latency = GetTime() - lastRecvTime;
                Color latencyColor = (latency < 0.1f) ? GREEN : (latency < 0.2f) ? YELLOW : RED;
                DrawCircle(screenWidth - 15, 15, 6, latencyColor);
        
                // 自己的球拍
                paddle.Draw();
            } else {
                // 单机模式或主机模式的正常绘制
                ball.Draw();
                paddle.Draw();
            }
            
            DrawExtraBalls();
            paddle.Draw();
            for (auto& brick : bricks) brick.Draw();
            for (auto& powerUp : powerUps) powerUp.Draw();  // 注意大小写
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

// ========== 网络状态同步实现 ==========

void Game::SendGameStateToClient() {
    if (!netPeer) return;
    
    NetworkGameState state;
    
    // 主机球的实际位置
    Vector2 ballPos = ball.GetPosition();
    Vector2 ballSpeed = ball.GetSpeed();
    
    state.ballX = ballPos.x;
    state.ballY = ballPos.y;
    state.ballSpeedX = ballSpeed.x;
    state.ballSpeedY = ballSpeed.y;
    state.paddle1X = paddle.GetRect().x;      // 主机的球拍
    state.paddle2X = opponentPaddleX;          // 客户端的球拍
    state.score1 = score;                      // 主机的分数
    state.score2 = opponentScore;              // 客户端的分数
    
    ENetPacket* packet = enet_packet_create(&state, sizeof(state), 
                                             ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(netPeer, 0, packet);
    
    TraceLog(LOG_DEBUG, "Sent game state - Ball: (%.1f, %.1f), Score: %d vs %d", 
             state.ballX, state.ballY, state.score1, state.score2);
}

void Game::ReceiveGameStateFromHost() {
    // 这个方法现在合并到 UpdateNetwork() 中
    // 保留接口以备需要
}

void Game::Shutdown() {
    SaveLeaderboard();

        // 释放异步加载器
    if (asyncLoader) {
        delete asyncLoader;
        asyncLoader = nullptr;
    }
    
    // 释放加载的纹理
    if (loadedDemoTexture.id != 0) {
        UnloadTexture(loadedDemoTexture);
    }

    // 释放网络资源
    if (netHost) {
        if (netPeer) {
            enet_peer_disconnect(netPeer, 0);
            // 等待断开完成
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

// ========== 任务实现 ==========

// 任务1: 按下 L 键时启动异步加载
void Game::RequestAsyncLoad(const std::string& texturePath) {
    if (!asyncLoader) return;
    
    // 如果正在加载，忽略新请求
    if (asyncLoader->IsLoading()) {
        TraceLog(LOG_INFO, "Already loading, ignoring new request");
        return;
    }
    
    // ===== 可选：如果已经加载完成，先重置 =====
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

// 更新异步加载状态
void Game::UpdateAsyncLoading() {
    if (!asyncLoader) return;
    
    Texture2D loadedTex;
    if (asyncLoader->TryGetLoadedTexture(loadedTex)) {
        if (loadedTex.id != 0) {
            // 卸载旧纹理（如果存在）
            if (loadedDemoTexture.id != 0) {
                // 注意：不要在这里卸载，因为可能还在使用
                // UnloadTexture(loadedDemoTexture);
            }
            loadedDemoTexture = loadedTex;
            showLoadedTexture = true;
            textureDisplayTimer = 3.0f;  // 显示3秒
            
            // 每次加载都用不同的颜色改变砖块
            static int colorIndex = 0;
            Color colors[] = {
                GREEN,      // 第1次：绿色
                RED,        // 第2次：红色
                BLUE,       // 第3次：蓝色
                YELLOW,     // 第4次：黄色
                ORANGE,     // 第5次：橙色
                PURPLE,     // 第6次：紫色
                SKYBLUE,    // 第7次：天蓝
                PINK        // 第8次：粉色
            };
            Color newColor = colors[colorIndex % 8];
            colorIndex++;
            
            // 改变所有存在的砖块颜色
            for (auto& brick : bricks) {
                if (brick.IsActive()) {
                    brick.SetColor(newColor);
                }
            }
            
            TraceLog(LOG_INFO, "Texture loaded successfully! Brick color changed (count: %d)", colorIndex);
            
            // 注意：这里不重置 asyncLoader 状态
            // 让用户下次按 L 时通过 ForceRestart 重置
        }
    }
    
    // 更新纹理显示计时器
    if (showLoadedTexture) {
        textureDisplayTimer -= GetFrameTime();
        if (textureDisplayTimer <= 0) {
            showLoadedTexture = false;
        }
    }
}

// 绘制异步加载 UI
void Game::DrawAsyncLoadingUI() {
    if (asyncLoader && asyncLoader->IsLoading()) {
        float progress = asyncLoader->GetProgress();
        
        // 半透明背景
        DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.7f));
        
        // 动态加载文字（带闪烁效果）
        float time = GetTime();
        int dotCount = ((int)(time * 2) % 4);
        std::string loadingText = "Loading";
        for (int i = 0; i < dotCount; i++) loadingText += ".";
        for (int i = dotCount; i < 3; i++) loadingText += " ";
        
        DrawText(loadingText.c_str(), screenWidth/2 - 60, screenHeight/2 - 60, 36, YELLOW);
        
        // 进度条
        int barWidth = 300;
        int barHeight = 20;
        int barX = screenWidth/2 - barWidth/2;
        int barY = screenHeight/2 - 10;
        
        DrawRectangle(barX, barY, barWidth, barHeight, DARKGRAY);
        DrawRectangle(barX, barY, (int)(barWidth * progress), barHeight, LIME);
        
        // 进度百分比
        DrawText(TextFormat("%d%%", (int)(progress * 100)), 
                 screenWidth/2 - 20, barY - 25, 20, WHITE);
        
        DrawText("Loading texture in background...", 
                 screenWidth/2 - 130, barY + 30, 16, GRAY);
    }
    
    // 显示已加载的纹理
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
    
    // 标题
    DrawText("SELECT LEVEL", screenWidth/2 - 120, 60, 40, YELLOW);
    DrawText("Choose your difficulty", screenWidth/2 - 100, 110, 20, GRAY);
    
    // 关卡卡片位置
    int cardWidth = 200;
    int cardHeight = 250;
    int startCardX = (screenWidth - (cardWidth * 3 + 40)) / 2;
    int cardY = 160;
    
    // 关卡颜色
    Color colors[3] = { GREEN, ORANGE, RED };
    const char* difficulties[3] = { "EASY", "NORMAL", "HARD" };
    
    for (int i = 0; i < 3; i++) {
        int cardX = startCardX + i * (cardWidth + 20);
        const LevelConfig& level = levels[i];
        
        // 卡片背景
        Color cardColor = ColorAlpha(colors[i], 0.3f);
        if (selectedLevel == i + 1) {
            cardColor = ColorAlpha(colors[i], 0.6f);
            DrawRectangleLines(cardX - 4, cardY - 4, cardWidth + 8, cardHeight + 8, GOLD);
        }
        
        DrawRectangle(cardX, cardY, cardWidth, cardHeight, cardColor);
        DrawRectangleLines(cardX, cardY, cardWidth, cardHeight, colors[i]);
        
        // 关卡编号和名称
        DrawText(TextFormat("%d", i + 1), cardX + cardWidth/2 - 15, cardY + 20, 30, colors[i]);
        DrawText(level.levelName.c_str(), cardX + cardWidth/2 - MeasureText(level.levelName.c_str(), 18)/2, 
                 cardY + 60, 18, WHITE);
        
        // 难度
        DrawText(difficulties[i], cardX + cardWidth/2 - 30, cardY + 90, 16, colors[i]);
        
        // 参数显示
        DrawText(TextFormat("Ball Speed: x%.0f%%", level.ballSpeedMultiplier * 100), 
                 cardX + 15, cardY + 130, 12, GRAY);
        DrawText(TextFormat("Paddle Speed: x%.0f%%", level.paddleSpeedMultiplier * 100), 
                 cardX + 15, cardY + 155, 12, GRAY);
        DrawText(TextFormat("Lives: %d", level.maxLives), 
                 cardX + 15, cardY + 180, 12, GRAY);
        DrawText(TextFormat("Score: x%d", level.scoreMultiplier), 
                 cardX + 15, cardY + 205, 12, GRAY);
        
        // 预览图标
        DrawText(TextFormat("[%d] Press %d", i + 1, i + 1), 
                 cardX + cardWidth/2 - 45, cardY + cardHeight - 30, 14, colors[i]);
    }
    
    // 操作说明
    DrawText("Press 1, 2 or 3 to select level", screenWidth/2 - 140, screenHeight - 80, 16, SKYBLUE);
    DrawText("Press ESC to return to menu", screenWidth/2 - 110, screenHeight - 50, 14, GRAY);
}