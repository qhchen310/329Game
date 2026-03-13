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
    cam->screenX = 0; // 初始屏幕偏移为 0，后续通过 LookAt 设置目标位置
    cam->screenY = 0; // 初始屏幕偏移为 0，后续通过 LookAt 设置目标位置
    cam->zoom = 1.0f;
    cam->targetZoom = 1.0f; // 初始目标缩放与当前缩放一致
    cam->lerpSpeed = 5.0f;  // 默认平滑度

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
void Camera_LookAt(Camera *self, float target_gx, float target_gy, int windowW, int windowH)
{
    self->targetX = GRID_TO_WORLD_X(target_gx, target_gy) - (windowW / 2.0f / self->targetZoom);
    self->targetY = GRID_TO_WORLD_Y(target_gx, target_gy) - (windowH / 2.0f / self->targetZoom);
}

/**
 * 瞬间移动：取消所有平滑动画，直接对齐
 * @param cam 相机指针
 */
void Camera_Snap(Camera *cam)
{
    cam->offsetX = cam->targetX;
    cam->offsetY = cam->targetY;
}

/**
 * 每一帧调用的平滑更新
 */
void Camera_Update(Camera *self, float dt)
{
    // 1. 计算插值权重 (基于 dt，确保帧率无关性)
    float interpolation = self->lerpSpeed * dt;

    if (interpolation > 1.0f)
        interpolation = 1.0f;

    // 1. 位置平滑更新
    self->offsetX += (self->targetX - self->offsetX) * interpolation;
    self->offsetY += (self->targetY - self->offsetY) * interpolation;

    // 2. 缩放平滑更新
    // 逻辑：当前 zoom 每一帧都向 targetZoom 靠近
    self->zoom += (self->targetZoom - self->zoom) * interpolation;
}

void Camera_Destroy(Camera *self)
{
    if (self)
    {
        free(self);
    }
}