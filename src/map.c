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
        return -1; // 越界
    }
    return map->objectLayer[Map_GetIndex(x, y, map->width)];
}

/**
 * 通行判定：这个格子能不能站人。
 * - 地图外一律阻挡（越界不再靠“返回 -1”暗示，边界在这里说死）
 * - 空格（-1）可通行
 * - 有东西的格子查 TileDef.isWalkable
 * - 还没登记过 TileDef 的 ID 保守当作障碍，避免静默穿模
 * @param map 地图
 * @param x, y 逻辑格坐标
 * @return true 表示不可通行
 */
bool Map_IsBlocked(const GameMap *map, int x, int y)
{
    if (x < 0 || x >= map->width || y < 0 || y >= map->height)
        return true; // 地图外 = 墙

    int id = map->objectLayer[Map_GetIndex(x, y, map->width)];
    if (id < 0)
        return false; // 空格

    if (id >= TILE_COUNT)
        return true; // 未登记的类型，先按实体处理

    const TileDef *def = Tile_GetDef((TileTypeID)id);
    return !def || !def->isWalkable;
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