#include "player.h"
#include <math.h>

#define INPUT_IDLE 0.0f
#define INPUT_FULL 1.0f

const float PLAYER_RADIUS = 0.25f; // 工业标准：约 1/4 格宽

static Direction _Internal_CalcDirection(float dx, float dy)
{
    if (dx == 0 && dy == 0)
        return DIR_NONE;

    // 1. 组合键产生的“单轴”偏移 (归一化后 dx 或 dy 必有一个为 0)
    if (dx == 0 && dy < 0)
        return DIR_NE; // 东北 (W+D)
    if (dx > 0 && dy == 0)
        return DIR_SE; // 东南 (S+D)
    if (dx == 0 && dy > 0)
        return DIR_SW; // 西南 (S+A)
    if (dx < 0 && dy == 0)
        return DIR_NW; // 西北 (W+A)

    // 2. 单按键产生的“双轴”偏移
    if (dx < 0 && dy < 0)
        return DIR_N; // 北 (W)
    if (dx > 0 && dy > 0)
        return DIR_S; // 南 (S)
    if (dx > 0 && dy < 0)
        return DIR_E; // 东 (D)
    if (dx < 0 && dy > 0)
        return DIR_W; // 西 (A)

    return DIR_NONE;
}

void Player_Init(Player *player)
{
    player->gridX = 2.0f;
    player->gridY = 2.0f;
    player->speed = 4.0f;
    player->isMoving = false;
    player->dir = DIR_S;
}

void Player_Update(Player *self, GameMap *map, float dt, const bool *keys)
{
    float dx = 0, dy = 0;

    // --- 1. 输入采集 (基于我们之前拨乱反正后的映射) ---
    if (keys[SDL_SCANCODE_W])
    {
        dx -= 1.0f;
        dy -= 1.0f;
    }
    if (keys[SDL_SCANCODE_S])
    {
        dx += 1.0f;
        dy += 1.0f;
    }
    if (keys[SDL_SCANCODE_D])
    {
        dx += 1.0f;
        dy -= 1.0f;
    }
    if (keys[SDL_SCANCODE_A])
    {
        dx -= 1.0f;
        dy += 1.0f;
    }

    float len = sqrtf(dx * dx + dy * dy);
    if (len > 0.0f)
    {
        dx /= len;
        dy /= len;

        // 计算这一帧理想的位移量
        float moveGridX = dx * self->speed * dt;
        float moveGridY = dy * self->speed * dt;

        // --- 1. X 轴方向检测 ---
        float nextGridX = self->gridX + moveGridX;
        // 关键：根据移动方向检查“边缘点”
        float checkGridX = nextGridX + (moveGridX > 0 ? PLAYER_RADIUS : -PLAYER_RADIUS);

        // 工业级精细化：不仅检查脚尖，还要检查身体两侧（Y轴上下偏移）
        int cellX = (int)floorf(checkGridX);
        int cellY_side1 = (int)floorf(self->gridY - 0.2f);
        int cellY_side2 = (int)floorf(self->gridY + 0.2f);

        if (Map_GetSafeObject(map, cellX, cellY_side1) == 0 &&
            Map_GetSafeObject(map, cellX, cellY_side2) == 0)
        {
            self->gridX = nextGridX; // X轴通路，更新坐标
        }

        // --- 2. Y 轴方向检测 ---
        float nextGridY = self->gridY + moveGridY;
        float checkGridY = nextGridY + (moveGridY > 0 ? PLAYER_RADIUS : -PLAYER_RADIUS);

        // 判定目标格子的坐标 (取整)
        int cellY = (int)floorf(checkGridY);
        int cellX_side1 = (int)floorf(self->gridX - 0.2f);
        int cellX_side2 = (int)floorf(self->gridX + 0.2f);

        if (Map_GetSafeObject(map, cellX_side1, cellY) == 0 &&
            Map_GetSafeObject(map, cellX_side2, cellY) == 0)
        {
            self->gridY = nextGridY; // Y轴通路，更新坐标
        }

        self->dir = _Internal_CalcDirection(dx, dy);

        // --- 核心：更新动画 ---
        self->animTimer += dt;
        if (self->animTimer >= 0.15f)
        { // 每 0.15 秒换一帧
            self->currentFrame = (self->currentFrame + 1) % 4;
            self->animTimer = 0.0f;
        }

        self->isMoving = true;
    }
    else
    {
        self->isMoving = false;
        self->currentFrame = 0; // 停止时回到站立帧
    }
}