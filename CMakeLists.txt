# CMakeLists.txt
# CMake 构建配置文件
#
# 本文件定义了项目的构建规则，包括：
#   - 编译器设置（C++17标准）
#   - 依赖库查找（Raylib, nlohmann/json, ENet, Threads）
#   - 源文件列表
#   - 可执行文件生成
#   - 配置文件复制
#   - 测试程序构建
#
# 使用方法：
#   mkdir build && cd build
#   cmake ..
#   make -j$(nproc)
#   ./breakout_game

# 最低CMake版本要求：3.10
# 为什么是3.10：支持FetchContent模块（用于自动下载nlohmann/json）
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(breakout_week2)

# ========== C++标准设置 ==========
# 使用C++17标准，这是必需的（代码中使用了std::optional、std::variant等特性）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 禁止编译器特定扩展，确保代码可移植
set(CMAKE_CXX_EXTENSIONS OFF)

# ========== 编译选项 ==========
# GCC和Clang编译器启用常用警告
# -Wall: 开启大部分常见警告
# -Wextra: 开启额外警告（如未使用参数）
# 注意：移除了-Wpedantic以避免过多警告（如结构体初始化）
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra)
endif()

# ========== 查找 Raylib ==========
# Raylib是必需的图形库，如未找到编译将失败
find_package(raylib REQUIRED)
message(STATUS "Raylib found: ${raylib_VERSION}")

# ========== nlohmann/json 配置 ==========
# 查找系统中的nlohmann/json库
find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "nlohmann_json found (config mode)")
else()
    # 如果系统未安装，尝试在include目录查找
    find_path(NLOHMANN_JSON_INCLUDE_DIR "nlohmann/json.hpp")
    if(NLOHMANN_JSON_INCLUDE_DIR)
        message(STATUS "nlohmann_json found at: ${NLOHMANN_JSON_INCLUDE_DIR}")
        # 创建INTERFACE库，仅包含头文件路径
        add_library(nlohmann_json INTERFACE)
        target_include_directories(nlohmann_json INTERFACE ${NLOHMANN_JSON_INCLUDE_DIR})
    else()
        # 如果系统也没有，使用FetchContent自动从GitHub下载
        # 为什么这样做：简化依赖安装，用户无需手动安装nlohmann/json
        message(STATUS "nlohmann_json not found, downloading from GitHub...")
        include(FetchContent)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
        )
        FetchContent_MakeAvailable(nlohmann_json)
        message(STATUS "nlohmann_json downloaded and configured")
    endif()
endif()

# ========== ENet 配置（可选，用于多人模式） ==========
# ENet是网络库，用于双人联机功能
# 如果未找到，会禁用多人模式，但单人游戏仍然可以正常运行
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(ENET libenet)
    if(ENET_FOUND)
        message(STATUS "ENet found - Multiplayer support ENABLED")
    else()
        message(STATUS "ENet not found - Multiplayer support DISABLED")
        message(STATUS "Install libenet-dev to enable multiplayer: sudo apt install libenet-dev")
    endif()
else()
    message(STATUS "pkg-config not found - Multiplayer support DISABLED")
endif()

# ========== 查找线程库 ==========
# pthread是必需的，用于异步资源加载（std::async）
find_package(Threads REQUIRED)

# ========== 源文件目录 ==========
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# ========== 收集主程序源文件 ==========
set(MAIN_SRC_FILES
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${SRC_DIR}/Game.cpp
    ${SRC_DIR}/Ball.cpp
    ${SRC_DIR}/Paddle.cpp
    ${SRC_DIR}/Brick.cpp
    ${SRC_DIR}/PowerUp.cpp
)

# ========== 创建主可执行文件 ==========
add_executable(breakout_game ${MAIN_SRC_FILES})

# ========== 包含目录 ==========
# 添加include和src目录到头文件搜索路径
target_include_directories(breakout_game PRIVATE 
    ${INCLUDE_DIR}
    ${SRC_DIR}
)

# ========== 链接库 ==========
# Raylib - 图形和输入处理
target_link_libraries(breakout_game raylib)

# nlohmann/json - JSON解析
target_link_libraries(breakout_game nlohmann_json)

# 线程库 - std::async支持
target_link_libraries(breakout_game Threads::Threads)

# ENet（如果找到）- 网络联机
if(ENET_FOUND)
    target_include_directories(breakout_game PRIVATE ${ENET_INCLUDE_DIRS})
    target_link_libraries(breakout_game ${ENET_LIBRARIES})
    # 定义USE_MULTIPLAYER宏，启用main.cpp中的多人模式代码
    target_compile_definitions(breakout_game PRIVATE USE_MULTIPLAYER)
endif()

# ========== 编译时复制配置文件 ==========
# 确保config.json在构建目录中可用
# 为什么需要：游戏运行时需要读取config.json，放在build目录便于执行
if(EXISTS ${CMAKE_SOURCE_DIR}/config.json)
    add_custom_command(
        TARGET breakout_game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/config.json
            $<TARGET_FILE_DIR:breakout_game>/config.json
        COMMENT "Copying config.json to build directory"
    )
endif()

# ========== 测试可执行文件 ==========
# 单元测试程序，验证碰撞检测等功能
set(TEST_SRC_FILES
    ${CMAKE_SOURCE_DIR}/tests/test.cpp
    ${SRC_DIR}/Game.cpp        # 需要Game.cpp中的函数定义（如AddExtraBalls）
    ${SRC_DIR}/Ball.cpp
    ${SRC_DIR}/Paddle.cpp
    ${SRC_DIR}/Brick.cpp
    ${SRC_DIR}/PowerUp.cpp
)

add_executable(test_game ${TEST_SRC_FILES})

target_include_directories(test_game PRIVATE 
    ${INCLUDE_DIR}
    ${SRC_DIR}
)

target_link_libraries(test_game 
    raylib 
    nlohmann_json
    Threads::Threads
)

# ========== 安装规则（可选） ==========
# 支持 make install 安装游戏到系统
install(TARGETS breakout_game RUNTIME DESTINATION bin)
install(FILES config.json DESTINATION bin)

# ========== 打印配置摘要 ==========
# 显示构建配置信息，便于调试
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Breakout Game Configuration Summary:")
message(STATUS "========================================")
message(STATUS "  C++ Standard:      C++17")
message(STATUS "  Raylib:            ${raylib_VERSION}")
message(STATUS "  nlohmann/json:     ${nlohmann_json_VERSION}")
if(ENET_FOUND)
    message(STATUS "  ENet:              ${ENET_VERSION} (Multiplayer ENABLED)")
else()
    message(STATUS "  ENet:              Not found (Multiplayer DISABLED)")
endif()
message(STATUS "  Threads:           ${CMAKE_THREAD_LIBS_INIT}")
message(STATUS "========================================")
message(STATUS "")