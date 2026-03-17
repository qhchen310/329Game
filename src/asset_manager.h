#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <SDL3/SDL.h>
#include <config.h>

typedef int AssetID;
typedef int TextureHandle;

#define INVALID_HANDLE -1
#define Asset_RegisterStatic(am, id, file, x, y, w, h, px, py) \
    Asset_Register(am, id, file, x, y, w, h, px, py, 1, 0.0f, 1)

// 注册简单单行动画
#define Asset_RegisterAnim(am, id, file, x, y, w, h, px, py, count, dur) \
    Asset_Register(am, id, file, x, y, w, h, px, py, count, dur, count)

// 原始纹理资源
typedef struct
{
    SDL_Texture *texture;
    char path[256];
    int ref_count; // 引用计数，用于资源卸载
} TextureResource;

// 逻辑素材定义（数据驱动的核心）

typedef struct
{
    AssetID id;           // 逻辑 ID
    TextureHandle tex_h;  // 指向纹理池的索引
    SDL_FRect src_rect;   // 在图集中的坐标
    float pivotX, pivotY; // 锚点 (0.0~1.0)

    // --- 动画核心属性 ---
    bool is_animated;     // 是否为动画素材
    int frame_count;      // 总帧数
    float frame_duration; // 每帧持续时间 (单位: 秒，例如 0.1f 代表 10FPS)
    int frames_per_row;   // 图集中每一行有多少帧 (用于自动计算下一帧的 UV 偏移)
    bool loop;            // 是否循环播放
} AssetDef;

// 管理器主体
typedef struct
{
    SDL_Renderer *renderer;

    TextureResource *texture_pool; // 纹理资源池
    int texture_count;             // 当前加载的纹理数量
    int texture_capacity;          // 纹理数组的容量

    AssetDef *defs;   // 逻辑素材定义数组
    int def_count;    // 当前定义的素材数量
    int def_capacity; // 定义数组的容量
} AssetManager;

AssetManager *Asset_Create(SDL_Renderer *renderer);
void Asset_Register(AssetManager *am, AssetID id, const char *filename, float x, float y, float w, float h, float px, float py, int frame_count, float frame_duration, int frames_per_row);
const AssetDef *Asset_GetDef(AssetManager *am, AssetID id);
SDL_Texture *Asset_GetRawTexture(AssetManager *am, TextureHandle h);
void Asset_Destroy(AssetManager *am);

#endif