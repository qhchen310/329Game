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

    return map;
}

// 增加一个防御性获取函数，防止越界崩溃
int Map_GetSafeObject(const GameMap *map, int x, int y)
{
    if (x < 0 || x >= map->width || y < 0 || y >= map->height)
    {
        return -1; // 地图外视为障碍物
    }
    return map->objectLayer[Map_GetIndex(x, y, map->width)];
}

/**
 * 在地图上放置物体（支持多格占用）
 * @param startX, startY 物品左上角（北端）的逻辑坐标
 * @param width, height  逻辑上占用的格数（如 2, 2）
 */
void Map_PlaceObject(GameMap *map, int startX, int startY, int width, int height, TileTypeID tileID)
{
    for (int y = startY; y < startY + height; y++)
    {
        for (int x = startX; x < startX + width; x++)
        {
            // 确保不越界
            if (x >= 0 && x < map->width && y >= 0 && y < map->height)
            {
                map->objectLayer[Map_GetIndex(x, y, map->width)] = tileID;
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
        free(self); // map 结构体本身是用 malloc 分配的
    }
}