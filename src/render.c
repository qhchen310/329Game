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
 * @param x 世界坐标 X（像素）
 * @param y 世界坐标 Y（像素）
 * @param scale 缩放倍率
 * @param dir 方向枚举值（0~7），用于计算动画帧
 * @param current_frame 当前动画帧索引（0~帧数-1）
 * @param z_index Z-Index 用于渲染排序，数值越大越靠前
 * @return void
 * @note 这个函数是专门为角色等多方向动画设计的，它会根据传入的方向和当前帧自动计算出正确的切片区域，并将其封装到渲染命令中，极大简化了业务层的渲染调用！
 */
void Render_Entity_Push(AssetManager *am, AssetID id, float x, float y, float scale, int dir, int current_frame, int z_index)
{
    if (queue_size >= queue_capacity)
        return;

    const AssetDef *def = Asset_GetDef(am, id);
    if (!def)
        return;

    SDL_Texture *tex = Asset_GetRawTexture(am, def->tex_h);
    if (!tex)
        return;

    // 1. 获取图片真实的物理尺寸（例如 256）
    float texW, texH;
    SDL_GetTextureSize(tex, &texW, &texH);

    // 2. 计算图集坐标和物理像素之间的比例，考虑动画帧的行列布局
    // 例如：如果你的图集是 256x512，单帧逻辑尺寸是 64x64，且每行有 4 帧，那么 ratioX = 256 / (64 * 4) = 1.0，ratioY = 512 / (64 * 2) = 4.0
    float ratioX = texW / (def->src_rect.w * (def->is_animated ? def->frames_per_row : 1));
    float ratioY = texH / (def->src_rect.h * (def->is_animated ? (def->frame_count / def->frames_per_row) : 1));

    // 3. 基础物理原图切片大小
    SDL_FRect phys_src = {
        def->src_rect.x * ratioX,
        def->src_rect.y * ratioY,
        def->src_rect.w * ratioX,
        def->src_rect.h * ratioY};

    // 4. 如果是多方向帧动画，在这里根据业务层传来的 dir 和 frame 算偏移
    if (def->is_animated)
    {
        int col = current_frame % def->frames_per_row;
        int row = dir; // 垂直方向直接对应你的 Player 8方向枚举值 (0~7)

        phys_src.x += col * phys_src.w;
        phys_src.y += row * phys_src.h;
    }

    // 5. 填入渲染命令队列
    RenderCommand *cmd = &queue[queue_size++];
    cmd->id = id;
    cmd->x = x;
    cmd->y = y;
    cmd->scale = scale;
    cmd->angle = 0.0f;
    cmd->z_index = z_index;
    cmd->flip = SDL_FLIP_NONE;

    cmd->custom_src = phys_src; // 把算好的完美物理切片塞给命令
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

        // 目标绘制尺寸
        float draw_w = def->src_rect.w * cmd->scale;
        float draw_h = def->src_rect.h * cmd->scale;

        // 基于资产自带的 pivot 自动计算对齐偏移，无硬编码
        SDL_FRect dst_rect = {
            cmd->x - (def->pivotX * draw_w),
            cmd->y - (def->pivotY * draw_h),
            draw_w,
            draw_h};

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