#include "tile.h"
#include <stdlib.h>

// 1. 私有定义表：所有的 Tile 属性都存在这里
// 使用 TileID 枚举作为索引，TILE_COUNT 是枚举里的最后一项
static TileDef g_tileLibrary[TILE_COUNT];

/**
 * 工业级 Tile 定义：现在不需要传入指针，直接传入枚举 ID
 * sx, sy: 图集里的像素起点
 * px, py: 锚点 (通常 32, 64)
 */
void Tile_Define(TileTypeID id, float sx, float sy, float sw, float sh,
                 float px, float py, bool walkable)
{
    if (id < 0 || id >= TILE_COUNT)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Tile_InitDef: Invalid tile ID %d", id);
        return;
    }

    TileDef *def = &g_tileLibrary[id];

    def->id = id;

    // 计算图集切割位置
    def->srcRect = (SDL_FRect){sx, sy, sw, sh}; // 直接指定像素坐标
    // 设置锚点偏移
    def->pivotX = px;
    def->pivotY = py;
    // 是否可行走
    def->isWalkable = walkable;
}

/**
 * Tile获取
 */
const TileDef *Tile_GetDef(TileTypeID id)
{
    if (id < 0 || id >= TILE_COUNT)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Tile_GetDef: Invalid tile ID %d", id);
        return NULL;
    }

    return &g_tileLibrary[id];
}