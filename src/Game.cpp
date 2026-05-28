// Game.cpp
// 游戏主控制类实现文件
//
// 本文件实现了Game.h中声明的所有方法
// 包含游戏的核心逻辑：状态机、碰撞检测、道具系统、粒子特效、网络同步等
//
// 主要功能模块：
//   1. 构造函数与初始化 - 加载配置、初始化游戏对象
//   2. 状态机管理 - 菜单/关卡选择/游戏中/暂停/结束等状态切换
//   3. 输入处理 - 响应键盘输入
//   4. 碰撞检测 - 球与砖块、挡板、边界的碰撞
//   5. 道具系统 - 掉落、拾取、效果应用
//   6. 粒子特效 - 破碎效果和光晕效果
//   7. 网络同步 - 双人联机
//   8. 存档系统 - 保存/加载游戏进度
//   9. 性能优化 - 对象池粒子系统和空间划分网格

#include "Game.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <cmath>
#include <unordered_set> 

// ==================== 构造函数 ====================
// 初始化所有成员变量为默认值
// 注意：实际游戏初始化需要在Init()中完成
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
    // 初始化8种砖块颜色（按行循环使用）
    brickColors[0] = RED;
    brickColors[1] = ORANGE;
    brickColors[2] = YELLOW;
    brickColors[3] = GREEN;
    brickColors[4] = SKYBLUE;
    brickColors[5] = BLUE;
    brickColors[6] = PURPLE;
    brickColors[7] = PINK;
    
    // 清空排行榜数组
    memset(leaderboardEntries, 0, sizeof(leaderboardEntries));
    // 初始化演示纹理为空
    loadedDemoTexture = Texture2D{0, 0, 0, 0, 0};
    
    // 初始化粒子对象池（所有粒子标记为未激活）
    for (int i = 0; i < MAX_PARTICLES; i++) {
        pooledParticles[i].active = false;
    }
    
    // 初始化关卡配置
    InitLevels();
}

// ==================== 析构函数 ====================
// 释放资源（主要在Shutdown中完成）
Game::~Game() {
}

// ==================== 配置加载 ====================
// 从JSON配置文件加载游戏参数
// 为什么需要：将游戏参数与代码分离，便于调整平衡性而不需重新编译
void Game::LoadConfig(const std::string& path) {
    TraceLog(LOG_INFO, "Loading config from: %s", path.c_str());
    
    // TODO: 实际解析JSON并加载参数
    // 当前版本使用硬编码默认值，未来可扩展从config.json读取
    
    TraceLog(LOG_INFO, "Configuration loaded:");
    TraceLog(LOG_INFO, "  Ball radius: %.1f", ballRadius);
    TraceLog(LOG_INFO, "  Gravity: %.2f", gravity);
    TraceLog(LOG_INFO, "  Paddle speed: %.1f", paddleSpeed);
    TraceLog(LOG_INFO, "  Paddle boost speed: %.1f", paddleBoostSpeed);
    TraceLog(LOG_INFO, "  Initial lives: %d", initialLives);
    TraceLog(LOG_INFO, "  Score per brick: %d", scorePerBrick);
    TraceLog(LOG_INFO, "  PowerUp drop rate: %.2f", powerUpDropRate);
}

// ==================== 游戏初始化 ====================
// 必须在游戏循环前调用
// 执行：加载配置、初始化对象、加载排行榜、检查存档
void Game::Init() {
    LoadConfig("config.json");

    // 创建异步资源加载器
    asyncLoader = new AsyncResourceLoader(textureCache);
    
    // 创建小球和挡板对象
    ball = Ball({(float)screenWidth/2, (float)screenHeight/2}, {0, 0}, ballRadius);
    paddle = Paddle(screenWidth/2 - paddleWidth/2, screenHeight - 50, paddleWidth, paddleHeight);
    
    // 加载排行榜（从scores.txt文件）
    LoadLeaderboard();
    
    // 初始化演示纹理为空
    loadedDemoTexture = Texture2D{0, 0, 0, 0, 0};

    // 初始化随机数种子
    srand((unsigned int)time(nullptr));
    
    // 检查是否存在存档文件
    CheckForSaveFile();
    
    TraceLog(LOG_INFO, "Game initialized. Initial state: MENU");
}

// ==================== 网络初始化 ====================
// 初始化ENet网络库，支持双人联机
// 参数asHost: true=主机模式（创建房间），false=客户端模式（连接房间）
// 参数serverIP: 客户端模式下要连接的服务器IP地址
void Game::InitNetwork(bool asHost, const char* serverIP) {
    static bool enetInitialized = false;
    // 确保ENet只初始化一次
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
        // ========== 主机模式 ==========
        // 监听所有网络接口的12345端口
        ENetAddress address;
        enet_address_set_host(&address, "0.0.0.0");
        address.port = 12345;
        
        // 创建主机，最多允许1个客户端连接
        netHost = enet_host_create(&address, 1, 2, 0, 0);
        if (!netHost) {
            TraceLog(LOG_ERROR, "Failed to create ENet host!");
            return;
        }
        TraceLog(LOG_INFO, "Server started on port 12345, waiting for client...");
        isConnected = false;
    } else {
        // ========== 客户端模式 ==========
        // 创建客户端主机
        netHost = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!netHost) {
            TraceLog(LOG_ERROR, "Failed to create ENet client!");
            return;
        }
        
        // 设置服务器地址
        ENetAddress address;
        enet_address_set_host(&address, serverIP);
        address.port = 12345;
        
        // 连接到服务器
        netPeer = enet_host_connect(netHost, &address, 2, 0);
        if (!netPeer) {
            TraceLog(LOG_ERROR, "Failed to connect to server!");
            return;
        }
        TraceLog(LOG_INFO, "Connecting to server %s:12345...", serverIP);
        isConnected = false;
    }
}

// ==================== 砖块初始化 ====================
// 初始化标准矩形布局的砖块
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

// ==================== 关卡配置初始化 ====================
// 硬编码3个关卡的默认参数
void Game::InitLevels() {
    // 第1关：森林山谷（简单）
    levels[0] = {
        1, "Forest Valley", "Easy",
        0.8f, 1.0f, 5, 8, 1, 0.25f, 4, {}, 0
    };
    
    // 第2关：金字塔峰（普通）
    levels[1] = {
        2, "Pyramid Peak", "Normal",
        1.0f, 1.0f, 7, 10, 2, 0.35f, 3, {}, 1
    };
    
    // 第3关：黑暗城堡（困难）
    levels[2] = {
        3, "Dark Castle", "Hard",
        1.25f, 1.2f, 9, 12, 3, 0.45f, 2, {}, 4
    };
}

// ==================== 根据布局类型初始化砖块 ====================
// 支持5种不同的砖块布局，增加关卡多样性
// layoutType: 0=标准,1=菱形,2=金字塔,3=波浪,4=城堡
void Game::InitBricksByLayout(int layoutType) {
    bricks.clear();
    
    switch (layoutType) {
        case 0:  // 标准矩形布局
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
            
        case 1:  // 菱形布局：中间多两边少
            for (int row = 0; row < brickRows; row++) {
                Color rowColor = brickColors[row % 8];
                // 每行的砖块数量递减
                int bricksInRow = brickCols - row;
                // 计算偏移量使布局居中
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
            
        case 2:  // 金字塔布局：下宽上窄
            for (int row = 0; row < brickRows; row++) {
                Color rowColor = brickColors[row % 8];
                int bricksInRow;
                int offsetX;
                
                // 计算金字塔形状：先增后减
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
            
        case 3:  // 波浪布局：模拟波浪形状
            for (int row = 0; row < brickRows; row++) {
                Color rowColor = brickColors[row % 8];
                // 根据行和列的正弦值决定是否放置砖块
                int waveOffset = (int)(sin(row * 0.8f) * 3);
                
                for (int col = 0; col < brickCols; col++) {
                    int colWave = (int)(cos(col * 0.8f) * 2);
                    // 跳过特定位置的砖块形成波浪空洞
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
            
        case 4:  // 城堡布局：两侧有柱状结构
            for (int row = 0; row < brickRows; row++) {
                Color rowColor = brickColors[row % 8];
                for (int col = 0; col < brickCols; col++) {
                    bool shouldPlace = false;
                    
                    // 左侧柱子
                    if (col < 3 && row < 6) {
                        shouldPlace = true;
                    }
                    // 右侧柱子
                    else if (col >= brickCols - 3 && row < 6) {
                        shouldPlace = true;
                    }
                    // 顶部横梁
                    else if (col >= 3 && col < brickCols - 3 && row < 3) {
                        shouldPlace = true;
                    }
                    // 中间空洞（城门）
                    else if (col >= brickCols/2 - 2 && col <= brickCols/2 + 2 && row == 3) {
                        shouldPlace = false;
                    }
                    // 底部基础
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
            
        default:  // 默认标准布局
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

// ==================== 加载关卡 ====================
// 根据关卡编号应用对应的参数配置
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
    
    // 动态计算砖块宽度以适应屏幕
    float totalWidth = brickCols * brickWidth + (brickCols - 1) * spacing;
    if (totalWidth > screenWidth - 100) {
        brickWidth = (screenWidth - 100 - (brickCols - 1) * spacing) / brickCols;
    }
    // 水平居中砖块区域
    startX = (screenWidth - brickCols * brickWidth - (brickCols - 1) * spacing) / 2;
    
    // 应用难度相关的倍率
    ballSpeedMultiplier = cfg.ballSpeedMultiplier;
    paddleSpeed = 9.0f * cfg.paddleSpeedMultiplier;
    paddleBoostSpeed = 15.0f * cfg.paddleSpeedMultiplier;
    scorePerBrick = 10 * cfg.scoreMultiplier;
    powerUpDropRate = cfg.powerUpDropRate;
    
    // 根据布局类型创建砖块
    InitBricksByLayout(cfg.layoutType);
    
    // 重置小球和挡板
    ball = Ball({(float)screenWidth/2, (float)screenHeight/2}, {0, 0}, ballRadius);
    ball.SetLaunched(false);
    paddle = Paddle(screenWidth/2 - paddleWidth/2, screenHeight - 50, paddleWidth, paddleHeight);
    
    // 清空道具和效果
    powerUps.clear();
    activeEffects.clear();
    extraBalls.clear();
    
    // 清除粒子
    for (int i = 0; i < MAX_PARTICLES; i++) {
        pooledParticles[i].active = false;
    }
    activeParticleCount = 0;
    
    // 重置空间划分网格
    for (int x = 0; x < GRID_COLS; x++) {
        for (int y = 0; y < GRID_ROWS; y++) {
            grid[x][y].brickIndices.clear();
        }
    }
    BuildSpatialGrid();
    
    TraceLog(LOG_INFO, "Loaded Level %d: %s (%s)", level, cfg.levelName.c_str(), cfg.difficulty.c_str());
}

// ==================== 输入处理 ====================
// 根据当前游戏状态响应键盘输入
void Game::HandleInput() {
    // 全局按键：按R键重置游戏
    if (IsKeyPressed(KEY_R)) {
        ResetGame();
        ChangeState(GameState::PLAYING);
        return;
    }
    
    switch (currentState) {
        case GameState::MENU:
            // 菜单状态：按Enter或空格开始游戏
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (SaveExists()) {
                    // 有存档，询问是否继续（当前直接加载）
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
            // 按L键：如果有存档则加载，否则显示排行榜并触发异步加载
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
            // 关卡选择状态：按1/2/3选择关卡
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
            // ESC或Backspace返回菜单
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
                ChangeState(GameState::MENU);
            }
            break;    

        case GameState::PLAYING: {
            // 游戏中状态
            // P键暂停
            if (IsKeyPressed(KEY_P)) {
                ChangeState(GameState::PAUSED);
            }
            // L键显示排行榜
            if (IsKeyPressed(KEY_L)) {
                ChangeState(GameState::LEADERBOARD);
            }
            // G键切换空间划分优化（用于性能对比测试）
            if (IsKeyPressed(KEY_G)) {
                useSpatialPartition = !useSpatialPartition;
                TraceLog(LOG_INFO, "Spatial partition %s", 
                         useSpatialPartition ? "ENABLED" : "DISABLED");
            }
    
            // 挡板移动
            float speed = paddleSpeed;
            // Shift键加速
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                speed = paddleBoostSpeed;
            }
            if (IsKeyDown(KEY_LEFT)) {
                paddle.MoveLeft(speed);
            }
            if (IsKeyDown(KEY_RIGHT)) {
                paddle.MoveRight(speed);
            }
    
            // 空格键发射小球
            if (IsKeyPressed(KEY_SPACE) && !ball.IsLaunched()) {
                float paddleCenterX = paddle.GetRect().x + paddle.GetRect().width / 2;
                float paddleTopY = paddle.GetRect().y;
                ball.Launch(paddleCenterX, paddleTopY, paddle.GetRect().width);
                TraceLog(LOG_INFO, "Ball launched!");
            }

            // F5键手动保存游戏
            if (IsKeyPressed(KEY_F5)) {
                SaveGame("savegame.json");
                TraceLog(LOG_INFO, "Game manually saved!");
            }
            break;
            }
            
        case GameState::PAUSED:
            // 暂停状态：P键恢复
            if (IsKeyPressed(KEY_P)) {
                ChangeState(GameState::PLAYING);
            }
            // L键显示排行榜
            if (IsKeyPressed(KEY_L)) {
                ChangeState(GameState::LEADERBOARD);
            }
            break;
            
        case GameState::LEADERBOARD:
            // 排行榜状态：L键或ESC返回上一状态
            if (IsKeyPressed(KEY_L) || IsKeyPressed(KEY_ESCAPE)) {
                ChangeState(previousState);
            }
            break;
            
        case GameState::GAMEOVER:
        case GameState::VICTORY:
            // 游戏结束或胜利状态：按Enter或空格返回菜单
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                ChangeState(GameState::MENU);
            }
            break;
            
        default:
            break;
    }
}

// ==================== 状态切换 ====================
// 切换游戏状态，调用进出回调
void Game::ChangeState(GameState newState) {
    if (currentState == newState) return;
    
    OnExitState(currentState);
    previousState = currentState;
    TraceLog(LOG_INFO, "State transition: %d -> %d", (int)currentState, (int)newState);
    currentState = newState;
    OnEnterState(currentState);
}

// ==================== 进入状态回调 ====================
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
            // 检查是否可以进入排行榜
            if (CanEnterLeaderboard(score)) {
                playerRank = AddToLeaderboard("Player", score);
            }
            // 游戏结束时删除存档
            remove("savegame.json");
            break;
        case GameState::VICTORY:
            TraceLog(LOG_INFO, "Victory! Final score: %d", score);
            // 检查是否可以进入排行榜
            if (CanEnterLeaderboard(score)) {
                playerRank = AddToLeaderboard("Player", score);
            }
            // 胜利时删除存档
            remove("savegame.json");
            break;
        default:
            break;
    }
}

// ==================== 退出状态回调 ====================
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

// ==================== 游戏逻辑更新 ====================
// 仅在PLAYING状态调用，更新小球位置和碰撞
void Game::UpdateGame() {
    if (currentState != GameState::PLAYING) return;
    
    // 累积游戏时间（用于计算分数倍率）
    if (ball.IsLaunched()) {
        gameTime += GetFrameTime();
    }
    
    // 未发射时让小球跟随挡板
    if (!ball.IsLaunched()) {
        float paddleCenterX = paddle.GetRect().x + paddle.GetRect().width / 2;
        float paddleTopY = paddle.GetRect().y;
        ball.FollowPaddle(paddleCenterX, paddleTopY);
    }
    
    // 移动小球并应用物理
    ball.Move();
    ball.ApplyGravity();
    CheckCollisions();
    CheckWinCondition();
}

// ==================== 网络更新 ====================
// 处理ENet事件和数据收发
void Game::UpdateNetwork() {
    if (!netHost) return;
    
    ENetEvent event;
    float now = GetTime();
    
    // 处理所有待处理的网络事件（非阻塞）
    while (enet_host_service(netHost, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                // 连接成功
                TraceLog(LOG_INFO, "Client connected to server!");
                isConnected = true;
                if (!isHost) {
                    netPeer = event.peer;
                }
                lastRecvTime = now;
                networkReceiveTimeout = 0;
                break;
                
            case ENET_EVENT_TYPE_RECEIVE:
                // 收到数据包
                if (isHost) {
                    // 主机模式：收到客户端挡板位置（4字节float）
                    if (event.packet->dataLength == sizeof(float)) {
                        float clientPaddleX;
                        memcpy(&clientPaddleX, event.packet->data, sizeof(float));
                        opponentPaddleX = clientPaddleX;
                        TraceLog(LOG_DEBUG, "Received opponent paddle: %.1f", opponentPaddleX);
                    }
                } else {
                    // 客户端模式：收到游戏状态（NetworkGameState结构体）
                    if (event.packet->dataLength == sizeof(NetworkGameState)) {
                        // 将当前状态保存为上一状态（用于插值）
                        netCurrentState = netTargetState;
                        lastStateTime = lastRecvTime;
                        
                        // 读取新状态
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
                // 对方断开连接
                TraceLog(LOG_WARNING, "Peer disconnected!");
                isConnected = false;
                netPeer = nullptr;
                break;
                
            default:
                break;
        }
    }
    
    // 超时检测：5秒未收到数据判定连接断开
    if (isConnected && (now - lastRecvTime > 5.0f)) {
        TraceLog(LOG_WARNING, "Network timeout - connection lost!");
        isConnected = false;
    }
    
    // 主机端：定期发送游戏状态给客户端
    if (isHost && isConnected && netPeer) {
        if (ball.IsLaunched()) {
            networkSendTimer += GetFrameTime();
            if (networkSendTimer >= 1.0f / 30.0f) {  // 30fps发送频率
                SendGameStateToClient();
                networkSendTimer = 0;
            }
        }
    }
    
    // 客户端：定期发送本地挡板位置给主机
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
    
    // 客户端：计算插值位置（平滑显示对手小球）
    if (!isHost && isConnected && lastRecvTime > 0) {
        double nowTime = GetTime();
        if (nextStateTime > lastStateTime) {
            interpolationAlpha = (nowTime - lastStateTime) / (nextStateTime - lastStateTime);
            interpolationAlpha = std::clamp(interpolationAlpha, 0.0f, 1.0f);
        } else {
            interpolationAlpha = 1.0f;
        }
        
        // 线性插值：P = P0 * (1-alpha) + P1 * alpha
        interpolatedBallX = netCurrentState.ballX * (1 - interpolationAlpha) + 
                            netTargetState.ballX * interpolationAlpha;
        interpolatedBallY = netCurrentState.ballY * (1 - interpolationAlpha) + 
                            netTargetState.ballY * interpolationAlpha;
        
        opponentScore = netTargetState.score2;
    }
}

// ==================== 碰撞检测 ====================
// 核心碰撞检测逻辑
void Game::CheckCollisions() {
    if (!ball.IsLaunched()) return;
    
    double startTime = GetTime();
    
    // 1. 边界碰撞检测
    ball.BounceEdge(screenWidth, screenHeight);
    
    // 2. 底部碰撞（小球掉落）
    if (ball.GetPosition().y + ball.GetRadius() >= screenHeight) {
        lives--;
        score = std::max(0, score - deathPenalty);
        
        if (lives <= 0) {
            ChangeState(GameState::GAMEOVER);
        } else {
            // 重置小球到挡板
            float paddleCenterX = paddle.GetRect().x + paddle.GetRect().width / 2;
            float paddleTopY = paddle.GetRect().y;
            ball.ResetToPaddle(paddleCenterX, paddleTopY);
            ball.SetLaunched(false);
            ball.SetSpeed({0, 0});
        }
        return;
    }
    
    // 3. 挡板碰撞检测
    if (CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), paddle.GetRect())) {
        if (ball.GetSpeed().y > 0) {
            ball.BouncePaddle(paddle.GetRect());
        }
    }
    
    // 4. 砖块碰撞检测
    std::vector<int> nearbyBrickIndices;
    
    if (useSpatialPartition) {
        // 使用空间划分优化：每3帧重建一次网格
        static int frameCounter = 0;
        if (++frameCounter >= 3) {
            BuildSpatialGrid();
            frameCounter = 0;
        }
        
        // 只检测小球附近的砖块
        GetNearbyBricks(ball, nearbyBrickIndices);
        
        for (int idx : nearbyBrickIndices) {
            if (idx >= 0 && idx < (int)bricks.size()) {
                auto& brick = bricks[idx];
                if (brick.IsActive() && ball.CheckBrickCollision(brick.GetRect())) {
                    brick.SetActive(false);
                    int addScore = (int)(scorePerBrick * CalculateMultiplier());
                    score += addScore;
                    
                    // 生成粒子特效
                    SpawnBrickParticles(brick.GetRect(), brick.GetColor());
                    
                    // 随机掉落道具
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
                    break;  // 一次只处理一个碰撞
                }
            }
        }
    } else {
        // 暴力检测：遍历所有砖块
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
    
    // 5. 额外小球的碰撞检测
    for (auto& b : extraBalls) {
        if (!b.IsLaunched()) continue;
        
        b.BounceEdge(screenWidth, screenHeight);
        
        // 挡板碰撞
        if (CheckCollisionCircleRec(b.GetPosition(), b.GetRadius(), paddle.GetRect())) {
            if (b.GetSpeed().y > 0) {
                b.BouncePaddle(paddle.GetRect());
            }
        }
        
        // 底部掉落检测
        if (b.GetPosition().y + b.GetRadius() >= screenHeight) {
            b.SetLaunched(false);
            continue;
        }
        
        // 砖块碰撞
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

// ==================== 胜利条件检查 ====================
// 检查是否所有砖块都被击碎
void Game::CheckWinCondition() {
    bool allBricksDestroyed = true;
    for (auto& brick : bricks) {
        if (brick.IsActive()) {
            allBricksDestroyed = false;
            break;
        }
    }
    
    if (allBricksDestroyed) {
        // 通关后自动保存并加载下一关
        if (currentLevel < 3) {
            currentLevel++;
            selectedLevel = currentLevel;
            SaveGame("savegame.json");  // 保存进度
            LoadLevel(currentLevel);
            TraceLog(LOG_INFO, "Level %d completed! Loading level %d", currentLevel - 1, currentLevel);
            // 重置小球状态
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

// ==================== 重置游戏 ====================
// 重新开始当前关卡
void Game::ResetGame() {
    LoadLevel(selectedLevel);
    ChangeState(GameState::PLAYING);
    remove("savegame.json");
}

// ==================== 计算分数倍率 ====================
// 倍率随时间递减，鼓励快速通关
// 公式：3.0 - 游戏时间 * 0.03，最小1.0
float Game::CalculateMultiplier() {
    float multiplier = 3.0f - gameTime * 0.03f;
    if (multiplier < 1.0f) multiplier = 1.0f;
    return multiplier;
}

// ==================== 道具系统 ====================

// 生成道具掉落物
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

// 应用道具效果
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

// 检查道具与挡板的碰撞
void Game::CheckPowerUpCollisions() {
    for (auto& powerUp : powerUps) {
        if (!powerUp.IsActive()) continue;
        
        if (CheckCollisionRecs(powerUp.GetRect(), paddle.GetRect())) {
            ApplyPowerUpEffect(powerUp.GetType());
            powerUp.SetActive(false);
            SpawnPowerUpGlow(powerUp.GetRect().x, powerUp.GetRect().y, GOLD);
        }
    }
    
    // 移除超出屏幕的道具
    powerUps.erase(
        std::remove_if(powerUps.begin(), powerUps.end(),
            [this](const PowerUp& p) { 
                return !p.IsActive() || p.IsOffScreen(screenHeight); 
            }),
        powerUps.end()
    );
}

// 更新激活的道具效果
void Game::UpdateEffects(float dt) {
    for (auto& effect : activeEffects) {
        effect->Update(*this, dt);
    }
    
    // 移除过期的效果
    activeEffects.erase(
        std::remove_if(activeEffects.begin(), activeEffects.end(),
            [](const std::unique_ptr<PowerUpEffect>& e) { return e->IsExpired(); }),
        activeEffects.end()
    );
}

// 添加额外小球（多球道具效果）
void Game::AddExtraBalls(int count) {
    Vector2 mainPos = ball.GetPosition();
    Vector2 mainSpeed = ball.GetSpeed();
    
    for (int i = 0; i < count; i++) {
        // 计算偏移角度：45度、90度等
        float angleOffset = (i + 1) * 45.0f;
        float rad = angleOffset * 3.14159f / 180.0f;
        
        Ball newBall(mainPos, {0, 0}, ballRadius);
        newBall.SetLaunched(true);
        
        // 计算旋转后的速度向量
        float speedMagnitude = sqrt(mainSpeed.x * mainSpeed.x + mainSpeed.y * mainSpeed.y);
        if (speedMagnitude < 1.0f) speedMagnitude = 6.5f;
        
        float newSpeedX = mainSpeed.x * cosf(rad) - mainSpeed.y * sinf(rad);
        float newSpeedY = mainSpeed.x * sinf(rad) + mainSpeed.y * cosf(rad);
        
        newBall.SetSpeed({ newSpeedX, newSpeedY });
        
        extraBalls.push_back(newBall);
    }
}

// 减慢小球速度（减速道具效果）
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

// 恢复小球速度（减速效果结束时调用）
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

// 更新额外小球
void Game::UpdateExtraBalls(float /*dt*/) {
    for (auto& b : extraBalls) {
        b.Move();
        b.ApplyGravity();
        
        b.BounceEdge(screenWidth, screenHeight);
        
        // 挡板碰撞
        if (CheckCollisionCircleRec(b.GetPosition(), b.GetRadius(), paddle.GetRect())) {
            if (b.GetSpeed().y > 0) {
                b.BouncePaddle(paddle.GetRect());
            }
        }
        
        // 砖块碰撞
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
        
        // 底部掉落检测
        if (b.GetPosition().y + b.GetRadius() >= screenHeight) {
            b.SetLaunched(false);
        }
    }
    
    // 移除掉落的小球
    extraBalls.erase(
        std::remove_if(extraBalls.begin(), extraBalls.end(),
            [](const Ball& b) { return !b.IsLaunched(); }),
        extraBalls.end()
    );
}

// ==================== 绘制额外小球 ====================
void Game::DrawExtraBalls() {
    for (auto& b : extraBalls) {
        b.Draw();
    }
}

// ==================== UI绘制 ====================
void Game::DrawUI() {
    // 边框
    DrawRectangle(0, 0, screenWidth, 5, GRAY);
    DrawRectangle(0, 0, 5, screenHeight, GRAY);
    DrawRectangle(screenWidth-5, 0, 5, screenHeight, GRAY);
    
    // 分数和生命值
    DrawText(TextFormat("Score: %d", score), 15, 12, 20, WHITE);
    DrawText(TextFormat("Lives: %d", lives), screenWidth - 110, 12, 20, lives > 1 ? GREEN : RED);
    
    // 分数倍率
    float multiplier = CalculateMultiplier();
    DrawText(TextFormat("Time: %.1f", gameTime), 15, 38, 16, Fade(WHITE, 0.7f));
    DrawText(TextFormat("x%.1f", multiplier), 120, 38, 16, multiplier > 1.5f ? GREEN : YELLOW);
    
    // 激活的道具效果显示
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
    
    // 发射提示
    if (!ball.IsLaunched() && currentState == GameState::PLAYING) {
        DrawText("Press SPACE to launch!", screenWidth/2 - 110, screenHeight - 60, 15, YELLOW);
    }
    
    // 操作提示
    DrawText("L/R arrows | Shift+Arrow=BOOST | P=Pause | R=Restart | L=Leaderboard | G=Grid", 
             screenWidth/2 - 380, screenHeight - 30, 13, GRAY);
    
    // 性能数据显示
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

// ==================== 菜单绘制 ====================
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

// ==================== 排行榜绘制 ====================
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

// ==================== 暂停界面绘制 ====================
void Game::DrawPaused() {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.7f));
    DrawText("PAUSED", screenWidth/2 - 60, screenHeight/2 - 20, 40, YELLOW);
    DrawText("Press P to Resume", screenWidth/2 - 100, screenHeight/2 + 30, 20, WHITE);
}

// ==================== 游戏结束界面绘制 ====================
void Game::DrawGameOver() {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.7f));
    DrawText("GAME OVER!", screenWidth/2 - 100, screenHeight/2 - 50, 50, RED);
    DrawText(TextFormat("Final Score: %d", score), screenWidth/2 - 100, screenHeight/2 + 10, 30, YELLOW);
    
    if (playerRank > 0) {
        DrawText(TextFormat("Rank #%d on Leaderboard!", playerRank), screenWidth/2 - 130, screenHeight/2 + 50, 20, GOLD);
    }
    
    DrawText("Press ENTER to Return to Menu", screenWidth/2 - 150, screenHeight/2 + 120, 20, WHITE);
}

// ==================== 胜利界面绘制 ====================
void Game::DrawVictory() {
    DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.7f));
    DrawText("VICTORY!", screenWidth/2 - 80, screenHeight/2 - 50, 50, GREEN);
    DrawText(TextFormat("Final Score: %d", score), screenWidth/2 - 100, screenHeight/2 + 10, 30, YELLOW);
    
    if (playerRank > 0) {
        DrawText(TextFormat("Rank #%d on Leaderboard!", playerRank), screenWidth/2 - 130, screenHeight/2 + 50, 20, GOLD);
    }
    
    DrawText("Press ENTER to Return to Menu", screenWidth/2 - 150, screenHeight/2 + 120, 20, WHITE);
}

// ==================== 主更新循环 ====================
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

// ==================== 主绘制循环 ====================
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
                // 网络模式：绘制插值后的对手小球和挡板
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
                // 延迟显示（网络质量指示器）
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

// ==================== 排行榜加载 ====================
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

// ==================== 排行榜保存 ====================
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

// ==================== 检查是否可以进入排行榜 ====================
bool Game::CanEnterLeaderboard(int score) {
    return leaderboardCount < 10 || score > leaderboardEntries[leaderboardCount - 1].score;
}

// ==================== 添加到排行榜 ====================
int Game::AddToLeaderboard(const char* name, int score) {
    if (!CanEnterLeaderboard(score)) return 0;
    
    ScoreEntry newEntry;
    strncpy(newEntry.name, name, 31);
    newEntry.name[31] = '\0';
    newEntry.score = score;
    newEntry.timestamp = time(nullptr);
    
    // 找到插入位置
    int pos = 0;
    while (pos < leaderboardCount && leaderboardEntries[pos].score >= score) pos++;
    
    if (leaderboardCount < 10) leaderboardCount++;
    
    // 移动后续元素
    for (int i = leaderboardCount - 1; i > pos; i--) {
        leaderboardEntries[i] = leaderboardEntries[i - 1];
    }
    
    leaderboardEntries[pos] = newEntry;
    SaveLeaderboard();
    
    return pos + 1;
}

// ==================== 发送游戏状态给客户端 ====================
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

// ==================== 从主机接收游戏状态 ====================
// 实际逻辑已合并到UpdateNetwork()中
void Game::ReceiveGameStateFromHost() {
    // 合并到 UpdateNetwork() 中
}

// ==================== 游戏关闭 ====================
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

// ==================== 异步加载请求 ====================
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

// ==================== 检查是否正在异步加载 ====================
bool Game::IsAsyncLoading() const {
    return asyncLoader ? asyncLoader->IsLoading() : false;
}

// ==================== 获取异步加载进度 ====================
float Game::GetAsyncLoadProgress() const {
    return asyncLoader ? asyncLoader->GetProgress() : 0.0f;
}

// ==================== 更新异步加载状态 ====================
void Game::UpdateAsyncLoading() {
    if (!asyncLoader) return;
    
    Texture2D loadedTex;
    if (asyncLoader->TryGetLoadedTexture(loadedTex)) {
        if (loadedTex.id != 0) {
            loadedDemoTexture = loadedTex;
            showLoadedTexture = true;
            textureDisplayTimer = 3.0f;
            
            // 改变砖块颜色作为加载完成的视觉效果
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

// ==================== 绘制异步加载UI ====================
void Game::DrawAsyncLoadingUI() {
    if (asyncLoader && asyncLoader->IsLoading()) {
        float progress = asyncLoader->GetProgress();
        
        DrawRectangle(0, 0, screenWidth, screenHeight, ColorAlpha(BLACK, 0.7f));
        
        // 动态加载文字动画
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

// ==================== 关卡选择界面绘制 ====================
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
        
        // 卡片背景
        Color cardColor = ColorAlpha(colors[i], 0.3f);
        if (selectedLevel == i + 1) {
            cardColor = ColorAlpha(colors[i], 0.6f);
            DrawRectangleLines(cardX - 4, cardY - 4, cardWidth + 8, cardHeight + 8, GOLD);
        }
        
        DrawRectangle(cardX, cardY, cardWidth, cardHeight, cardColor);
        DrawRectangleLines(cardX, cardY, cardWidth, cardHeight, colors[i]);
        
        // 关卡编号
        DrawText(TextFormat("%d", i + 1), cardX + cardWidth/2 - 15, cardY + 20, 30, colors[i]);
        // 关卡名称
        DrawText(level.levelName.c_str(), cardX + cardWidth/2 - MeasureText(level.levelName.c_str(), 18)/2, 
                 cardY + 60, 18, WHITE);
        // 难度标签
        DrawText(difficulties[i], cardX + cardWidth/2 - 30, cardY + 90, 16, colors[i]);
        
        // 关卡参数
        DrawText(TextFormat("Ball Speed: x%.0f%%", level.ballSpeedMultiplier * 100), 
                 cardX + 15, cardY + 130, 12, GRAY);
        DrawText(TextFormat("Paddle Speed: x%.0f%%", level.paddleSpeedMultiplier * 100), 
                 cardX + 15, cardY + 155, 12, GRAY);
        DrawText(TextFormat("Lives: %d", level.maxLives), 
                 cardX + 15, cardY + 180, 12, GRAY);
        DrawText(TextFormat("Score: x%d", level.scoreMultiplier), 
                 cardX + 15, cardY + 205, 12, GRAY);
        
        // 选择提示
        DrawText(TextFormat("[%d] Press %d", i + 1, i + 1), 
                 cardX + cardWidth/2 - 45, cardY + cardHeight - 30, 14, colors[i]);
    }
    
    DrawText("Press 1, 2 or 3 to select level", screenWidth/2 - 140, screenHeight - 80, 16, SKYBLUE);
    DrawText("Press ESC to return to menu", screenWidth/2 - 110, screenHeight - 50, 14, GRAY);
}

// ==================== 构建空间划分网格 ====================
void Game::BuildSpatialGrid() {
    // 清空所有网格
    for (int x = 0; x < GRID_COLS; x++) {
        for (int y = 0; y < GRID_ROWS; y++) {
            grid[x][y].brickIndices.clear();
        }
    }
    
    // 将每个活跃砖块添加到其覆盖的网格中
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

// ==================== 获取小球附近的砖块 ====================
void Game::GetNearbyBricks(const Ball& ball, std::vector<int>& outIndices) {
    outIndices.clear();
    Vector2 pos = ball.GetPosition();
    float radius = ball.GetRadius();
    
    // 计算小球覆盖的网格范围
    int startCol = std::max(0, (int)((pos.x - radius) / CELL_WIDTH));
    int endCol = std::min(GRID_COLS - 1, (int)((pos.x + radius) / CELL_WIDTH));
    int startRow = std::max(0, (int)((pos.y - radius) / CELL_HEIGHT));
    int endRow = std::min(GRID_ROWS - 1, (int)((pos.y + radius) / CELL_HEIGHT));
    
    // 使用unordered_set去重
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

// ==================== 粒子系统（对象池版本） ====================

// 生成一个粒子（使用对象池）
void Game::SpawnParticlePooled(Vector2 pos, Vector2 vel, Color color, float lifetime) {
    // 查找第一个空闲粒子
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
    
    // 对象池已满：循环覆盖策略（覆盖最旧的粒子）
    // 这种情况很少发生，但需要处理
    static int fallbackIndex = 0;
    fallbackIndex = (fallbackIndex + 1) % MAX_PARTICLES;
    pooledParticles[fallbackIndex].active = true;
    pooledParticles[fallbackIndex].position = pos;
    pooledParticles[fallbackIndex].velocity = vel;
    pooledParticles[fallbackIndex].color = color;
    pooledParticles[fallbackIndex].life = lifetime;
    pooledParticles[fallbackIndex].maxLife = lifetime;
}

// 更新所有粒子
void Game::UpdateParticlesPooled(float dt) {
    activeParticleCount = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!pooledParticles[i].active) continue;
        
        // 更新位置
        pooledParticles[i].position.x += pooledParticles[i].velocity.x * dt * 60;
        pooledParticles[i].position.y += pooledParticles[i].velocity.y * dt * 60;
        // 应用重力
        pooledParticles[i].velocity.y += 200.0f * dt;
        // 减少生命
        pooledParticles[i].life -= dt;
        
        if (pooledParticles[i].life <= 0) {
            pooledParticles[i].active = false;
        } else {
            activeParticleCount++;
        }
    }
}

// 绘制所有激活的粒子
void Game::DrawParticlesPooled() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!pooledParticles[i].active) continue;
        
        // 根据剩余生命比例计算透明度（淡出效果）
        float alpha = pooledParticles[i].life / pooledParticles[i].maxLife;
        DrawCircleV(pooledParticles[i].position, 3, 
                    ColorAlpha(pooledParticles[i].color, alpha));
    }
}

// 砖块破碎时生成粒子特效
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

// 道具掉落时生成光晕特效
void Game::SpawnPowerUpGlow(float x, float y, Color color) {
    for (int i = 0; i < 8; i++) {
        Vector2 vel = { 
            ((rand() % 100) - 50) / 10.0f,
            ((rand() % 100) - 50) / 10.0f
        };
        SpawnParticlePooled({x, y}, vel, color, 0.3f);
    }
}

// ==================== 存档系统 ====================

// 检查存档文件是否存在
bool Game::SaveExists() const {
    std::ifstream file("savegame.json");
    return file.good();
}

// 从JSON文件加载数据
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

// 根据JSON配置初始化砖块
void Game::InitBricksFromJSON(const json& config) {
    bricks.clear();
    
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

// 保存游戏进度
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
    
    // 保存活动道具效果（简化版）
    json effectsArray = json::array();
    for (const auto& effect : activeEffects) {
        json effectJson;
        effectJson["type"] = "effect";
        effectsArray.push_back(effectJson);
    }
    saveData["active_effects"] = effectsArray;
    
    // 保存剩余砖块数量（简化版）
    int remainingBricks = 0;
    for (const auto& brick : bricks) {
        if (brick.IsActive()) remainingBricks++;
    }
    saveData["remaining_bricks_count"] = remainingBricks;
    
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

// 加载游戏进度
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

// 检查存档并提示
void Game::CheckForSaveFile() {
    if (SaveExists()) {
        TraceLog(LOG_INFO, "Save file detected! Press L to continue or any key for new game");
    }
}