#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h>
#include "render.h"
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
    AssetManager *assetManager;
    Camera *camera;
    GameMap *map;
    Player player;
    uint64_t lastTime;
} AppState;

// ---------------------------------------------------------
// 2. 定义函数原型
// ---------------------------------------------------------
void SetupTileLibrary(AppState *as)
{
    const float SLOT = TILE_WIDTH; // 128.0f
    const float PIVOT_X = 0.5f;
    const float PIVOT_Y = 0.0f;

    // 1. 注册原来的静态草地地砖
    Tile_Define(TILE_GRASS, 0.0f, 0.0f, SLOT, SLOT, PIVOT_X, PIVOT_Y);
    Asset_RegisterStatic(as->assetManager, (AssetID)TILE_GRASS, "tile_grass_6.png", 0, 0, SLOT, SLOT, PIVOT_X, PIVOT_Y);

    // 2. 注册新的高清英雄素材（单张静态图）
    Asset_Register(
        as->assetManager,
        100,             // AssetID
        "player_v1.png", // 对应的文件名（确保放到了你的 ASSET_DIR 目录下）
        0.0f, 0.0f,      // 图集中的起点 (x, y)
        SLOT, SLOT,      // 逻辑裁剪与原始宽高 (w, h)保持 1:1
        0.5f, 0.9375f,   // 灵魂锚点：水平居中，垂直靠脚底
        0.0f, 0.0f,      // 格子锚点：角色不用它（走 Entity_Push，用连续坐标）
        1,               // frame_count: 目前是单张静态图，传入 1
        0.0f,            // frame_duration: 无动画，传 0.0f
        1                // frames_per_row: 1 行
    );
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

    // 开垂直同步：把循环锁在刷新率上，别让 400 次绘制空转烧一个核
    if (!SDL_SetRenderVSync(as->renderer, 1))
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "VSync 开启失败: %s", SDL_GetError());

    Render_Init(QUEUE_INITIAL_CAPACITY);

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
    as->lastTime = SDL_GetTicksNS();

    // 生成并定义图集
    SetupTileLibrary(as);

    // 资源没找齐就别装作没事：给出可见提示并退出，避免“黑屏但没有任何报错”
    if (as->assetManager->missing_assets > 0)
    {
        char msg[512];
        SDL_snprintf(msg, sizeof(msg),
                     "有 %d 张贴图加载失败。\n\n当前资源目录：\n%s\n\n请把 assets 文件夹放在 exe 同级，或放到 exe 的上一级目录。",
                     as->assetManager->missing_assets,
                     as->assetManager->asset_root);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "资源加载失败", msg, as->window);
        return SDL_APP_FAILURE;
    }

    // 填充地图数据
    for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++)
        as->map->groundLayer[i] = TILE_GRASS; // 全是草地

    SDL_Log("SDL_AppInit Success!");
    return SDL_APP_CONTINUE;
}

// ---------------------------------------------------------
// 5. SDL_AppIterate
// ---------------------------------------------------------
SDL_AppResult SDL_AppIterate(void *appstate)
{
    // 时间步进
    static float total_time = 0.0f;

    AppState *as = (AppState *)appstate;

    uint64_t now = SDL_GetTicksNS();
    float deltaTime = (now - as->lastTime) / 1000000000.0f;
    as->lastTime = now;

    // 卡顿保护：一帧最多走 MAX_DELTA_TIME，避免大步长穿墙/相机瞬移
    if (deltaTime > MAX_DELTA_TIME)
        deltaTime = MAX_DELTA_TIME;

    total_time += deltaTime; // deltaTime 是上一帧消耗的秒数

    // 更新玩家逻辑 (包含归一化、方向判定)
    const bool *keys = SDL_GetKeyboardState(NULL);
    Player_Update(&as->player, as->map, deltaTime, keys);

    // 渲染流程开始
    SDL_SetRenderDrawColor(as->renderer, 15, 15, 15, 255);
    SDL_RenderClear(as->renderer);

    // 让相机瞄准玩家
    Camera_LookAt(as->camera, as->player.gridX, as->player.gridY, WINDOW_WIDTH, WINDOW_HEIGHT);

    // 执行平滑移动
    Camera_Update(as->camera, deltaTime);

    // 开始新的渲染帧
    Render_Begin(as->renderer);

    // 1. 渲染地表层
    for (int y = 0; y < as->map->height; y++)
    {
        for (int x = 0; x < as->map->width; x++)
        {
            int id = as->map->groundLayer[Map_GetIndex(x, y, as->map->width)];

            // 负数 = 空；素材是否存在交给 Render_Tile_Push 判断
            if (id < 0)
                continue;

            Render_Tile_Push(as->assetManager, as->camera, (AssetID)id, x, y, LAYER_GROUND);
        }
    }

    // 2. 收集物体层（和地表层同一套 ID：AssetID，-1 = 空）
    for (int y = 0; y < as->map->height; y++)
    {
        for (int x = 0; x < as->map->width; x++)
        {
            int id = as->map->objectLayer[Map_GetIndex(x, y, as->map->width)];

            if (id < 0)
                continue;

            Render_Tile_Push(as->assetManager, as->camera, (AssetID)id, x, y, LAYER_OBJECT);
        }
    }

    // 使用新的 Entity 推送函数，自动处理动画帧和图集切割
    // 这里我们还加了一个呼吸动画，使用正弦函数让玩家稍微放大缩小，增加生命力
    SDL_FPoint pPos = Camera_GridToScreen(as->camera, as->player.gridX, as->player.gridY);

    Render_Entity_Push(
        as->assetManager,
        100,         // 资产 ID
        pPos.x,      // 屏幕 X
        pPos.y,      // 屏幕 Y
        1,           // 把算好的 0.25+ 呼吸直接传进去！
        0,           // dir: 静态图传 0
        0,           // current_frame: 静态图传 0
        LAYER_OBJECT // 物体层
    );

    // 4. 执行排序渲染
    Render_Flush(as->assetManager, as->renderer, total_time);

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
        Render_Destroy();
        Asset_Destroy(as->assetManager);
        SDL_free(as);
    }
}