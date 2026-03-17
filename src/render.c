#include "render.h"
#include <stdlib.h>
#include <string.h>
#include <iso_math.h>
#include <SDL3_image/SDL_image.h>

static RenderCommand *queue = NULL;
static size_t queue_capacity = 0;
static size_t queue_size = 0;

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
 * @return void
 */
void Render_Begin(SDL_Renderer *renderer)
{
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
    SDL_RenderClear(renderer);
    queue_size = 0; // 重置队列
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
        .id = id, .x = x, .y = y, .scale = scale, .angle = angle, .z_index = z_index, .flip = flip};
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
    // 1. 按照 z_index 排序
    qsort(queue, queue_size, sizeof(RenderCommand), CompareCommands);

    // 2. 遍历并执行真正的渲染
    for (int i = 0; i < queue_size; i++)
    {
        RenderCommand *cmd = &queue[i];
        const AssetDef *def = Asset_GetDef(am, cmd->id);
        if (!def)
            continue;

        SDL_FRect current_src = def->src_rect;

        if (def->is_animated && def->frame_count > 1)
        {
            // 计算当前处于第几帧
            int frame_idx = (int)(total_time / def->frame_duration) % def->frame_count;

            // 处理网格图集 (Atlas) 的 UV 偏移
            int col = frame_idx % def->frames_per_row;
            int row = frame_idx / def->frames_per_row;

            current_src.x = def->src_rect.x + (col * def->src_rect.w);
            current_src.y = def->src_rect.y + (row * def->src_rect.h);
        }

        float draw_w = def->src_rect.w * cmd->scale;
        float draw_h = def->src_rect.h * cmd->scale;

        SDL_FRect dst_rect = {
            cmd->x - (def->pivotX * draw_w),
            cmd->y - (def->pivotY * draw_h),
            draw_w,
            draw_h};

        SDL_FPoint center = {def->pivotX * draw_w, def->pivotY * draw_h};
        SDL_Texture *tex = Asset_GetRawTexture(am, def->tex_h);
        SDL_RenderTextureRotated(renderer, tex, &def->src_rect, &dst_rect, cmd->angle, &center, cmd->flip);
    }

    // 3. 提交显示
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