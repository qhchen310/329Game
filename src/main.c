#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h>
#include "renderer.h"
#include "player.h"
#include "camera.h"
#include "tile.h"
#include "map.h"
#include "config.h"
#include "asset_manager.h"

// ---------------------------------------------------------
// 1. 定义应用全局状态 (AppState)
// ---------------------------------------------------------
typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    RenderQueue *renderQueue;
    AssetManager *assetManager;
    Camera *camera;
    GameMap *map;
    Player player;
    SDL_Texture *heroShadowTexture; // 阴影图集（如果需要）
    SDL_Texture *heroTexture;       // 英雄 8 方向图集
    uint64_t lastTime;
} AppState;

// ---------------------------------------------------------
// 2. 模拟英雄图集定义 (手动指定像素坐标和重心)
// ---------------------------------------------------------
SDL_Texture *CreateHeroAtlas(SDL_Renderer *renderer)
{
    // 8 行 (方向) x 4 列 (动画帧)，每格 64x64
    SDL_Surface *surf = SDL_CreateSurface(256, 512, SDL_PIXELFORMAT_RGBA8888);
    SDL_FillSurfaceRect(surf, NULL, 0x00000000); // 透明底

    // 循环画出 8 个方向的“英雄”占位图
    for (int dir = 0; dir < DIR_NONE; dir++)
    {
        for (int frame = 0; frame < 4; frame++)
        {
            // 计算当前帧在图集里的绝对起始位置
            int slotX = frame * TILE_WIDTH;
            int slotY = dir * TILE_HEIGHT;

            // 1. 画身体 (所有的英雄部位都限制在各自的 slot 内部)
            // 身体放在槽位下半部分，靠近脚底 (32, 64)
            SDL_Rect body = {slotX + 16, slotY + 24, 32, 32};
            SDL_FillSurfaceRect(surf, &body, SDL_MapRGB(SDL_GetPixelFormatDetails(surf->format), NULL, 50 + dir * 20, 100, 200));

            // 2. 画“朝向点”（眼睛/头灯）
            // 我们通过偏移来模拟 8 个方向的视觉差异
            int eyeX = slotX + 28;
            int eyeY = slotY + 28;

            // 根据方向枚举微调眼睛位置，确保它不出 64x64 范围
            if (dir == DIR_N)
                eyeY -= 10; // 北 (DIR_N)
            if (dir == DIR_S)
                eyeY += 10; // 南 (DIR_S)
            if (dir == DIR_E)
                eyeX += 10; // 东 (DIR_E)
            if (dir == DIR_W)
                eyeX -= 10; // 西 (DIR_W)

            SDL_Rect eye = {eyeX, eyeY, 8, 8};
            SDL_FillSurfaceRect(surf, &eye, SDL_MapRGB(SDL_GetPixelFormatDetails(surf->format), NULL, 255, 255, 0));
        }
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    return tex;
}

SDL_Texture *CreateHeroShadowAtlas(SDL_Renderer *renderer)
{
    int atlasH = DIR_NONE * TILE_HEIGHT;
    SDL_Surface *surf = SDL_CreateSurface(256, atlasH, SDL_PIXELFORMAT_RGBA8888);
    SDL_FillSurfaceRect(surf, NULL, 0x00000000);

    for (int dir = 0; dir < DIR_NONE; dir++)
    {
        for (int frame = 0; frame < 4; frame++)
        {
            int slotX = frame * TILE_WIDTH;
            int slotY = dir * TILE_HEIGHT;

            // 基础影子属性
            int sw = 40; // 宽度
            int sh = 20; // 高度
            int ox = 32; // 相对于 slot 的中心 X
            int oy = 56; // 相对于 slot 的中心 Y (脚底位置)

            // --- 核心：8方向几何适配逻辑 ---
            // 假设光源在“西北方向”
            switch (dir)
            {
            case DIR_N:
                sh = 12;
                oy = 52;
                ox = 32;
                break; // 正北：影子缩短在身后
            case DIR_S:
                sh = 28;
                oy = 60;
                ox = 32;
                break; // 正南：影子拉长在身前
            case DIR_E:
                sw = 45;
                sh = 18;
                ox = 38;
                break; // 正东：影子向右侧斜拉
            case DIR_W:
                sw = 35;
                sh = 18;
                ox = 26;
                break; // 正西：影子向左侧缩进
            case DIR_NE:
                sw = 42;
                sh = 14;
                ox = 36;
                oy = 54;
                break;
            case DIR_NW:
                sw = 30;
                sh = 12;
                ox = 30;
                oy = 52;
                break;
            case DIR_SE:
                sw = 45;
                sh = 24;
                ox = 36;
                oy = 58;
                break;
            case DIR_SW:
                sw = 35;
                sh = 22;
                ox = 28;
                oy = 58;
                break;
            }

            // 绘制半透明阴影（工业标准：使用黑色且 Alpha 值约为 100）
            SDL_Rect sRect = {slotX + ox - sw / 2, slotY + oy - sh / 2, sw, sh};
            SDL_FillSurfaceRect(surf, &sRect, SDL_MapRGBA(SDL_GetPixelFormatDetails(surf->format), NULL, 0, 0, 0, 110));
        }
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    return tex;
}

// ---------------------------------------------------------
// 3. 配置地砖库 (统一重心 Pivot)
// ---------------------------------------------------------
void SetupTileLibrary(AppState *as)
{
    const float SLOT = TILE_WIDTH;
    const float PIVOT_X = TILE_WIDTH / 2.0f; // 宽度一半

    // ID 0: 草地 - 虽然内容只有 32 高，但我们在 64x64 槽位里定义它
    Tile_Define(TILE_GRASS, 0, 0, SLOT, SLOT, PIVOT_X, 0, true);

    // 注册草地地砖 (静态)
    Asset_RegisterDef(as->assetManager,
                      (AssetDef){.id = TILE_GRASS, .src_rect = {0, 0, SLOT, SLOT}, .pivotX = PIVOT_X, .pivotY = 0.0f, .is_animated = false},
                      "atlas_tiles.png");
}

// ---------------------------------------------------------
// 4. SDL_AppInit
// ---------------------------------------------------------
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error initializing SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState *as = (AppState *)SDL_calloc(1, sizeof(AppState));
    if (!as)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error allocating memory for AppState");
        return SDL_APP_FAILURE;
    }

    *appstate = as;

    if (!SDL_CreateWindowAndRenderer(TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0, &as->window, &as->renderer))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window/Renderer Create Failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!Renderer_Init(as->renderer, QUEUE_INITIAL_CAPACITY))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Renderer_Init (Loading PNG) Failed!");
        return SDL_APP_FAILURE;
    }

    if (!(as->assetManager = Asset_Create(as->renderer)))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Asset Manager Creation Failed!");
        return SDL_APP_FAILURE;
    }

    // 初始化地图
    as->map = Map_Create(MAP_WIDTH, MAP_HEIGHT); // 地图稍微大一点

    // 初始化玩家
    Player_Init(&as->player);

    as->camera = Camera_Create(TILE_WIDTH, TILE_HEIGHT, -WINDOW_WIDTH / 2, 0); // 初始偏移到窗口中心
    // 让相机瞄准玩家
    Camera_LookAt(as->camera, as->player.gridX, as->player.gridY, WINDOW_WIDTH, WINDOW_HEIGHT);
    // 直接对齐，避免开场动画
    Camera_Snap(as->camera);

    // 初始化时间戳
    as->lastTime = SDL_GetTicks();

    // 生成并定义图集
    SetupTileLibrary(as);

    as->heroShadowTexture = CreateHeroShadowAtlas(as->renderer); // 新的英雄阴影图集
    if (!as->heroShadowTexture)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Hero Shadow Atlas Texture Create Failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // 生成英雄图集（8 方向，每个方向 4 帧动画）
    as->heroTexture = CreateHeroAtlas(as->renderer); // 新的 8 方向英雄图集
    if (!as->heroTexture)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Hero Atlas Texture Create Failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // 填充地图数据
    for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++)
        as->map->groundLayer[i] = 0; // 全是草地

    SDL_Log("SDL_AppInit Success!");
    return SDL_APP_CONTINUE;
}

// ---------------------------------------------------------
// 5. SDL_AppIterate
// ---------------------------------------------------------
SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *as = (AppState *)appstate;

    // A. 时间步进
    uint64_t now = SDL_GetTicks();
    float dt = (now - as->lastTime) / 1000.0f;
    as->lastTime = now;

    // B. 更新玩家逻辑 (包含归一化、方向判定)
    const bool *keys = SDL_GetKeyboardState(NULL);
    Player_Update(&as->player, as->map, dt, keys);

    // C. 渲染流程开始
    SDL_SetRenderDrawColor(as->renderer, 15, 15, 15, 255);
    SDL_RenderClear(as->renderer);

    // // 1. 让相机瞄准玩家
    Camera_LookAt(as->camera, as->player.gridX, as->player.gridY, WINDOW_WIDTH, WINDOW_HEIGHT);

    // // 2. 执行平滑移动
    Camera_Update(as->camera, dt);

    // 1. 渲染地表层 (增加安全检查)
    for (int y = 0; y < as->map->height; y++)
    {
        for (int x = 0; x < as->map->width; x++)
        {
            int id = as->map->groundLayer[Map_GetIndex(x, y, as->map->width)];

            // --- 核心修正：检查 ID 范围和纹理是否存在 ---
            if (id < 0 || id >= TILE_COUNT)
                continue;
            const TileDef *tile = Tile_GetDef(id);
            Renderer_SubmitTile(as->camera, id, (float)x, (float)y, 0.0f);
        }
    }

    // 2. 收集物体层 (同样增加检查)
    for (int y = 0; y < as->map->height; y++)
    {
        for (int x = 0; x < as->map->width; x++)
        {
            int id = as->map->objectLayer[Map_GetIndex(x, y, as->map->width)];

            // --- 核心修正：检查 ID 范围和纹理是否存在 ---
            if (id < 0 || id >= TILE_COUNT)
                continue;

            const TileDef *tile = Tile_GetDef(id);
            Renderer_SubmitTile(as->camera, id, (float)x, (float)y, 0.0f);
        }
    }

    // 3.渲染玩家 (假设玩家图片 32x64, 重心在脚底 16, 64)
    SDL_FRect heroSrc = {(float)as->player.currentFrame * 64.0f, (float)as->player.dir * 64.0f, 64.0f, 64.0f};
    SDL_FPoint pPos = Camera_GridToScreen(as->camera, as->player.gridX, as->player.gridY);
    SDL_FRect pDst = {pPos.x - 32, pPos.y - 64, 64, 64}; // 使用统一重心
    // Renderer_Submit(as->renderQueue, as->heroShadowTexture, heroSrc, pDst, as->player.gridX + as->player.gridY - 0.01f);
    Renderer_Submit(as->heroTexture, heroSrc, pDst, as->player.gridX + as->player.gridY);

    // 4. 执行排序渲染
    RenderQueue_Sort();
    RenderQueue_DrawAll(as->renderer);

    SDL_RenderPresent(as->renderer);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    AppState *as = (AppState *)appstate;
    if (as)
    {
        Camera_Destroy(as->camera);
        Map_Destroy(as->map);
        RenderQueue_Destroy(as->renderQueue);
        SDL_free(as);
    }
}