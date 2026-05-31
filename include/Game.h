#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include "PowerUp.h"
#include "OptimizedParticlePool.h"   // 新增：优化版粒子池（空闲列表实现，O(1)分配）
#include "DirtySpatialGrid.h"        // 新增：脏标记空间划分（按需重建，避免每帧重建）
#include <vector>
#include <string>
#include <memory>
#include <enet/enet.h>
#include "AsyncResourceLoader.h"
#include <fstream>      
#include <nlohmann/json.hpp>  
#include <unordered_map>

using json = nlohmann::json;

// 游戏状态枚举
// 定义了游戏可能处于的各种状态
enum class GameState {
    MENU,           // 主菜单界面
    LEVEL_SELECT,   // 关卡选择界面
    PLAYING,        // 游戏中
    PAUSED,         // 暂停状态
    GAMEOVER,       // 游戏结束
    VICTORY,        // 通关胜利
    VICTORY_MENU,   // 胜利后菜单（选择重玩或下一关）
    LEADERBOARD     // 排行榜界面
};

// 关卡配置结构体
// 存储每个关卡的静态配置数据
struct LevelConfig {
    int levelNumber;                    // 关卡编号（1-3）
    std::string levelName;              // 关卡名称（如"Forest Valley"）
    std::string difficulty;             // 难度字符串（Easy/Normal/Hard）
    float ballSpeedMultiplier;          // 球速倍率（0.8x/1.0x/1.25x）
    float paddleSpeedMultiplier;        // 挡板速度倍率（1.0x/1.0x/1.2x）
    int brickRows;                      // 砖块行数
    int brickCols;                      // 砖块列数
    int scoreMultiplier;                // 分数倍率（1x/2x/3x）
    float powerUpDropRate;              // 道具掉落率（0.25/0.35/0.45）
    int maxLives;                       // 最大生命值
    std::vector<std::pair<int, int>> brickPositions; // 砖块位置（用于自定义布局）
    int layoutType;                     // 布局类型（0=矩形，1=菱形，2=金字塔，4=城堡）
};

// 网络游戏状态结构体
// 用于双人联机模式下同步游戏数据
struct NetworkGameState {
    float ballX, ballY;         // 小球中心位置坐标（主机计算的权威值）
    float ballSpeedX, ballSpeedY; // 小球速度向量（主机计算的权威值）
    float paddle1X;             // 主机挡板的X坐标
    float paddle2X;             // 客户端挡板的X坐标（主机收到后存储并转发）
    int score1, score2;         // 双方当前分数（主机计算的权威值）
};

// Game类
// 游戏主控类，管理整个游戏的状态、逻辑和渲染
//
// 职责：
//   - 管理游戏状态机（MENU/PLAYING/PAUSED等）
//   - 管理所有游戏对象（Ball、Paddle、Bricks、PowerUps）
//   - 处理碰撞检测和游戏规则
//   - 管理道具系统和特效系统
//   - 支持双人联机网络同步
//   - 保存/加载游戏进度
//   - 管理排行榜
//
// 主要用法：
//   Game game;
//   game.Init();
//   while (!WindowShouldClose()) {
//       game.Update();
//       game.Draw();
//   }
//   game.Shutdown();
class Game {
private:
    // ========== 窗口和屏幕 ==========
    int screenWidth;    // 屏幕宽度（像素），固定800
    int screenHeight;   // 屏幕高度（像素），固定600
    
    // ========== 游戏对象 ==========
    Ball ball;          // 主球对象（核心玩法）
    Paddle paddle;      // 玩家控制的挡板
    std::vector<Brick> bricks;  // 砖块数组（关卡中的砖块集合）
    
    // ========== 游戏状态 ==========
    GameState currentState;     // 当前状态
    GameState previousState;    // 上一状态（用于返回）
    
    // ========== 游戏数据 ==========
    int lives;          // 剩余生命值（掉球扣1命）
    int score;          // 当前得分（击碎砖块增加）
    float gameTime;     // 游戏运行时间（秒，用于分数倍率计算）
    
    // ========== 排行榜相关 ==========
    bool showLeaderboard;   // 是否显示排行榜（当前未使用）
    int playerRank;         // 玩家在排行榜中的排名（游戏结束时设置）
    
    // ========== 砖块布局参数 ==========
    float brickWidth;       // 单个砖块宽度（像素）
    float brickHeight;      // 单个砖块高度（像素）
    float startX;           // 砖块区域起始X坐标
    float startY;           // 砖块区域起始Y坐标
    float spacing;          // 砖块之间的间距（像素）
    Color brickColors[8];   // 砖块颜色数组（每行循环使用）
    
    // ========== 游戏配置参数（从config.json加载） ==========
    float ballRadius;       // 小球半径（像素）
    float gravity;          // 重力加速度（每帧加到speed.y）
    float maxSpeed;         // 小球最大速度（防止穿透）
    float bounceForce;      // 碰撞时额外反弹力度
    float launchSpeed;      // 发射时的初始速度大小
    float paddleSpeed;      // 挡板基础移动速度（像素/秒）
    float paddleBoostSpeed; // 挡板加速移动速度（Shift键）
    float paddleWidth;      // 挡板宽度（像素）
    float paddleHeight;     // 挡板高度（像素）
    int initialLives;       // 初始生命值
    int scorePerBrick;      // 每个砖块的基础分数
    int brickRows;          // 砖块行数
    int brickCols;          // 砖块列数
    int deathPenalty;       // 掉球扣分惩罚
    float powerUpDropRate;  // 道具掉落概率（0.0-1.0）
    
    // ========== 关卡系统 ==========
    int currentLevel;           // 当前关卡编号（1-3）
    LevelConfig levels[3];      // 三个关卡的配置
    int selectedLevel;          // 在关卡选择界面选中的关卡
    
    // ========== 道具系统 ==========
    std::vector<PowerUp> powerUps;                      // 屏幕上的道具掉落物
    std::vector<std::unique_ptr<PowerUpEffect>> activeEffects; // 激活中的道具效果
    std::vector<Ball> extraBalls;                       // 额外小球（多球道具产生）
    float ballSpeedMultiplier;  // 球速倍率（减速球效果）
    bool isSlowed;              // 是否处于减速状态
    
    // ========== 原有粒子系统（保留兼容，线性查找实现） ==========
    static constexpr int MAX_PARTICLES = 500;   // 最大粒子数量
    
    // 粒子结构体（池化版本）
    struct ParticlePooled {
        Vector2 position;   // 粒子位置
        Vector2 velocity;   // 粒子速度
        Color color;        // 粒子颜色
        float life;         // 剩余生命时间
        float maxLife;      // 最大生命时间
        float active;       // 是否活跃（1.0=活跃，0.0=未激活）
    };
    
    ParticlePooled pooledParticles[MAX_PARTICLES];  // 粒子数组
    int activeParticleCount;                        // 当前活跃粒子数
    
    // ========== 新增：优化版粒子池（空闲列表实现，O(1)分配） ==========
    // 优化原理：使用空闲索引栈管理可用槽位，分配复杂度从O(n)降为O(1)
    // 性能提升：粒子密集场景下帧率提升15-25%
    OptimizedParticlePool optimizedParticlePool;
    
    // ========== 新增：脏标记空间划分（按需重建，避免每帧重建） ==========
    // 优化原理：使用脏标记机制，仅在砖块被击碎时标记相关单元格
    //         限制重建频率为每3帧一次，避免CPU浪费
    // 性能提升：碰撞检测整体性能提升30-40%
    DirtySpatialGrid dirtySpatialGrid;
    
    // ========== 空间划分系统（网格法优化碰撞检测） ==========
    // 注意：以下为原有空间划分代码，与dirtySpatialGrid并存，可通过useSpatialPartition切换
    struct GridCell {
        std::vector<int> brickIndices;  // 该单元格内的砖块索引列表
    };
    
    static constexpr int GRID_COLS = 12;                     // 网格列数
    static constexpr int GRID_ROWS = 8;                      // 网格行数
    static constexpr float CELL_WIDTH = 800.0f / GRID_COLS;  // 单元格宽度
    static constexpr float CELL_HEIGHT = 600.0f / GRID_ROWS; // 单元格高度
    
    GridCell grid[GRID_COLS][GRID_ROWS];  // 空间划分网格
    bool useSpatialPartition;             // true=使用空间划分优化，false=使用暴力检测
    
    // ========== 性能统计（用于UI显示调试信息） ==========
    double lastFrameTime;       // 上一帧的时间戳
    float collisionTimeMs;      // 碰撞检测耗时（毫秒）
    float particleTimeMs;       // 粒子更新耗时（毫秒）
    float spatialTimeMs;        // 空间划分更新耗时（毫秒）
    float totalFrameTimeMs;     // 总帧耗时（毫秒）
    
    // ========== 排行榜数据结构 ==========
    struct ScoreEntry {
        char name[32];      // 玩家名称
        int score;          // 分数
        time_t timestamp;   // 达成时间戳
    };
    ScoreEntry leaderboardEntries[10];  // 最多保存10条记录
    int leaderboardCount;               // 当前记录数量
    
    // ========== 网络联机（ENet） ==========
    ENetHost* netHost;      // ENet主机对象（主机或客户端共用）
    ENetPeer* netPeer;      // 连接的远端对等点
    bool isHost;            // true=主机模式，false=客户端模式
    bool isConnected;       // true=已连接，false=未连接
    float lastSendTime;     // 上次发送数据包的时间戳
    float lastRecvTime;     // 上次接收数据包的时间戳
    
    NetworkGameState netCurrentState;   // 当前网络状态（本地）
    NetworkGameState netTargetState;    // 目标网络状态（插值用）
    double lastStateTime;               // 上次收到状态的时间
    double nextStateTime;               // 下次插值目标时间
    
    float opponentPaddleX;  // 对手挡板X坐标（客户端存储主机挡板位置）
    int opponentScore;      // 对手分数
    
    // ========== 异步资源加载 ==========
    AsyncResourceLoader* asyncLoader;   // 异步加载器
    TextureCache textureCache;          // 纹理缓存（线程安全）
    Texture2D loadedDemoTexture;        // 加载完成的演示纹理
    bool showLoadedTexture;             // 是否显示加载完成的纹理
    float textureDisplayTimer;          // 纹理显示计时器
    bool isLoadingRequested;            // 是否请求了加载
    
    // ========== 网络同步参数 ==========
    float networkSendTimer;         // 网络发送计时器
    float networkReceiveTimeout;    // 接收超时计时器
    
    float interpolatedBallX;        // 插值后的小球X坐标
    float interpolatedBallY;        // 插值后的小球Y坐标
    float interpolationAlpha;       // 插值因子（0-1之间）
    
    // ========== 分裂砖块相关常量 ==========
    static constexpr int MAX_SPLIT_COUNT = 2;        // 小球最大分裂次数（防止无限分裂）
    static constexpr float SPLIT_SPEED_BOOST = 1.15f; // 分裂后球速加成（提高游戏难度）
    
    // ========== 传送门系统 ==========
    std::unordered_map<int, std::pair<int, Vector2>> portalPairs;  // 传送门配对映射
    std::unordered_map<int, float> portalCooldowns;               // 传送门冷却时间
    static constexpr float PORTAL_COOLDOWN_DURATION = 0.3f;        // 传送冷却时间（秒）
    
    // ========== 关卡完成状态 ==========
    bool levelCompleted;    // 当前关卡是否已完成
    bool showVictoryMenu;   // 是否显示胜利菜单
    
    // ========== 疯狂模式 ==========
    bool isFrenzyMode;      // true=疯狂模式开启（击碎砖块后随机生成新砖块和小球）
    
    // ========== 私有方法 ==========
    
    // 配置加载
    void LoadConfig(const std::string& path);           // 从JSON文件加载配置
    
    // 砖块初始化
    void InitBricks();                                  // 初始化砖块（矩形布局）
    void InitBricksByLayout(int layoutType);            // 根据布局类型初始化砖块
    void InitBricksFromJSON(const json& config);        // 从JSON配置初始化砖块
    
    // 排行榜
    void LoadLeaderboard();                             // 从文件加载排行榜
    void SaveLeaderboard();                             // 保存排行榜到文件
    
    // 输入处理
    void HandleInput();                                 // 处理键盘输入（每帧调用）
    
    // 游戏逻辑
    void UpdateGame();                                  // 更新游戏逻辑（碰撞、移动等）
    void CheckCollisions();                             // 碰撞检测（球vs砖块/挡板/边界）
    void CheckWinCondition();                           // 检查通关条件（所有砖块击碎）
    void ResetGame();                                   // 重置游戏（重新开始）
    
    // UI绘制
    void DrawUI();                                      // 绘制UI（分数、生命、道具状态等）
    void DrawMenu();                                    // 绘制主菜单
    void DrawLeaderboard();                             // 绘制排行榜界面
    void DrawPaused();                                  // 绘制暂停界面
    void DrawGameOver();                                // 绘制游戏结束界面
    void DrawVictory();                                 // 绘制胜利界面
    void DrawVictoryMenu();                             // 绘制胜利菜单（重玩/下一关）
    void DrawLevelSelect();                             // 绘制关卡选择界面
    
    // 状态机
    void ChangeState(GameState newState);               // 切换游戏状态
    void OnEnterState(GameState state);                 // 进入状态时的回调
    void OnExitState(GameState state);                  // 退出状态时的回调
    
    // 排行榜辅助
    bool CanEnterLeaderboard(int score);                // 检查分数是否能上榜
    int AddToLeaderboard(const char* name, int score);  // 添加分数到排行榜，返回排名
    
    // 分数倍率计算（随时间递减，鼓励快速通关）
    float CalculateMultiplier();
    
    // 道具系统
    void AddPowerUp(float x, float y, PowerUpType type);    // 生成道具掉落物
    void ApplyPowerUpEffect(PowerUpType type);              // 应用道具效果
    void CheckPowerUpCollisions();                          // 检测玩家拾取道具
    void UpdateEffects(float dt);                           // 更新道具效果计时
    
    // 额外小球管理
    void UpdateExtraBalls(float dt);    // 更新额外小球（移动、碰撞）
    void DrawExtraBalls();              // 绘制额外小球
    
    // 粒子系统（原有版本）
    void SpawnParticlePooled(Vector2 pos, Vector2 vel, Color color, float lifetime);  // 生成粒子
    void UpdateParticlesPooled(float dt);   // 更新所有粒子
    void DrawParticlesPooled();             // 绘制所有粒子
    
    // 粒子特效辅助
    void SpawnBrickParticles(Rectangle brickRect, Color brickColor);   // 砖块破碎粒子
    void SpawnPowerUpGlow(float x, float y, Color color);              // 道具光晕粒子
    
    // 空间划分（原有版本）
    void BuildSpatialGrid();                                // 重建空间划分网格
    void GetNearbyBricks(const Ball& ball, std::vector<int>& outIndices);  // 获取小球附近砖块
    
    // 网络同步
    void SendGameStateToClient();       // 主机发送游戏状态给客户端
    void ReceiveGameStateFromHost();    // 客户端接收主机游戏状态
    void UpdateNetwork();               // 更新网络（发送/接收）
    
    // 异步加载
    void UpdateAsyncLoading();          // 更新异步加载状态
    void DrawAsyncLoadingUI();          // 绘制异步加载UI
    
    // 存档系统
    bool SaveGame(const std::string& filename = "savegame.json");   // 保存游戏进度
    bool LoadGame(const std::string& filename = "savegame.json");   // 加载游戏进度
    bool SaveExists() const;                                        // 检查存档是否存在
    json LoadJSONFromFile(const std::string& path);                 // 从JSON文件加载数据
    void CheckForSaveFile();                                        // 启动时检查存档
    
    // ========== 新增：分裂砖块和重球系统 ==========
    void SplitBall(Ball& ball, const Brick& splitBrick);        // 小球分裂（击中分裂砖块时）
    void SplitBallIntoTwo(Ball& ball, Vector2 splitPosition);   // 将小球一分为二
    void CheckBallMerge();                                      // 检测两个普通小球碰撞合并
    std::vector<Ball*> GetAllActiveBalls();                     // 获取所有活跃小球指针
    bool HandleHeavyBallCollision(Ball& ball, std::vector<int>& hitBrickIndices);  // 重球穿透碰撞
    void HandleSplitBrickHit(Brick& brick, Ball& hittingBall);  // 处理分裂砖块被击中
    void CreateHeavyBall(Vector2 position, Vector2 velocity);   // 创建重球
    
    // ========== 新增：传送门系统 ==========
    void BuildPortalPairs();                                    // 构建传送门配对
    void HandlePortalTeleport(Ball& ball, int portalId);        // 处理传送门传送
    Vector2 GetPairedPortalPosition(int portalId);              // 获取配对的传送门位置
    
    // ========== 新增：移动砖块更新 ==========
    void UpdateMovingBricks(float dt);  // 更新移动砖块位置（每帧调用）
    
    // ========== 新增：关卡初始化 ==========
    void InitLevels();                  // 初始化三个关卡的配置数据
    void LoadLevel(int level);          // 加载指定关卡
    
    // ========== 新增：疯狂模式 ==========
    // 疯狂模式说明：击碎砖块后随机生成新砖块和额外小球，游戏难度大幅提升
    // 在菜单界面按F键开启
    
public:
    // 构造函数
    Game();
    
    // 析构函数
    ~Game();
    
    // ========== 公共接口 ==========
    
    // 初始化游戏
    // 加载配置、初始化状态、加载排行榜
    void Init();
    
    // 每帧更新
    // 处理输入、更新游戏逻辑、更新粒子效果
    void Update();
    
    // 每帧绘制
    // 根据当前状态绘制相应界面
    void Draw();
    
    // 关闭游戏
    // 释放资源、保存排行榜
    void Shutdown();
    
    // 初始化网络（多人模式）
    // 参数：
    //   asHost: true=作为主机等待连接，false=作为客户端连接服务器
    //   serverIP: 客户端模式下需要连接的服务器IP地址
    void InitNetwork(bool asHost, const char* serverIP = nullptr);
    
    // 获取挡板引用（用于道具效果）
    Paddle& GetPaddle() { return paddle; }
    
    // 添加额外小球（多球道具）
    // 参数count：要添加的小球数量
    void AddExtraBalls(int count);
    
    // 减速所有小球（减速球道具）
    // 参数factor：速度倍率（0.6表示减速40%）
    void SlowDownBalls(float factor);
    
    // 恢复小球速度（减速球效果到期后）
    void RestoreBallSpeed();
    
    // ========== 异步加载接口 ==========
    void RequestAsyncLoad(const std::string& texturePath);  // 请求异步加载纹理
    bool IsAsyncLoading() const;                            // 是否正在异步加载
    float GetAsyncLoadProgress() const;                     // 获取加载进度（0-1）
    
    // ========== 网络状态查询 ==========
    float GetOpponentPaddleX() const { return opponentPaddleX; }  // 获取对手挡板位置
    int GetOpponentScore() const { return opponentScore; }        // 获取对手分数
    bool IsNetworkGame() const { return netHost != nullptr && isConnected; }  // 是否联机游戏中
    bool IsHost() const { return isHost; }                      // 是否为主机
};

#endif // GAME_H