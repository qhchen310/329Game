#include "render.h"
#include <stdlib.h>
#include <string.h>
#include <iso_math.h>
#include <SDL3_image/SDL_image.h>

static RenderCommand *queue = NULL;
static size_t queue_capacity = 0;
static size_t queue_size = 0;
static float frame_zoom = 1.0f; // 本帧的相机缩放，Render_Begin 时设置

static int CompareCommands(const void *a, const void *b);

/**
 * 比较函数用于根据 Z-Index 对渲染命令进行排序
 * @param a 第一个渲染命令
 * @param b 第二个渲染命令
 * @return 负数如果 a 在 b 前，正数如果 a 在 b 后，0 如果相等
 * @note 目前使用简单的冒泡排序，后续可以替换为更高效的排序算法（如 qsort）
 */
static int CompareCommands(const void *a, const void *b)
{
    const RenderCommand *cmdA = (const RenderCommand *)a;
    const RenderCommand *cmdB = (const RenderCommand *)b;

    // 第一级排序：按逻辑层级（枚举值）
    // 如果一个是地砖 (10)，一个是人 (20)，人永远在后面画（盖在地上）
    if (cmdA->z_index != cmdB->z_index)
    {
        return cmdA->z_index - cmdB->z_index;
    }

    // 第二级排序：同层级内按 Y 坐标（2.5D 深度）
    // 如果两个人都在 LAYER_OBJECT 层，谁在下面谁后画
    if (cmdA->y < cmdB->y)
        return -1;
    if (cmdA->y > cmdB->y)
        return 1;

    return 0;
}

/**
 * 渲染器模块实现
 * @param capacity 渲染队列的最大容量
 * @return void
 */
void Render_Init(int capacity)
{
    queue_capacity = capacity;
    queue = SDL_malloc(sizeof(RenderCommand) * capacity);
    if (!queue)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate render queue");
        return;
    }
}

/**
 * 开始一个新的渲染帧，清空渲染队列
 * @param renderer SDL_Renderer 对象
 * @param zoom 本帧的相机缩放。位置已经在 Camera_GridToScreen 里乘过 zoom 了，
 *             这里拿它把绘制尺寸也乘上，两者才不会脱节。
 * @return void
 */
void Render_Begin(SDL_Renderer *renderer, float zoom)
{
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
    SDL_RenderClear(renderer);
    queue_size = 0; // 重置队列
    frame_zoom = (zoom > 0.0f) ? zoom : 1.0f;
}

/**
 * 将一个渲染命令推入队列
 * @param id 素材 ID
 * @param x 世界坐标 X（像素）
 * @param y 世界坐标 Y（像素）
 * @param scale 缩放倍率
 * @param angle 旋转角度
 * @param z_index Z-Index 用于渲染排序，数值越大越靠前
 * @param flip 水平/垂直翻转
 * @return void
 */
void Render_Object_Push(AssetID id, float x, float y, float scale, float angle, int z_index, SDL_FlipMode flip)
{
    if (queue_size >= queue_capacity)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Render Queue Full");
        return;
    }

    queue[queue_size++] = (RenderCommand){
        .id = id, .x = x, .y = y, .scale = scale, .angle = angle, .z_index = z_index, .flip = flip, .custom_src = {0}, .use_custom_src = false};
}

/**
 * 按“格子坐标”推入一个瓦片或物体。
 * 素材在格子里的落点完全由注册时声明的 anchor 决定（AssetDef::anchorX/anchorY），
 * 这里不出现任何写死的偏移；锚点在投影之前参与运算，所以相机缩放时不会脱节。
 * @param am 资源管理器（用来查素材定义）
 * @param cam 相机（负责格子坐标 -> 屏幕坐标的投影）
 * @param id 素材 ID
 * @param cx 格子 X（整数格）
 * @param cy 格子 Y（整数格）
 * @param z_index 渲染层级
 * @return void
 */
void Render_Tile_Push(AssetManager *am, const Camera *cam, AssetID id, int cx, int cy, int z_index)
{
    if (queue_size >= queue_capacity)
        return;

    const AssetDef *def = Asset_GetDef(am, id);
    if (!def)
        return;

    SDL_FPoint screenPos = Camera_GridToScreen(cam, (float)cx + def->anchorX, (float)cy + def->anchorY);
    Render_Object_Push(id, screenPos.x, screenPos.y, 1.0f, 0.0f, z_index, SDL_FLIP_NONE);
}

/**
 * 将一个实体渲染命令推入队列，自动计算动画帧的切片区域
 * @param am 资源管理器
 * @param id 素材 ID
 * @param x,y 屏幕坐标（锚点）
 * @param scale 缩放倍率（和素材定义里的源尺寸相乘得到实际绘制尺寸）
 * @param dir 方向枚举值，用作图集的行号
 * @param current_frame 当前动画帧，用作图集的列号
 * @param z_index Z-Index 用于渲染排序，数值越大越靠前
 * @return void
 * @note src_rect 就是“这一帧在纹理里的真实像素范围”，所以图集带边距/留白也能切对；
 *       帧与帧之间按 src_rect 的宽高紧密排列。绘制尺寸 = src_rect 宽高 × scale。
 */
void Render_Entity_Push(AssetManager *am, AssetID id, float x, float y, float scale, int dir, int current_frame, int z_index)
{
    if (queue_size >= queue_capacity)
        return;

    const AssetDef *def = Asset_GetDef(am, id);
    if (!def)
        return;

    // 直接使用注册时声明的源矩形，不再用“纹理尺寸 / 逻辑尺寸”反推比例。
    // 旧写法只对“整张图恰好等于帧网格、且从 (0,0) 开始”成立，带偏移或留白就会算歪。
    SDL_FRect src = def->src_rect;

    // 多方向图集：列来自 current_frame，行来自 dir，步长就是一帧的宽高
    if (def->is_animated)
    {
        int cols = (def->frames_per_row > 0) ? def->frames_per_row : 1;
        int col = current_frame % cols;
        int row = dir;

        src.x += col * def->src_rect.w;
        src.y += row * def->src_rect.h;
    }

    // 填入渲染命令队列
    RenderCommand *cmd = &queue[queue_size++];
    cmd->id = id;
    cmd->x = x;
    cmd->y = y;
    cmd->scale = scale;
    cmd->angle = 0.0f;
    cmd->z_index = z_index;
    cmd->flip = SDL_FLIP_NONE;
    cmd->custom_src = src;
    cmd->use_custom_src = true;
}

/**
 * 执行渲染队列中的所有命令，按照 Z-Index 和 Y 坐标排序后提交到 SDL_Renderer
 * @param am 资源管理器
 * @param renderer SDL_Renderer 对象
 * @param total_time 游戏运行的总时间（秒），用于动画帧计算
 * @return void
 */
void Render_Flush(AssetManager *am, SDL_Renderer *renderer, float total_time)
{
    // 1. 根据 Z-Index 和 Y 坐标对渲染命令进行排序，确保正确的渲染顺序
    qsort(queue, queue_size, sizeof(RenderCommand), CompareCommands);

    // 2. 纯粹的渲染管线循环，不带任何业务逻辑
    for (int i = 0; i < queue_size; i++)
    {
        RenderCommand *cmd = &queue[i];
        const AssetDef *def = Asset_GetDef(am, cmd->id);
        if (!def)
            continue;

        SDL_Texture *tex = Asset_GetRawTexture(am, def->tex_h);
        if (!tex)
            continue;

        // 决定源切片矩形：如果命令自带了算好的 src，直接用它；否则用资产定义默认的
        SDL_FRect src_rect = cmd->use_custom_src ? cmd->custom_src : def->src_rect;

        // 目标绘制尺寸：素材源尺寸 × 命令缩放 × 相机缩放
        // 位置在 Camera_GridToScreen 里已经乘过 zoom，这里不乘的话缩放一开就会脱节
        float draw_w = def->src_rect.w * cmd->scale * frame_zoom;
        float draw_h = def->src_rect.h * cmd->scale * frame_zoom;

        // 基于资产自带的 pivot 自动计算对齐偏移，无硬编码
        SDL_FRect dst_rect = {
            cmd->x - (def->pivotX * draw_w),
            cmd->y - (def->pivotY * draw_h),
            draw_w,
            draw_h};

        // 1:1 时把目标位置对齐到整像素：相机 lerp 出来的坐标是小数，
        // LINEAR 采样会让菱形边缘插出半透明，相邻地砖之间出现发丝缝
        if (frame_zoom > 0.999f && frame_zoom < 1.001f)
        {
            dst_rect.x = SDL_roundf(dst_rect.x);
            dst_rect.y = SDL_roundf(dst_rect.y);
        }

        SDL_FPoint center = {def->pivotX * draw_w, def->pivotY * draw_h};

        // 渲染输出
        SDL_RenderTextureRotated(renderer, tex, &src_rect, &dst_rect, cmd->angle, &center, cmd->flip);
    }

    // 3. 清空队列，重置计数器
    queue_size = 0;

    // 4. 最后提交渲染结果到屏幕
    SDL_RenderPresent(renderer);
}

/**
 * 销毁渲染器模块，释放渲染队列内存
 * @return void
 */
void Render_Destroy(void)
{
    if (queue)
    {
        SDL_free(queue);
        queue = NULL;
    }
}