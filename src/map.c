#include "map.h"
#include <stdlib.h>

// 辅助工具：计算 [y * width + x]
int Map_GetIndex(int x, int y, int width)
{
    return y * width + x;
}

GameMap *Map_Create(int w, int h)
{
    GameMap *map = (GameMap *)malloc(sizeof(GameMap));
    if (!map)
        return NULL;

    map->width = w;
    map->height = h;

    // 使用 SDL_calloc 可以自动清零，避免出现野 ID
    map->groundLayer = (int *)SDL_calloc(w * h, sizeof(int));
    if (!map->groundLayer)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Map_Create: Failed to allocate groundLayer");
        free(map);
        return NULL;
    }

    map->objectLayer = (int *)SDL_calloc(w * h, sizeof(int));
    if (!map->objectLayer)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Map_Create: Failed to allocate objectLayer");
        SDL_free(map->groundLayer);
        free(map);
        return NULL;
    }

    // 初始化 objectLayer 为 -1 (代表没有物体)
    for (int i = 0; i < w * h; i++)
        map->objectLayer[i] = -1;

    // 碰撞层：默认全 0（可通行），放物体时按素材属性写入
    map->solidLayer = (uint8_t *)SDL_calloc(w * h, sizeof(uint8_t));
    if (!map->solidLayer)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Map_Create: Failed to allocate solidLayer");
        SDL_free(map->groundLayer);
        SDL_free(map->objectLayer);
        free(map);
        return NULL;
    }

    return map;
}

// 增加一个防御性获取函数，防止越界崩溃
int Map_GetSafeObject(const GameMap *map, int x, int y)
{
    if (x < 0 || x >= map->width || y < 0 || y >= map->height)
    {
        return -1; // 越界
    }
    return map->objectLayer[Map_GetIndex(x, y, map->width)];
}

/**
 * 通行判定：这个格子能不能站人。
 * 只看碰撞层，和“物体 ID 是多少”完全无关：
 * - 地图外一律阻挡
 * - 碰撞层为 1 就是墙
 * @param map 地图
 * @param x, y 逻辑格坐标
 * @return true 表示不可通行
 */
bool Map_IsBlocked(const GameMap *map, int x, int y)
{
    if (x < 0 || x >= map->width || y < 0 || y >= map->height)
        return true; // 地图外 = 墙

    return map->solidLayer[Map_GetIndex(x, y, map->width)] != 0;
}

/**
 * 直接改某一格的碰撞标记（测试、开门这类动态变化用）。
 * 正常放置物体请用 Map_PlaceObject，它会按素材属性自动填。
 */
void Map_SetSolid(GameMap *map, int x, int y, bool solid)
{
    if (x < 0 || x >= map->width || y < 0 || y >= map->height)
        return;
    map->solidLayer[Map_GetIndex(x, y, map->width)] = solid ? 1 : 0;
}

/**
 * 在地图上放置物体（支持多格占用）。
 * 物体的“能不能走”由素材注册时的 solid 决定，这里一次性把两层都写好，
 * 不会出现“画得出来但穿不过去”或“挡人但看不见”的错位。
 * @param map 地图
 * @param am 资源管理器（用来查素材的 solid；传 NULL 则一律不挡路）
 * @param startX, startY 物品左上角（北端）的逻辑坐标
 * @param width, height  逻辑上占用的格数（如 2, 2）
 * @param assetID 物体使用的素材 ID
 */
void Map_PlaceObject(GameMap *map, const AssetManager *am, int startX, int startY, int width, int height, AssetID assetID)
{
    const AssetDef *def = am ? Asset_GetDef(am, assetID) : NULL;
    if (am && !def)
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Map_PlaceObject: AssetID %d 没有注册，这个物体不会显示", assetID);

    uint8_t solid = (def && def->solid) ? 1 : 0;

    for (int y = startY; y < startY + height; y++)
    {
        for (int x = startX; x < startX + width; x++)
        {
            // 确保不越界
            if (x >= 0 && x < map->width && y >= 0 && y < map->height)
            {
                int idx = Map_GetIndex(x, y, map->width);
                map->objectLayer[idx] = assetID;
                map->solidLayer[idx] = solid;
            }
        }
    }
}

void Map_Destroy(GameMap *self)
{
    if (self)
    {
        SDL_free(self->groundLayer);
        SDL_free(self->objectLayer);
        SDL_free(self->solidLayer);
        free(self); // map 结构体本身是用 malloc 分配的
    }
}