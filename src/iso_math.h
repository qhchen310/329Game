#ifndef ISO_MATH_H
#define ISO_MATH_H

// --- 基础尺寸定义 ---
#define TILE_WIDTH 64.0f
#define TILE_HEIGHT 32.0f
#define HALF_W (TILE_WIDTH / 2.0f)
#define HALF_H (TILE_HEIGHT / 2.0f)

// 1. 逻辑格点 -> 世界像素坐标
#define GRID_TO_WORLD_X(gx, gy) (((gx) - (gy)) * HALF_W)
#define GRID_TO_WORLD_Y(gx, gy) (((gx) + (gy)) * HALF_H)

// 2. 世界像素 -> 逻辑格点
#define WORLD_TO_GRID_X(wx, wy) (((wx) / HALF_W + (wy) / HALF_H) / 2.0f)
#define WORLD_TO_GRID_Y(wx, wy) (((wy) / HALF_H - (wx) / HALF_W) / 2.0f)

// 3. 世界像素 -> 屏幕像素 (使用Camera 结构体)
// 公式：(世界坐标 - 相机当前坐标) * 相机缩放
#define WORLD_TO_SCREEN_X(wx, cam) (((wx) - (cam)->screenX) * (cam)->zoom)
#define WORLD_TO_SCREEN_Y(wy, cam) (((wy) - (cam)->screenY) * (cam)->zoom)

// 4. 屏幕像素 -> 世界像素 (用于鼠标拾取)
#define SCREEN_TO_WORLD_X(sx, cam) (((sx) / (cam)->zoom) + (cam)->screenX)
#define SCREEN_TO_WORLD_Y(sy, cam) (((sy) / (cam)->zoom) + (cam)->screenY)

#endif // ISO_MATH_H