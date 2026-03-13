#ifndef PLAYER_H
#define PLAYER_H

#include <SDL3/SDL.h>
#include "tile.h"
#include "map.h"

typedef enum
{
    DIR_N,
    DIR_NE,
    DIR_E,
    DIR_SE,
    DIR_S,
    DIR_SW,
    DIR_W,
    DIR_NW,
    DIR_NONE
} Direction;

typedef struct
{
    float gridX;
    float gridY;
    float speed;
    bool isMoving;
    Direction dir;
    float animTimer;  // 动画计时器 (秒)
    int currentFrame; // 当前第几帧 (0-3)
} Player;

void Player_Init(Player *player);
void Player_Update(Player *self, GameMap *map, float dt, const bool *keys);

#endif