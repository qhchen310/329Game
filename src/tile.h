#ifndef TILE_H
#define TILE_H

#include <SDL3/SDL.h>

typedef enum
{
    TILE_GRASS,
    TILE_WATER,
    TILE_WALL,
    TILE_COUNT // 这个必须放在最后，表示总共有多少种类型
} TileTypeID;

// 1. 瓦片类型定义（存储在图集中的信息）
// 注意：碰撞不再由这里判断，统一看 AssetDef.solid（在地图放置物体时写入碰撞层）
typedef struct
{
    int id;
    SDL_FRect srcRect; // 在图集里的像素矩形（x, y, w, h）
    float pivotX;      // 重心/锚点 X
    float pivotY;      // 重心/锚点 Y
} TileDef;

// 3. 瓦片定义函数和获取函数
void Tile_Define(TileTypeID id, float sx, float sy, float sw, float sh, float px, float py);
const TileDef *Tile_GetDef(TileTypeID id);

#endif