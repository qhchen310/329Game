#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "tile.h"

// 地图对象（存储实际关卡布局）
typedef struct
{
    int width;        // 地图宽度（格）
    int height;       // 地图高度（格）
    int *groundLayer; // 地表层数组 (w * h)
    int *objectLayer; // 物体层数组 (w * h) - 0代表空
} GameMap;

// 4.地图相关函数
int Map_GetIndex(int x, int y, int width);
GameMap *Map_Create(int w, int h);                                                                    // 辅助函数：坐标转索引
int Map_GetSafeObject(const GameMap *map, int x, int y);                                              // 安全获取物体层数据
void Map_PlaceObject(GameMap *map, int startX, int startY, int width, int height, TileTypeID tileID); // 放置多格物体
void Map_Destroy(GameMap *self);

#endif // MAP_H