#ifndef CONFIG_H
#define CONFIG_H

// 基础网格定义
#define TILE_WIDTH 128.0f
#define TILE_HEIGHT 64.0f

#define MAP_WIDTH 20
#define MAP_HEIGHT 20

#define TITLE "SDL3 2.5D Engine"

#define QUEUE_INITIAL_CAPACITY 1024

// 单帧最大步进（秒）。卡顿/拖窗口/断点恢复时，dt 可能是几百毫秒，
// 直接拿去算位移会一帧穿过整堵墙，所以在这里封顶。
#define MAX_DELTA_TIME (1.0f / 30.0f)

// 窗口定义
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

// 资源目录
#define ASSET_DIR "assets/textures/"

// 纹理资源池容量
#define TEXTURE_CAPACITY 16

// 逻辑素材定义容量
#define DEF_CAPACITY 128

#endif