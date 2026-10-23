#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "tile.h"
#include "asset_manager.h"

// 地图对象（存储实际关卡布局）
typedef struct
{
    int width;         // 地图宽度（格）
    int height;        // 地图高度（格）
    int *groundLayer;  // 地表层数组 (w * h)，存 TileTypeID / 素材 ID
    int *objectLayer;  // 物体层数组 (w * h)，存 AssetID，-1 表示空
    uint8_t *solidLayer; // 碰撞层 (w * h)，1 = 不可通行；由物体层的素材属性推导而来
} GameMap;

// 4.地图相关函数
int Map_GetIndex(int x, int y, int width);
GameMap *Map_Create(int w, int h);                    // 辅助函数：坐标转索引
int Map_GetSafeObject(const GameMap *map, int x, int y); // 安全获取物体层数据（越界返回 -1）
bool Map_IsBlocked(const GameMap *map, int x, int y);    // 通行判定：越界或碰撞层为 1
void Map_SetSolid(GameMap *map, int x, int y, bool solid); // 直接改某格的碰撞标记
// 放置物体：按素材注册时声明的 solid 同步写入碰撞层
void Map_PlaceObject(GameMap *map, const AssetManager *am, int startX, int startY, int width, int height, AssetID assetID);
void Map_Destroy(GameMap *self);

#endif // MAP_H
