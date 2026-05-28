#ifndef ASYNC_RESOURCE_LOADER_H
#define ASYNC_RESOURCE_LOADER_H

#include <future>
#include <atomic>
#include <mutex>
#include <functional>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <unordered_map>
#include "raylib.h"

// ========== 任务1 & 2: 异步加载状态 ==========
// 异步加载状态枚举
// 用于标记异步加载任务所处的阶段
// IDLE -> LOADING -> LOADED/ERROR
enum class AsyncLoadState {
    IDLE,       // 空闲：没有正在进行的加载任务
    LOADING,    // 加载中：后台线程正在处理
    LOADED,     // 加载完成：纹理已成功加载并存入缓存
    ERROR       // 加载错误：文件不存在或解析失败
};

// ========== 任务3: 线程安全的共享数据 ==========
// 加载状态结构体
// 用于在主线程和工作线程之间安全地传递加载进度和结果
//
// 为什么使用std::atomic：
//   - 避免使用互斥锁，提高性能
//   - state和progress需要跨线程访问，使用原子操作保证线程安全
//
// 使用方法：
//   - 工作线程更新progress和state
//   - 主线程通过GetState()和GetProgress()读取
struct LoadStatus {
    std::atomic<AsyncLoadState> state{AsyncLoadState::IDLE};  // 当前加载状态，原子操作
    std::atomic<float> progress{0.0f};                        // 加载进度 0.0~1.0，原子操作
    std::string loadedTexturePath;                            // 加载完成的纹理路径（主线程只读）
    bool loadSuccessful{false};                               // 加载是否成功
};

// ========== 任务4: 纹理缓存（线程安全） ==========
// class TextureCache
// 纹理缓存管理器，线程安全
//
// 职责：
//   - 缓存已加载的纹理，避免重复加载
//   - 使用互斥锁保护缓存容器，支持多线程访问
//   - 析构时自动卸载所有纹理
//
// 主要用法：
//   TextureCache cache;
//   Texture2D tex = cache.Get("path.png");  // 从缓存获取
//   if (tex.id == 0) {
//       tex = LoadTexture("path.png");       // 缓存未命中，手动加载
//       cache.Put("path.png", tex);          // 存入缓存
//   }
//
// 注意事项：
//   - Get()和Put()都是线程安全的
//   - 析构时会自动调用UnloadTexture()释放GPU内存
class TextureCache {
private:
    std::unordered_map<std::string, Texture2D> cache;  // 路径 -> 纹理 的映射表
    std::mutex mtx;      // 互斥锁，保护cache的并发访问
    
public:
    TextureCache() = default;
    
    // 析构函数
    // 自动卸载所有缓存的纹理，释放GPU内存
    ~TextureCache() {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& pair : cache) {
            if (pair.second.id != 0) {
                UnloadTexture(pair.second);
            }
        }
        cache.clear();
    }
    
    // 从缓存获取纹理（线程安全）
    // 参数path：纹理文件路径
    // 返回值：Texture2D对象，如果缓存未命中则返回空纹理(id=0)
    // 注意：返回空纹理时调用方需要检查id是否为0
    Texture2D Get(const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache.find(path);
        if (it != cache.end()) {
            return it->second;
        }
        // 返回空纹理（Raylib中id=0表示无效纹理）
        // 使用{}初始化所有成员为0，避免编译警告
        return Texture2D{0, 0, 0, 0, 0};
    }
    
    // 将纹理存入缓存（线程安全）
    // 参数path：纹理文件路径，作为键
    // 参数texture：纹理对象
    void Put(const std::string& path, Texture2D texture) {
        std::lock_guard<std::mutex> lock(mtx);
        cache[path] = texture;
    }
    
    // 检查缓存中是否存在指定纹理（线程安全）
    bool Has(const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx);
        return cache.find(path) != cache.end();
    }
};

// class AsyncResourceLoader
// 异步资源加载器，使用std::async在后台线程加载纹理
//
// 为什么需要异步加载：
//   - 避免主线程被阻塞，防止游戏卡顿
//   - 大纹理加载可能需要几百毫秒，同步加载会导致画面冻结
//   - 用户可以在加载过程中看到进度条，体验更好
//
// 主要用法：
//   AsyncResourceLoader loader(cache);
//   loader.StartLoadTexture("texture.png");
//   while (loader.IsLoading()) {
//       float progress = loader.GetProgress();
//       DrawProgressBar(progress);
//   }
//   Texture2D tex;
//   if (loader.TryGetLoadedTexture(tex)) {
//       // 使用纹理
//   }
//
// 实现原理：
//   - 使用std::async创建后台线程执行SimulateLoadTexture()
//   - 主线程通过TryGetLoadedTexture()非阻塞地检查完成状态
//   - 加载完成后自动存入TextureCache
//
// 注意事项：
//   - 当前版本模拟加载，实际使用时需要替换为真实LoadTexture
//   - Raylib的LoadTexture必须在主线程调用，因此本类加载的是模拟纹理
//   - 如需真实异步加载，需要使用Image + LoadTextureFromImage组合
class AsyncResourceLoader {
private:
    std::future<Texture2D> loadFuture;      // 异步任务的future对象
    std::future<void> loadImageFuture;      // 备用future（当前未使用）
    LoadStatus status;                      // 加载状态（原子操作）
    TextureCache& textureCache;             // 纹理缓存引用
    std::string loadingPath;                // 正在加载的纹理路径
    
    // 模拟加载大纹理的耗时操作
    // 为什么模拟：Raylib的LoadTexture必须在主线程调用
    // 实现方式：生成棋盘格纹理替代真实加载，用于演示异步加载效果
    // 参数path：纹理路径（当前未使用，保留接口）
    // 返回值：生成的棋盘格纹理
    Texture2D SimulateLoadTexture(const std::string& /*path*/) {
        // 模拟耗时加载（0.5-1.5秒随机）
        int duration = 500 + (rand() % 1000);
        int steps = 50;
        
        for (int i = 0; i <= steps; i++) {
            // 检查是否被中断（重置标志）
            if (status.state.load() == AsyncLoadState::IDLE) {
                break;
            }
            // 更新进度（0.0 ~ 1.0）
            status.progress = static_cast<float>(i) / steps;
            // 模拟加载耗时
            std::this_thread::sleep_for(std::chrono::milliseconds(duration / steps));
        }
        
        // 实际创建一个棋盘格纹理来模拟加载
        Image img = GenImageChecked(128, 128, 16, 16, RED, YELLOW);
        Texture2D tex = LoadTextureFromImage(img);
        UnloadImage(img);
        
        return tex;
    }
    
public:
    // 构造函数
    // 参数cache：纹理缓存引用
    AsyncResourceLoader(TextureCache& cache) : textureCache(cache) {}
    
    // 开始异步加载纹理（模拟版本）
    // 使用std::async创建工作线程，不阻塞主线程
    // 参数path：纹理文件路径
    void StartLoadTexture(const std::string& path) {
        // 已有加载任务在进行，忽略新请求
        if (status.state != AsyncLoadState::IDLE) {
            return;
        }
        
        // 检查缓存是否已存在
        if (textureCache.Has(path)) {
            status.loadedTexturePath = path;
            status.state = AsyncLoadState::LOADED;
            status.loadSuccessful = true;
            return;
        }
        
        loadingPath = path;
        status.state = AsyncLoadState::LOADING;
        status.progress = 0.0f;
        
        // 使用std::async在后台线程执行加载任务
        // std::launch::async 保证在新线程中执行
        loadFuture = std::async(std::launch::async, [this, path]() {
            return SimulateLoadTexture(path);
        });
    }
    
    // 开始异步加载纹理（自定义加载器版本）
    // 允许调用方自定义加载函数，便于扩展
    // 参数path：纹理文件路径
    // 参数loader：自定义加载函数，类型为 std::function<Texture2D(const std::string&)>
    void StartLoadTextureWithCustomLoader(const std::string& path, 
                                          std::function<Texture2D(const std::string&)> loader) {
        if (status.state != AsyncLoadState::IDLE) {
            return;
        }
        
        if (textureCache.Has(path)) {
            status.loadedTexturePath = path;
            status.state = AsyncLoadState::LOADED;
            status.loadSuccessful = true;
            return;
        }
        
        loadingPath = path;
        status.state = AsyncLoadState::LOADING;
        status.progress = 0.0f;
        
        // 模拟进度更新，然后执行自定义加载器
        loadFuture = std::async(std::launch::async, [this, path, loader]() {
            // 模拟进度更新（50步，每步10ms）
            int steps = 50;
            for (int i = 0; i <= steps; i++) {
                if (status.state != AsyncLoadState::LOADING) break;
                status.progress = static_cast<float>(i) / steps;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return loader(path);
        });
    }
    
    // 尝试获取已加载的纹理（非阻塞）
    // 检查异步加载是否完成，完成后提取纹理并存入缓存
    // 参数outTexture：输出参数，成功时存储纹理对象
    // 返回值：true=加载成功且已获取纹理，false=仍在加载或失败
    bool TryGetLoadedTexture(Texture2D& outTexture) {
        // 正在加载中，检查是否完成
        if (status.state == AsyncLoadState::LOADING) {
            // 检查future是否完成（非阻塞，wait_for(0)立即返回）
            if (loadFuture.valid() && 
                loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                try {
                    Texture2D tex = loadFuture.get();
                    textureCache.Put(loadingPath, tex);
                    status.loadedTexturePath = loadingPath;
                    status.state = AsyncLoadState::LOADED;
                    status.loadSuccessful = true;
                    outTexture = tex;
                    return true;
                } catch (const std::exception& e) {
                    status.state = AsyncLoadState::ERROR;
                    status.loadSuccessful = false;
                    return false;
                }
            }
            return false;
        }
        
        // 已加载完成，直接从缓存获取
        if (status.state == AsyncLoadState::LOADED) {
            outTexture = textureCache.Get(loadingPath);
            return true;
        }
        
        return false;
    }
    
    // 获取加载状态
    AsyncLoadState GetState() const { return status.state; }
    
    // 获取加载进度（0.0 ~ 1.0）
    float GetProgress() const { return status.progress; }
    
    // 检查是否正在加载中
    bool IsLoading() const { return status.state == AsyncLoadState::LOADING; }
    
    // 检查是否已加载完成
    bool IsLoaded() const { return status.state == AsyncLoadState::LOADED; }
    
    // 重置加载器（允许重新加载）
    // 如果正在加载，标记为IDLE让加载线程退出
    void Reset() {
        // 如果正在加载，标记为 IDLE 让加载线程退出
        if (status.state == AsyncLoadState::LOADING) {
            status.state = AsyncLoadState::IDLE;
        }
        // 重置所有状态
        status.state = AsyncLoadState::IDLE;
        status.progress = 0.0f;
        status.loadSuccessful = false;
        status.loadedTexturePath.clear();
        // 注意：不销毁loadFuture，让它自然完成
    }

    // 重置已加载状态（允许重新加载相同的纹理）
    // 用于需要重新加载同一纹理的场景
    void ResetLoadedState() {
        if (status.state == AsyncLoadState::LOADED) {
            status.state = AsyncLoadState::IDLE;
            status.progress = 0.0f;
            status.loadSuccessful = false;
            status.loadedTexturePath.clear();
        }
    }

    // 强制重新开始加载（不管当前什么状态）
    // 用于用户手动触发重新加载（如按L键）
    void ForceRestart() {
        // 重置所有状态
        status.state = AsyncLoadState::IDLE;
        status.progress = 0.0f;
        status.loadSuccessful = false;
        status.loadedTexturePath.clear();
    }
};

#endif // ASYNC_RESOURCE_LOADER_H