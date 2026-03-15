#include "renderer.h"
#include <stdlib.h>
#include <string.h>
#include <iso_math.h>
#include <SDL3_image/SDL_image.h>

// 如果你以后想把地砖做得更“陡”或者更“平”，只需要改这个比例因子
static const float ISO_RATIO = 0.5f; // 16 / 32 = 0.5

static SDL_Texture *game_atlas = NULL;
static RenderQueue *main_queue = NULL; // 内部持有队列实例

static int CompareDepth(const void *a, const void *b);
static RenderQueue *RenderQueue_Create(int capacity);
static void RenderQueue_Push(RenderQueue *self, SDL_Texture *tex, SDL_FRect src, SDL_FRect dst, float depth);

static int CompareDepth(const void *a, const void *b)
{
    RenderItem *itemA = (RenderItem *)a;
    RenderItem *itemB = (RenderItem *)b;
    return (itemA->depth > itemB->depth) - (itemA->depth < itemB->depth);
}

static RenderQueue *RenderQueue_Create(int capacity)
{
    RenderQueue *queue = (RenderQueue *)malloc(sizeof(RenderQueue));
    if (!queue)
        return NULL;

    queue->items = (RenderItem *)malloc(sizeof(RenderItem) * capacity);
    if (!queue->items)
    {
        free(queue);
        return NULL;
    }

    queue->count = 0;
    queue->capacity = capacity;
    return queue;
}

/**
 * @details 深度值的计算通常基于对象在世界坐标系中的位置，尤其是它们的 \(x\) 和 \(y\) 坐标。
 * 普通瓦片/地表：depth = worldX + worldY;
 * 角色/装饰物：depth = worldX + worldY + 0.01f; （微小的偏移确保物体永远在地板之上）
 * 飞行物：即便 \(z\) 轴很高，它的 depth 依然由它在地面上的投影点 \((x,y)\) 决定，这样它才能正确地从树木后面飞过。 
 */
static void RenderQueue_Push(RenderQueue *self, SDL_Texture *tex, SDL_FRect src, SDL_FRect dst, float depth)
{
    // 1. 安全检查：防止队列溢出
    if (!self || self->count >= self->capacity)
    {
        // 工业级做法通常在这里报错或记录日志
        return;
    }

    // 2. 获取当前槽位并填充数据
    RenderItem *item = &self->items[self->count];

    item->texture = tex;
    item->srcRect = src;
    item->dstRect = dst;
    item->depth = depth;

    // 3. 计数增加
    self->count++;
}

bool Renderer_Init(SDL_Renderer *renderer, int queueCapacity)
{
    // 1. 加载静态资源
    game_atlas = IMG_LoadTexture(renderer, ATLAS_PATH);
    if (!game_atlas)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load atlas %s: %s", ATLAS_PATH, SDL_GetError());
        return false;
    }

    // 像素画神器：邻近过滤
    SDL_SetTextureScaleMode(game_atlas, SDL_SCALEMODE_NEAREST);

    // 2. 创建渲染队列
    main_queue = RenderQueue_Create(queueCapacity);

    return true;
}

// 包装一层 Push，外部不需要知道 main_queue 的存在
void Renderer_Submit(SDL_Texture *tex, SDL_FRect src, SDL_FRect dst, float depth)
{
    RenderQueue_Push(main_queue, tex, src, dst, depth);
}

/**
 * 工业级 Renderer_SubmitTile
 * 坐标投影 + 锚点对齐 + 深度计算
 */
void Renderer_SubmitTile(const Camera *cam, TileTypeID tileID, float gx, float gy, float z_offset)
{
    const TileDef *tile = Tile_GetDef(tileID);

    if (!game_atlas)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer_SubmitTile: Atlas texture not loaded");
        return;
    }

    if (!tile || tile->id != tileID)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer_SubmitTile: Invalid tile ID %d", tileID);
        return;
    }

    // 使用宏进行坐标转换，直接传入 cam 指针
    float wx = GRID_TO_WORLD_X(gx, gy);
    float wy = GRID_TO_WORLD_Y(gx, gy) - z_offset;

    float sx = WORLD_TO_SCREEN_X(wx - tile->pivotX, cam);
    float sy = WORLD_TO_SCREEN_Y(wy - tile->pivotY, cam);

    // 3. 最终位置计算 (投影点 - 锚点 - 相机偏移 - 高度)
    SDL_FRect dst = {
        sx,
        sy,
        tile->srcRect.w * cam->zoom,
        tile->srcRect.h * cam->zoom};

    // 4. 深度排序
    float depth = gx + gy + (z_offset * 0.0001f);

    RenderQueue_Push(main_queue, game_atlas, tile->srcRect, dst, depth);
}

void RenderQueue_Sort()
{
    if (!main_queue || main_queue->count <= 1)
        return;

    // 使用标准库 qsort 进行快速排序
    qsort(main_queue->items, main_queue->count, sizeof(RenderItem), CompareDepth);
}

void RenderQueue_DrawAll(SDL_Renderer *renderer)
{
    if (!main_queue || !renderer)
        return;

    for (int i = 0; i < main_queue->count; i++)
    {
        RenderItem *item = &main_queue->items[i];

        if (item->texture != NULL)
        { // 正常画图集里的瓦片
            SDL_RenderTexture(renderer, item->texture, &item->srcRect, &item->dstRect);
        }
        else
        {
            // 如果纹理是空的（比如目前的玩家），画一个红色方块占位，防止崩溃
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // 红色
            SDL_RenderFillRect(renderer, &item->dstRect);
        }
    }
}

void Renderer_DrawObject(SDL_Renderer *r, AssetManager *am, AssetID id, float wx, float wy, Camera *cam)
{
    // 1. 从资源管理器获取“只读数据定义” (Data-Driven)
    const AssetDef *def = Asset_GetDef(am, id);
    if (!def)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer_DrawObject: Invalid AssetID %d", id);
        return;
    }

    // 2. 获取实际的物理纹理
    SDL_Texture *tex = Asset_GetTexture(am, def->texture_index);
    if (!tex)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer_DrawObject: Failed to get texture for AssetID %d", id);
        return;
    }

    // 3. 核心：坐标转换逻辑 (这是 Renderer 的核心职责)
    // 根据相机当前的物理位置和 Zoom 计算
    float sx = WORLD_TO_SCREEN_X(wx, cam);
    float sy = WORLD_TO_SCREEN_Y(wy, cam);

    SDL_FRect dest;
    dest.w = def->src_rect.w * cam->zoom;
    dest.h = def->src_rect.h * cam->zoom;

    // 4. 应用锚点偏移
    dest.x = sx - (dest.w * def->pivotX);
    dest.y = sy - (dest.h * def->pivotY);

    // 5. 提交给 GPU
    SDL_RenderTexture(r, tex, &def->src_rect, &dest);
}

/**
 * 销毁渲染队列
 * 通常在 SDL_AppQuit 或程序关闭时调用一次
 */
void RenderQueue_Destroy(RenderQueue *queue)
{
    if (!queue)
        return;

    // 1. 释放动态分配的节点数组内存
    if (queue->items != NULL)
    {
        free(queue->items);
        queue->items = NULL; // 安全起见，置为空
    }

    // 2. 重置计数器和容量
    queue->count = 0;
    queue->capacity = 0;

    // 注意：如果是通过 malloc 分配的 RenderQueue 结构体本身，
    // 也可以在这里 free(queue)，但这取决于你是在栈上还是堆上定义的它。
}