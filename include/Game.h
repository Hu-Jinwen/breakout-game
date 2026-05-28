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
#include <fstream>      
#include <nlohmann/json.hpp>  

using json = nlohmann::json;  

// 游戏状态枚举
// 定义了游戏可能处于的所有状态，用于状态机管理
// 状态转换在 ChangeState() 函数中处理
enum class GameState {
    MENU,           // 主菜单界面
    LEVEL_SELECT,   // 关卡选择界面
    PLAYING,        // 游戏中
    PAUSED,         // 暂停状态
    GAMEOVER,       // 游戏结束
    VICTORY,        // 胜利通关
    LEADERBOARD     // 排行榜界面
};

// 关卡配置结构体
// 存储从 JSON 文件加载的关卡参数，用于动态调整游戏难度
// 每个关卡有独立的参数配置，支持不同难度级别
struct LevelConfig {
    int levelNumber;                    // 关卡编号 (1-3)
    std::string levelName;              // 关卡名称，如 "Forest Valley"
    std::string difficulty;             // 难度描述，如 "Easy"/"Normal"/"Hard"
    float ballSpeedMultiplier;          // 球速倍率，0.8x~1.25x
    float paddleSpeedMultiplier;        // 球拍速度倍率，1.0x~1.2x
    int brickRows;                      // 砖块行数
    int brickCols;                      // 砖块列数
    int scoreMultiplier;                // 分数倍率，1x~3x
    float powerUpDropRate;              // 道具掉落率，0.25~0.45
    int maxLives;                       // 初始生命值，2~4
    std::vector<std::pair<int, int>> brickPositions; // 自定义砖块位置（用于特殊布局）
    int layoutType;                     // 布局类型：0=标准,1=菱形,2=金字塔,3=波浪,4=城堡
};

// 网络同步结构体
// 用于双人联机模式下，在主机和客户端之间同步游戏状态
// 通过网络包传输，客户端收到后用于插值渲染，实现平滑移动
struct NetworkGameState {
    float ballX, ballY;         // 小球位置
    float ballSpeedX, ballSpeedY; // 小球速度
    float paddle1X;             // 玩家1（主机）的挡板X坐标
    float paddle2X;             // 玩家2（客户端）的挡板X坐标
    int score1, score2;         // 双方分数
};

// class Game
// 游戏主控制类，管理整个游戏的生命周期和核心逻辑
//
// 职责：
// - 管理游戏状态机（菜单、游戏中、暂停、结束等）
// - 处理用户输入和碰撞检测
// - 管理道具系统、粒子特效和排行榜
// - 支持单人和双人联机模式
// - 负责配置加载和存档管理
//
// 主要用法：
//   Game game;
//   game.Init();
//   while (!WindowShouldClose()) {
//       game.Update();
//       game.Draw();
//   }
//   game.Shutdown();
//
// 注意事项：
// - 必须在 Init() 之后才能调用 Update() 和 Draw()
// - 网络功能需要 ENet 库支持，若无则自动禁用多人模式
class Game {
private:
    // ========== 窗口参数 ==========
    int screenWidth;    // 屏幕宽度，默认800px
    int screenHeight;   // 屏幕高度，默认600px
    
    // ========== 游戏对象 ==========
    Ball ball;          // 主小球
    Paddle paddle;      // 玩家挡板
    std::vector<Brick> bricks;  // 砖块数组
    
    // ========== 游戏状态 ==========
    GameState currentState;     // 当前游戏状态
    GameState previousState;    // 上一个状态，用于按ESC返回上一界面
    
    int lives;          // 剩余生命值
    int score;          // 当前分数
    float gameTime;     // 游戏进行时间（秒），用于计算分数倍率，时间越长倍率越低
    
    bool showLeaderboard;   // 是否显示排行榜
    int playerRank;         // 玩家在排行榜中的排名，0表示未上榜
    
    // ========== 砖块参数（从config.json加载） ==========
    float brickWidth;       // 砖块宽度
    float brickHeight;      // 砖块高度
    float startX;           // 砖块区域起始X坐标
    float startY;           // 砖块区域起始Y坐标
    float spacing;          // 砖块间距
    Color brickColors[8];   // 8种砖块颜色，按行循环使用
    
    // ========== 游戏参数（从config.json加载） ==========
    float ballRadius;       // 小球半径
    float gravity;          // 重力加速度，影响小球的Y轴速度，模拟真实物理
    float maxSpeed;         // 小球最大速度限制，防止速度过快穿透物体
    float bounceForce;      // 碰撞时额外反弹力度，增加游戏变数
    float launchSpeed;      // 小球发射时的初始速度
    float paddleSpeed;      // 挡板移动速度
    float paddleBoostSpeed; // 挡板加速移动速度（按住Shift键时使用）
    float paddleWidth;      // 挡板宽度
    float paddleHeight;     // 挡板高度
    int initialLives;       // 初始生命值
    int scorePerBrick;      // 每击碎一个砖块的基础分数
    int brickRows;          // 砖块行数
    int brickCols;          // 砖块列数
    int deathPenalty;       // 小球掉落时的分数惩罚（扣分）
    float powerUpDropRate;  // 道具掉落率，0.35表示35%概率掉落
    
    // ========== 关卡系统 ==========
    int currentLevel;           // 当前关卡编号，有效值1-3
    LevelConfig levels[3];      // 3个关卡的配置数组
    int selectedLevel;          // 用户在关卡选择界面选择的关卡
    
    // ========== 道具系统 ==========
    std::vector<PowerUp> powerUps;                      // 当前屏幕上的掉落道具
    std::vector<std::unique_ptr<PowerUpEffect>> activeEffects; // 当前生效的道具效果（工厂模式管理）
    std::vector<Ball> extraBalls;                       // 多球道具生成的小球
    float ballSpeedMultiplier;  // 小球速度倍率，用于减速道具和恢复
    bool isSlowed;              // 小球是否处于减速状态，用于恢复时判断
    
    // ========== 性能优化：对象池粒子系统 ==========
    // 使用对象池替代vector动态分配，避免频繁内存分配导致的卡顿
    // 因为粒子每帧可能创建几十个，对象池可显著减少内存碎片
    static constexpr int MAX_PARTICLES = 500;  // 最大粒子数量（对象池大小）
    
    // 粒子结构体（对象池版本）
    // active标志标记是否在使用中，实现粒子复用
    struct ParticlePooled {
        Vector2 position;   // 粒子当前位置
        Vector2 velocity;   // 粒子速度（像素/秒）
        Color color;        // 粒子颜色
        float life;         // 剩余生命时间（秒），<=0时粒子消失
        float maxLife;      // 最大生命时间（秒），用于计算透明度渐变
        bool active;        // true=正在使用中，false=空闲可复用
    };
    
    ParticlePooled pooledParticles[MAX_PARTICLES];  // 粒子池，静态数组无动态分配
    int activeParticleCount;    // 当前激活的粒子数量，用于UI显示
    
    // ========== 性能优化：空间划分（网格法） ==========
    // 使用网格法减少碰撞检测次数：只检测小球所在网格及相邻网格的砖块
    // 将碰撞检测从O(n)优化到O(k)，n是砖块总数，k是相邻网格内的砖块数
    struct GridCell {
        std::vector<int> brickIndices;  // 该网格内的砖块索引
    };
    
    static constexpr int GRID_COLS = 12;                // 网格列数
    static constexpr int GRID_ROWS = 8;                 // 网格行数
    static constexpr float CELL_WIDTH = 800.0f / GRID_COLS;   // 单元格宽度 ≈ 66.7px
    static constexpr float CELL_HEIGHT = 600.0f / GRID_ROWS;  // 单元格高度 = 75px
    
    GridCell grid[GRID_COLS][GRID_ROWS];  // 二维网格数组
    bool useSpatialPartition;   // true=启用网格优化，false=使用暴力检测（按G键可切换对比性能）
    
    // ========== 性能测量 ==========
    double lastFrameTime;       // 上一帧的时间戳
    float collisionTimeMs;      // 碰撞检测耗时（毫秒），用于性能分析
    float particleTimeMs;       // 粒子系统耗时（毫秒），用于性能分析
    float totalFrameTimeMs;     // 总帧耗时（毫秒）

    // ========== 排行榜系统 ==========
    // 本地存储，最多10条记录，按分数降序排列
    // 数据保存在scores.txt文件中，程序启动时加载，退出时保存
    struct ScoreEntry {
        char name[32];          // 玩家名称（最多31字符）
        int score;              // 分数
        time_t timestamp;       // 达成时间戳，用于显示日期
    };
    ScoreEntry leaderboardEntries[10];  // 排行榜数组，最多10条
    int leaderboardCount;               // 当前排行榜条目数

    // ========== 网络相关 ==========
    // 基于ENet库实现的双人对战，主机创建房间，客户端连接
    ENetHost* netHost;      // ENet主机对象（服务器端监听连接）
    ENetPeer* netPeer;      // ENet对等节点（客户端连接或主机连接的客户端）
    bool isHost;            // true=作为主机（服务器），false=作为客户端
    bool isConnected;       // 网络是否已连接成功
    float lastSendTime;     // 上次发送网络包的时间，用于控制发送频率
    float lastRecvTime;     // 上次接收网络包的时间，用于超时检测
    
    // 状态同步与插值
    // 使用插值算法平滑显示对手动作，避免抖动
    NetworkGameState netCurrentState;   // 当前已知的状态（上一帧使用的状态）
    NetworkGameState netTargetState;    // 目标状态（最新收到的状态，用于插值）
    double lastStateTime;               // 上一个状态的时间戳
    double nextStateTime;               // 下一个状态的时间戳
    
    float opponentPaddleX;  // 对手挡板的X坐标（插值计算后的值）
    int opponentScore;      // 对手的分数

    // ========== 异步资源加载 ==========
    // 用于在后台线程加载纹理，避免阻塞主线程导致画面卡顿
    AsyncResourceLoader* asyncLoader;   // 异步加载器对象
    TextureCache textureCache;          // 纹理缓存，避免重复加载
    Texture2D loadedDemoTexture;        // 演示用纹理（加载完成后显示）
    bool showLoadedTexture;             // 是否显示加载完成的纹理
    float textureDisplayTimer;          // 纹理显示计时器，显示3秒后自动隐藏
    bool isLoadingRequested;            // 是否已请求异步加载，避免重复请求

    // ========== 网络同步定时器 ==========
    float networkSendTimer;         // 网络发送计时器，达到阈值时发送数据包
    float networkReceiveTimeout;    // 网络接收超时计时器，超时则标记连接断开
    
    // 客户端插值相关变量
    float interpolatedBallX;        // 插值后的小球X坐标，用于平滑显示
    float interpolatedBallY;        // 插值后的小球Y坐标
    float interpolationAlpha;       // 插值因子，0=上一个状态，1=目标状态，中间值线性插值

    // ========== 私有方法 ==========
    
    // 从JSON配置文件加载游戏参数
    // 为什么需要：将游戏参数与代码分离，便于调整平衡性而不需重新编译
    // 使用方法：在 Init() 中调用，传入 "config.json" 路径
    // 注意事项：如果文件不存在或解析失败，使用代码中的默认值
    void LoadConfig(const std::string& path);
    
    // 初始化砖块布局（标准矩形网格）
    // 根据brickRows、brickCols、startX、startY、spacing参数创建砖块
    // 砖块颜色按行循环使用brickColors数组
    void InitBricks();
    
    // 根据布局类型初始化砖块
    // 为什么需要：支持多种非矩形布局，增加关卡多样性
    // 布局类型说明：
    //   - layoutType=0: 标准矩形，所有位置都有砖块
    //   - layoutType=1: 菱形布局，中间多两边少
    //   - layoutType=2: 金字塔布局，下宽上窄
    //   - layoutType=3: 波浪布局，模拟波浪形状
    //   - layoutType=4: 城堡布局，两侧有柱状结构
    void InitBricksByLayout(int layoutType);
    
    // 从JSON文件加载排行榜数据
    // 从"scores.txt"读取存储的排行榜记录
    // 文件格式：每行"姓名 分数 时间戳"
    void LoadLeaderboard();
    
    // 保存排行榜数据到文件
    // 将leaderboardEntries写入"scores.txt"
    // 在添加新记录时自动调用
    void SaveLeaderboard();
    
    // 处理用户输入
    // 根据当前游戏状态，响应不同的按键操作
    // MENU状态：Enter开始游戏，L加载存档或排行榜
    // LEVEL_SELECT状态：1/2/3选择关卡，ESC返回菜单
    // PLAYING状态：方向键移动挡板，空格发射，P暂停，R重开，L排行榜，F5保存
    // PAUSED状态：P恢复游戏，L排行榜
    // 其他状态：Enter/空格返回菜单
    void HandleInput();
    
    // 更新游戏逻辑（仅在PLAYING状态调用）
    // 包含：小球移动、重力应用、碰撞检测、胜利条件检查
    // 每帧调用一次，dt通过GetFrameTime()获取
    void UpdateGame();
    
    // 执行碰撞检测
    // 检测并处理以下碰撞：
    //   1. 小球与屏幕边界（顶边和左右边反弹，底边扣血）
    //   2. 小球与挡板（根据击中位置计算反弹角度）
    //   3. 小球与砖块（击碎后增加分数，生成道具）
    //   4. 小球掉落底部（扣血或游戏结束）
    // 使用空间划分网格优化性能（如果useSpatialPartition = true）
    void CheckCollisions();
    
    // 检查胜利条件
    // 遍历所有砖块，如果没有活跃砖块则：
    //   - 如果还有下一关，自动保存并加载下一关
    //   - 如果是最后一关，进入VICTORY状态
    void CheckWinCondition();
    
    // 重置游戏
    // 重新加载当前关卡，清空分数、生命值、道具效果、额外小球
    // 按R键时调用
    void ResetGame();
    
    // 绘制UI界面
    // 包括：分数、生命值、游戏时间、分数倍率、
    // 激活的道具效果、FPS、碰撞检测耗时等性能数据
    void DrawUI();
    
    // 绘制主菜单界面
    // 显示游戏标题、操作说明和道具介绍
    void DrawMenu();
    
    // 绘制排行榜界面
    // 显示前10名记录，包含排名、姓名、分数、日期
    void DrawLeaderboard();
    
    // 绘制暂停界面
    // 半透明遮罩 + "PAUSED"文字 + 提示
    void DrawPaused();
    
    // 绘制游戏结束界面
    // 显示最终分数和排名
    void DrawGameOver();
    
    // 绘制胜利界面
    // 显示通关分数和排名
    void DrawVictory();
    
    // 游戏状态切换
    // 处理状态切换前的清理和切换后的初始化工作
    // 调用OnExitState退出旧状态，OnEnterState进入新状态
    void ChangeState(GameState newState);
    
    // 进入状态时的回调
    // 在ChangeState中调用，执行状态特定的初始化
    // 例如：进入PLAYING时重置计时器
    void OnEnterState(GameState state);
    
    // 退出状态时的回调
    // 在ChangeState中调用，执行状态特定的清理工作
    // 例如：退出PLAYING时保存当前进度
    void OnExitState(GameState state);
    
    // 判断分数是否可以进入排行榜
    // 为什么需要：避免不必要的排名计算
    // 判断逻辑：排行榜未满(小于10条) 或 分数大于最低分
    bool CanEnterLeaderboard(int score);
    
    // 将分数添加到排行榜
    // 插入到正确位置（按分数降序），保持排行榜有序
    // 返回值：排名位置（1-10），0表示未上榜
    int AddToLeaderboard(const char* name, int score);
    
    // 计算当前分数倍率
    // 倍率公式 = 3.0 - 游戏时间 * 0.03
    // 随时间递减，最小为1.0，鼓励玩家快速通关
    // 返回值范围：1.0 ~ 3.0
    float CalculateMultiplier();
    
    // ========== 道具系统私有方法 ==========
    
    // 在指定位置生成道具掉落物
    // 砖块被击碎时调用，根据powerUpDropRate随机决定是否掉落
    void AddPowerUp(float x, float y, PowerUpType type);
    
    // 应用道具效果
    // 根据道具类型创建对应的效果对象，添加到activeEffects列表
    // 使用工厂模式，每个效果类自己实现Apply逻辑
    void ApplyPowerUpEffect(PowerUpType type);
    
    // 检查道具与挡板的碰撞
    // 遍历所有道具，如果与挡板相交则应用效果并移除道具
    // 同时生成拾取特效粒子
    void CheckPowerUpCollisions();
    
    // 更新所有激活的道具效果
    // 每帧调用，更新效果的剩余时间，过期则移除
    void UpdateEffects(float dt);
    
    // 更新额外小球
    // 处理多球道具生成的小球的移动、碰撞和生命周期
    // 飞出底部的小球被标记为inactive并移除
    // 参数dt当前未使用，保留用于未来扩展（如逐帧物理）
    void UpdateExtraBalls(float dt);
    
    // ========== 粒子系统私有方法（对象池版本） ==========
    
    // 生成一个粒子（使用对象池）
    // 为什么使用对象池：避免频繁的动态内存分配和释放，
    // 粒子系统每帧可能创建几十个粒子，对象池可显著减少卡顿和内存碎片
    // 使用说明：
    //   - 从池中查找第一个inactive的粒子
    //   - 如果池满，采用循环覆盖策略（覆盖最旧的粒子）
    void SpawnParticlePooled(Vector2 pos, Vector2 vel, Color color, float lifetime);
    
    // 更新所有粒子
    // 更新粒子的位置、速度（应用重力）、生命周期
    // 生命耗尽的粒子标记为inactive以便复用
    void UpdateParticlesPooled(float dt);
    
    // 绘制所有激活的粒子
    // 根据剩余生命比例计算透明度，实现淡出效果
    void DrawParticlesPooled();
    
    // 砖块破碎时生成粒子特效
    // 在砖块中心周围生成12个彩色粒子，向外扩散
    void SpawnBrickParticles(Rectangle brickRect, Color brickColor);
    
    // 道具掉落时生成光晕特效
    // 生成8个围绕道具的闪光粒子，增强视觉效果
    void SpawnPowerUpGlow(float x, float y, Color color);
    
    // 绘制所有额外小球
    // 多球道具生成的小球需要单独绘制
    void DrawExtraBalls();
    
    // ========== 性能优化：空间划分方法 ==========
    
    // 构建空间划分网格
    // 为什么需要：将碰撞检测从O(n)优化到O(k)，
    // 其中n是砖块总数，k是相邻网格内的砖块数
    // 实现原理：
    //   1. 清空所有网格单元格的砖块索引
    //   2. 遍历每个活跃砖块，根据其矩形区域确定覆盖的网格
    //   3. 将砖块索引添加到所有覆盖的网格中
    // 注意事项：当砖块被击碎后需要重建网格（或定期重建）
    void BuildSpatialGrid();
    
    // 获取小球附近的砖块索引
    // 根据小球的包围盒（位置+半径）确定覆盖的网格区域
    // 收集这些网格中的所有砖块索引（使用unordered_set去重）
    void GetNearbyBricks(const Ball& ball, std::vector<int>& outIndices);
    
    // ========== 网络相关私有方法 ==========
    
    // 发送游戏状态给客户端
    // 主机端调用，将当前小球位置、挡板位置、分数打包发送给客户端
    // 发送频率约30fps，由networkSendTimer控制
    void SendGameStateToClient();
    
    // 从主机接收游戏状态
    // 客户端调用，接收主机发送的状态包并更新插值目标
    // 实际逻辑已合并到UpdateNetwork()中
    void ReceiveGameStateFromHost();
    
    // 更新网络状态
    // 处理ENet事件（连接、接收、断开）
    // 主机端定期发送状态，客户端定期发送挡板位置
    // 每帧在Update()中调用
    void UpdateNetwork();
    
    // ========== 异步加载相关方法 ==========
    
    // 更新异步加载状态
    // 检查异步加载是否完成，完成后将纹理应用到游戏
    // 每帧在Update()中调用
    void UpdateAsyncLoading();
    
    // 绘制异步加载UI
    // 在MENU状态显示加载进度条和提示文字
    void DrawAsyncLoadingUI();
    
    // ========== 存档系统私有方法 ==========
    
    // 保存游戏进度到文件
    // 为什么需要存档系统：允许玩家中断游戏后继续，提升用户体验
    // 保存内容：
    //   - 当前关卡、分数、生命值、游戏时间
    //   - 小球位置和速度
    //   - 挡板位置和道具效果状态
    //   - 剩余的砖块信息
    // 返回值：true保存成功，false保存失败
    bool SaveGame(const std::string& filename = "savegame.json");
    
    // 从文件加载游戏进度
    // 返回值：true加载成功，false文件不存在或格式错误
    bool LoadGame(const std::string& filename = "savegame.json");
    
    // 检查存档文件是否存在
    // 返回值：true如果"savegame.json"存在
    bool SaveExists() const;
    
    // 从JSON文件加载数据
    // 封装JSON文件读取和解析，统一错误处理
    // 解析失败时返回包含"error":true的对象
    json LoadJSONFromFile(const std::string& path);
    
    // 根据JSON配置初始化砖块
    // 为什么需要：支持关卡设计器导出的自定义砖块布局
    // 从JSON的layout_data字段读取砖块布局矩阵
    // 1=红色,2=橙色,3=黄色,4=绿色,5=天蓝,6=蓝色,7=紫色,8=粉色
    void InitBricksFromJSON(const json& config);
    
    // 检查存档并提示
    // 游戏启动时在MENU状态调用，如有存档则打印提示信息
    void CheckForSaveFile();

public:
    // 构造函数
    // 初始化所有成员变量为默认值
    // 注意事项：实际游戏初始化需要在Init()中完成
    Game();
    
    // 析构函数
    // 释放异步加载器和网络资源
    ~Game();
    
    // ========== 公共接口 ==========
    
    // 初始化游戏
    // 必须在使用其他功能前调用
    // 执行的操作：
    //   1. 加载config.json配置文件
    //   2. 初始化异步加载器
    //   3. 创建小球和挡板对象
    //   4. 加载排行榜
    //   5. 初始化随机数种子
    //   6. 检查存档文件
    // 调用后游戏处于MENU状态
    void Init();
    
    // 更新游戏逻辑
    // 每帧调用一次，必须在Init()之后调用
    // 调用顺序：
    //   1. 更新网络状态
    //   2. 处理用户输入
    //   3. 更新挡板效果时间
    //   4. 更新异步加载状态
    //   5. 如果状态为PLAYING，更新游戏逻辑、道具效果、粒子、小球
    void Update();
    
    // 绘制所有游戏内容
    // 每帧调用一次，在Update()之后调用
    // 根据当前游戏状态绘制不同的界面（菜单/游戏中/暂停/结束等）
    void Draw();
    
    // 关闭游戏，释放资源
    // 在游戏退出前调用
    // 释放的资源：
    //   - 保存排行榜
    //   - 删除异步加载器
    //   - 卸载纹理
    //   - 断开网络连接
    //   - 反初始化ENet
    void Shutdown();
    
    // 初始化网络功能
    // 使用方法：
    //   // 主机模式
    //   game.InitNetwork(true);
    //   // 客户端模式
    //   game.InitNetwork(false, "192.168.1.100");
    // 参数说明：
    //   asHost: true=作为主机（服务器），false=作为客户端
    //   serverIP: 客户端模式下要连接的服务端IP地址，主机模式可省略
    // 注意事项：需要ENet库支持，如未安装则此函数无效果
    void InitNetwork(bool asHost, const char* serverIP = nullptr);
    
    // ========== 道具效果接口（供PowerUpEffect子类调用） ==========
    
    // 获取挡板引用，供道具效果修改挡板属性
    // 返回值：挡板对象的引用
    Paddle& GetPaddle() { return paddle; }
    
    // 添加额外小球（多球道具效果）
    // 在挡板位置生成额外的小球，方向与主球成角度
    void AddExtraBalls(int count);
    
    // 减慢小球速度（减速道具效果）
    // 将所有小球的速度乘以factor倍率
    void SlowDownBalls(float factor);
    
    // 恢复小球速度（减速效果结束时调用）
    // 将速度除以之前的速度倍率
    void RestoreBallSpeed();
    
    // ========== 异步加载接口 ==========
    
    // 请求异步加载纹理
    // 在后台线程加载纹理，不阻塞主线程
    void RequestAsyncLoad(const std::string& texturePath);
    
    // 检查是否正在异步加载
    // 返回值：true正在加载中
    bool IsAsyncLoading() const;
    
    // 获取异步加载进度
    // 返回值：进度值，范围0.0 ~ 1.0
    float GetAsyncLoadProgress() const;
    
    // ========== 网络接口 ==========
    
    // 获取对手挡板的X坐标（客户端插值后）
    // 用于绘制对手的挡板
    float GetOpponentPaddleX() const { return opponentPaddleX; }
    
    // 获取对手分数
    int GetOpponentScore() const { return opponentScore; }
    
    // 判断是否处于网络游戏模式
    // 返回值：true已连接网络对局
    bool IsNetworkGame() const { return netHost != nullptr && isConnected; }
    
    // 判断当前是否为游戏主机
    bool IsHost() const { return isHost; }
    
    // ========== 关卡系统接口 ==========
    
    // 初始化3个关卡的配置数据
    // 硬编码关卡的默认参数，后续可从JSON加载扩展
    void InitLevels();
    
    // 加载指定关卡
    // 根据关卡编号应用对应的参数（球速倍率、生命值、分数倍率等）
    // 并按照布局类型生成砖块
    // 参数level：关卡编号，有效值1-3
    void LoadLevel(int level);
    
    // 绘制关卡选择界面
    // 显示3个关卡的卡片，包含难度、参数预览等信息
    void DrawLevelSelect();
};

#endif