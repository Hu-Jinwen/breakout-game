#ifndef ASYNC_RESOURCE_LOADER_H
#define ASYNC_RESOURCE_LOADER_H

#include <future>
#include <atomic>
#include <mutex>
#include <functional>
#include <string>
#include <memory>
#include <thread>      // 添加：用于 std::this_thread
#include <chrono>      // 添加：用于 std::chrono
#include <unordered_map>
#include "raylib.h"

// ========== 任务1 & 2: 异步加载状态 ==========
enum class AsyncLoadState {
    IDLE,       // 空闲
    LOADING,    // 加载中
    LOADED,     // 加载完成
    ERROR       // 加载错误
};

// ========== 任务3: 线程安全的共享数据 ==========
struct LoadStatus {
    std::atomic<AsyncLoadState> state{AsyncLoadState::IDLE};
    std::atomic<float> progress{0.0f};
    std::string loadedTexturePath;
    bool loadSuccessful{false};
};

// ========== 任务4: 纹理缓存（线程安全） ==========
class TextureCache {
private:
    std::unordered_map<std::string, Texture2D> cache;
    std::mutex mtx;
    
public:
    TextureCache() = default;
    ~TextureCache() {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& pair : cache) {
            if (pair.second.id != 0) {
                UnloadTexture(pair.second);
            }
        }
        cache.clear();
    }
    
    // 线程安全的获取/加载纹理
    Texture2D Get(const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache.find(path);
        if (it != cache.end()) {
            return it->second;
        }
        // 如果不存在，返回空纹理（需要在主线程实际加载）
        return Texture2D{0};
    }
    
    // 线程安全的存入纹理
    void Put(const std::string& path, Texture2D texture) {
        std::lock_guard<std::mutex> lock(mtx);
        cache[path] = texture;
    }
    
    bool Has(const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx);
        return cache.find(path) != cache.end();
    }
};

// ========== 异步资源加载器 ==========
class AsyncResourceLoader {
private:
    std::future<Texture2D> loadFuture;
    std::future<void> loadImageFuture;
    LoadStatus status;
    TextureCache& textureCache;
    std::string loadingPath;
    
    // 模拟加载大纹理的耗时操作
    Texture2D SimulateLoadTexture(const std::string& path) {
        // 模拟耗时加载（0.5-1.5秒）
        int duration = 500 + (rand() % 1000);
        int steps = 50;
        
        for (int i = 0; i <= steps; i++) {
            if (status.state.load() == AsyncLoadState::IDLE) {
                break;
            }
            status.progress = static_cast<float>(i) / steps;
            std::this_thread::sleep_for(std::chrono::milliseconds(duration / steps));
        }
        
        // 实际创建一个棋盘格纹理来模拟加载
        Image img = GenImageChecked(128, 128, 16, 16, RED, YELLOW);
        Texture2D tex = LoadTextureFromImage(img);
        UnloadImage(img);
        
        return tex;
    }
    
public:
    AsyncResourceLoader(TextureCache& cache) : textureCache(cache) {}
    
    // 任务1: 使用 std::async 创建工作线程
    void StartLoadTexture(const std::string& path) {
        if (status.state != AsyncLoadState::IDLE) {
            return;
        }
        
        // 检查缓存
        if (textureCache.Has(path)) {
            status.loadedTexturePath = path;
            status.state = AsyncLoadState::LOADED;
            status.loadSuccessful = true;
            return;
        }
        
        loadingPath = path;
        status.state = AsyncLoadState::LOADING;
        status.progress = 0.0f;
        
        // 使用 std::async 异步加载
        loadFuture = std::async(std::launch::async, [this, path]() {
            return SimulateLoadTexture(path);
        });
    }
    
    // 自定义加载函数（用于真正的纹理加载）
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
        
        loadFuture = std::async(std::launch::async, [this, path, loader]() {
            int steps = 50;
            for (int i = 0; i <= steps; i++) {
                if (status.state != AsyncLoadState::LOADING) break;
                status.progress = static_cast<float>(i) / steps;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return loader(path);
        });
    }
    
    // 检查加载完成并获取结果
    bool TryGetLoadedTexture(Texture2D& outTexture) {
        if (status.state == AsyncLoadState::LOADING) {
            // 检查 future 是否完成（非阻塞）
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
        
        if (status.state == AsyncLoadState::LOADED) {
            outTexture = textureCache.Get(loadingPath);
            return true;
        }
        
        return false;
    }
    
    // 获取加载状态
    AsyncLoadState GetState() const { return status.state; }
    float GetProgress() const { return status.progress; }
    bool IsLoading() const { return status.state == AsyncLoadState::LOADING; }
    bool IsLoaded() const { return status.state == AsyncLoadState::LOADED; }
    
    // 重置加载器（允许重新加载）
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
    
    // 注意：不销毁 loadFuture，让它自然完成
    }

    // 重置已加载状态（允许重新加载相同的纹理）
    void ResetLoadedState() {
        if (status.state == AsyncLoadState::LOADED) {
            status.state = AsyncLoadState::IDLE;
            status.progress = 0.0f;
            status.loadSuccessful = false;
            status.loadedTexturePath.clear();
        }
    }

    // 强制重新开始加载（不管当前什么状态）
void ForceRestart() {
    // 重置所有状态
    status.state = AsyncLoadState::IDLE;
    status.progress = 0.0f;
    status.loadSuccessful = false;
    status.loadedTexturePath.clear();
}
};

#endif // ASYNC_RESOURCE_LOADER_H