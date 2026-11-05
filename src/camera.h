#ifndef CAMERA_H
#define CAMERA_H

#include <SDL3/SDL.h>

typedef struct
{
    float tileWidth, tileHeight;
    float offsetX, offsetY; // 当前相机的世界坐标偏移 (以像素为单位)
    float screenX, screenY; // 当前相机的屏幕偏移
    float targetX, targetY; // 相机想要去的目标位置
    float lerpSpeed;        // 跟随平滑度 (如 0.1f, 越小越肉，越大越硬)
    float zoom;             // 缩放倍率 (默认 1.0f)
    float targetZoom;       // 目标缩放倍率 (用于插值)

    // 可视区域允许覆盖的世界坐标范围，防止镜头越出地图看到黑边
    bool hasBounds;
    float minWorldX, minWorldY, maxWorldX, maxWorldY;
} Camera;

Camera *Camera_Create(float tileWidth, float tileHeight, float startX, float startY);
// 设置相机允许看到的世界范围（一般就是地图的世界 AABB）
void Camera_SetBounds(Camera *self, float minX, float minY, float maxX, float maxY);
SDL_FPoint Camera_GridToScreen(const Camera *self, float gx, float gy);
// 反向方法：屏幕像素坐标 -> 逻辑网格坐标 (用于鼠标点击判定)
void Camera_ScreenToGrid(const Camera *self, float sx, float sy, float *gx, float *gy);
void Camera_LookAt(Camera *self, float target_gx, float target_gy, int windowW, int windowH);
void Camera_Snap(Camera *cam);
void Camera_Update(Camera *self, float dt);
void Camera_Destroy(Camera *self);

#endif