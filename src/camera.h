#ifndef CAMERA_H
#define CAMERA_H

#include <SDL3/SDL.h>

typedef struct
{
    float tileWidth, tileHeight;
    float offsetX, offsetY;
    float screenX, screenY; // 当前相机的屏幕偏移
    float targetX, targetY; // 相机想要去的目标位置
    float lerpSpeed;        // 跟随平滑度 (如 0.1f, 越小越肉，越大越硬)
    float zoom;             // 缩放倍率 (默认 1.0f)
} Camera;

Camera *Camera_Create(float tileWidth, float tileHeight, float startX, float startY);
SDL_FPoint Camera_GridToScreen(const Camera *self, float gx, float gy);
// 反向方法：屏幕像素坐标 -> 逻辑网格坐标 (用于鼠标点击判定)
void Camera_ScreenToGrid(const Camera *self, float sx, float sy, float *gx, float *gy);
void Camera_LookAt(Camera *self, float gx, float gy, int windowW, int windowH);
void Camera_Update(Camera *self, float dt);
void Camera_Destroy(Camera *self);

#endif