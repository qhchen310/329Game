#include "camera.h"
#include <stdlib.h>
#include <iso_math.h>

Camera *Camera_Create(float tileWidth, float tileHeight, float startX, float startY)
{
    Camera *cam = (Camera *)malloc(sizeof(Camera));
    if (!cam)
        return NULL;

    cam->tileWidth = tileWidth;
    cam->tileHeight = tileHeight;
    cam->offsetX = startX;
    cam->offsetY = startY;
    cam->zoom = 1.0f;

    return cam;
}

SDL_FPoint Camera_GridToScreen(const Camera *self, float gx, float gy)
{
    SDL_FPoint screenPos;
    screenPos.x = WORLD_TO_SCREEN_X(GRID_TO_WORLD_X(gx, gy), self);
    screenPos.y = WORLD_TO_SCREEN_Y(GRID_TO_WORLD_Y(gx, gy), self);

    return screenPos;
}

void Camera_ScreenToGrid(const Camera *self, float sx, float sy, float *gx, float *gy)
{
    *gx = WORLD_TO_GRID_X(SCREEN_TO_WORLD_X(sx, self), SCREEN_TO_WORLD_Y(sy, self));
    *gy = WORLD_TO_GRID_Y(SCREEN_TO_WORLD_X(sx, self), SCREEN_TO_WORLD_Y(sy, self));
}

/**
 * 让相机锁定目标点
 * @param gx, gy 目标的逻辑网格坐标（通常是玩家的脚下）
 * @param windowW, windowH 窗口宽高（用于居中计算）
 */
void Camera_LookAt(Camera *self, float gx, float gy, int windowW, int windowH)
{
    // @todo zoom 也应该影响目标位置的计算，否则放大时相机会“飘”起来
    // 1. 计算目标点在“零偏移”状态下的原始屏幕位置
    // 也就是：如果 screenX/Y 是 0，这个点在哪？
    float isoX = (gx - gy) * (self->tileWidth / 2.0f);
    float isoY = (gx + gy) * (self->tileHeight / 2.0f);

    // 2. 为了让目标点出现在窗口中心 (windowW/2, windowH/2)
    // 偏移量 = 窗口中心 - 目标原始位置
    self->targetX = (windowW / 2.0f) - isoX;
    self->targetY = (windowH / 2.0f) - isoY;
}

/**
 * 每一帧调用的平滑更新
 */
void Camera_Update(Camera *self, float dt)
{
    // @todo zoom 也应该参与插值计算，否则缩放时相机会“跳”起来
    // 线性插值公式：当前 = 当前 + (目标 - 当前) * 灵敏度
    // 这里的 5.0f 是随时间调整的权重，数值越大跟随越快
    float interpolation = 5.0f * dt;
    if (interpolation > 1.0f)
        interpolation = 1.0f;

    self->screenX += (self->targetX - self->screenX) * interpolation;
    self->screenY += (self->targetY - self->screenY) * interpolation;
}

void Camera_Destroy(Camera *self)
{
    if (self)
    {
        free(self);
    }
}