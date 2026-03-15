#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <SDL3/SDL.h>

typedef int AssetID;

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
    AssetID id;
    int texture_index;  // 指向 TextureResource 数组的索引
    SDL_FRect src_rect; // 在图集中的 UV 坐标
    float pivotX, pivotY;
    bool is_animated;
    int frame_count;
    float frame_duration;
} AssetDef;

// 管理器主体
typedef struct
{
    SDL_Renderer *renderer;

    TextureResource *textures;
    int texture_count;    // 当前加载的纹理数量
    int texture_capacity; // 纹理数组的容量

    AssetDef *defs;   // 逻辑素材定义数组
    int def_count;    // 当前定义的素材数量
    int def_capacity; // 定义数组的容量
} AssetManager;

AssetManager *Asset_Create(SDL_Renderer *renderer);
void Asset_RegisterDef(AssetManager *am, AssetDef def, const char *texName);
const AssetDef *Asset_GetDef(AssetManager *am, AssetID id);

#endif