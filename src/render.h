#ifndef RENDERER_H
#define RENDERER_H

#include <SDL3/SDL.h>
#include <tile.h>
#include <camera.h>
#include <asset_manager.h>

// 渲染层级枚举，数值越大越靠前
typedef enum
{
    LAYER_BACKGROUND = 0, // 背景层（如远景图）
    LAYER_GROUND = 10,    // 地面层（地砖）
    LAYER_OBJECT = 20,    // 物体层（角色、树木、建筑）
    LAYER_EFFECT = 30,    // 特效层（粒子、打击火花）
    LAYER_UI = 40         // 界面层（血条、菜单）
} RenderLayer;

// 渲染请求结构体（用于后续可能的 Z-Order 排序扩展）
typedef struct
{
    AssetID id;        // 素材 ID，用于查询 AssetDef 获取纹理和切割信息
    float x;           // 世界坐标 (以像素为单位)
    float y;           // 世界坐标 (以像素为单位)
    float scale;       // 缩放倍率（如果需要）
    float angle;       // 旋转角度（如果需要）
    int z_index;       // Z-Index 用于渲染排序，数值越大越靠前
    SDL_FlipMode flip; // 水平/垂直翻转
} RenderCommand;

void Render_Init(int capacity);
void Render_Begin(SDL_Renderer *renderer);
void Render_Object_Push(AssetID id, float x, float y, float scale, float angle, int z_index, SDL_FlipMode flip);
void Render_Flush(AssetManager *am, SDL_Renderer *renderer, float total_time);
void Render_Destroy(void);

#endif