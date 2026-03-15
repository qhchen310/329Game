#ifndef DEF_H
#define DEF_H

#include <SDL3/SDL.h>

typedef enum
{
    ASSET_STATIC,
    ASSET_ANIMATED
} AssetType;

typedef struct
{
    int id;
    AssetType type;
    SDL_FRect uv;         // 静态贴图或动画第一帧
    int frameCount;       // 帧数（1 代表静态）
    float frameSpeed;     // 播放速度（针对动态草地等）
    float pivotX, pivotY; // 锚点：地砖 (0.5, 0.5)，人物 (0.5, 1.0)
} AssetDef;

#endif