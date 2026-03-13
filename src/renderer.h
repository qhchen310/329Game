#ifndef RENDERER_H
#define RENDERER_H

#include <SDL3/SDL.h>
#include <tile.h>
#include <camera.h>

#define MAX_ENTITIES 1024
#define ATLAS_PATH "assets/textures/tile_grass.png"

typedef struct
{
    SDL_Texture *texture; // 素材图（为 NULL 时可以画调试色块）
    SDL_FRect srcRect;    // 图集切割区域
    SDL_FRect dstRect;    // 屏幕绘制区域
    float depth;          // 排序权重 (gridX + gridY)
} RenderItem;

// 2. 相当于“类”的定义
typedef struct
{
    RenderItem *items; // 建议用指针，方便动态扩展或在初始化时分配内存
    int count;
    int capacity;
} RenderQueue;

bool Renderer_Init(SDL_Renderer *renderer, int queueCapacity);
void Renderer_Submit(SDL_Texture *tex, SDL_FRect src, SDL_FRect dst, float depth);
void Renderer_SubmitTile(const Camera *cam, TileTypeID tileID, float gx, float gy, float z_offset);
void Renderer_Present(SDL_Renderer *renderer); // 内部执行 Flush
void RenderQueue_Sort();
void RenderQueue_DrawAll(SDL_Renderer *renderer);
void RenderQueue_Destroy(RenderQueue *queue);

#endif