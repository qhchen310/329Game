#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <SDL3/SDL.h>
#include <config.h>

typedef int AssetID;
typedef int TextureHandle;

#define INVALID_HANDLE -1

// 注册静态素材（默认锚定在格子顶点，适合地砖）
#define Asset_RegisterStatic(am, id, file, x, y, w, h, px, py) \
    Asset_Register(am, id, file, x, y, w, h, px, py, 0.0f, 0.0f, 1, 0.0f, 1)

// 注册整张图的静态素材，并显式声明它在格子里的锚点 (ax, ay)，单位是“格”。
// 例：地砖 (0,0) 锚在菱形顶点；树/建筑 (0.5,0.5) 锚在格子中心。
#define Asset_RegisterCell(am, id, file, w, h, px, py, ax, ay) \
    Asset_Register(am, id, file, 0, 0, w, h, px, py, ax, ay, 1, 0.0f, 1)

// 注册简单单行动画
#define Asset_RegisterAnim(am, id, file, x, y, w, h, px, py, count, dur) \
    Asset_Register(am, id, file, x, y, w, h, px, py, 0.0f, 0.0f, count, dur, count)

// 原始纹理资源
typedef struct
{
    SDL_Texture *texture;
    char path[512];
    int ref_count; // 引用计数，用于资源卸载
} TextureResource;

// 逻辑素材定义（数据驱动的核心）

typedef struct
{
    AssetID id;           // 逻辑 ID
    TextureHandle tex_h;  // 指向纹理池的索引
    SDL_FRect src_rect;   // 在图集中的坐标
    float pivotX, pivotY; // 图像内的锚点 (0.0~1.0)：素材里“贴地”的那个点

    // --- 格子锚点 ---
    // 素材锚点相对格子起点的偏移，单位是“格”（不是像素）。
    // (0,0)   = 菱形顶点，地砖就是这种；直接铺满自己的格子。
    // (0.5,0.5) = 格子中心，站立的物体/角色用这种，脚踩在格子正中间。
    // 它在投影前参与运算，所以相机缩放时偏移会一起缩放，不会脱节。
    float anchorX, anchorY;

    // --- 物理属性 ---
    // true = 阻挡通行。物体层用它推导碰撞，不再有第二套判断标准。
    // 默认 false（可走），树/墙这类记得 Asset_SetSolid(am, id, true)。
    bool solid;

    // --- 动画核心属性 ---
    bool is_animated;     // 是否为动画素材
    int frame_count;      // 总帧数
    float frame_duration; // 每帧持续时间 (单位: 秒，例如 0.1f 代表 10FPS)
    int frames_per_row;   // 图集中每一行有多少帧 (用于自动计算下一帧的 UV 偏移)
    bool loop;            // 是否循环播放
} AssetDef;

// 管理器主体
typedef struct
{
    SDL_Renderer *renderer;

    // 启动时解析出来的资源目录（绝对路径，带结尾分隔符）。
    // 不再依赖“当前工作目录”，双击 exe、从任意目录启动都能找到贴图。
    char asset_root[512];

    TextureResource *texture_pool; // 纹理资源池
    int texture_count;             // 当前加载的纹理数量
    int texture_capacity;          // 纹理数组的容量

    AssetDef *defs;   // 逻辑素材定义数组
    int def_count;    // 当前定义的素材数量
    int def_capacity; // 定义数组的容量

    int missing_assets; // 加载失败的素材数量（>0 说明资源没找齐）
} AssetManager;

AssetManager *Asset_Create(SDL_Renderer *renderer);
void Asset_Register(AssetManager *am, AssetID id, const char *filename, float x, float y, float w, float h, float px, float py, float ax, float ay, int frame_count, float frame_duration, int frames_per_row);
const AssetDef *Asset_GetDef(const AssetManager *am, AssetID id);
void Asset_SetSolid(AssetManager *am, AssetID id, bool solid);
SDL_Texture *Asset_GetRawTexture(AssetManager *am, TextureHandle h);
void Asset_Destroy(AssetManager *am);

#endif