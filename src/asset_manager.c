#include <asset_manager.h>
#include <config.h>
#include <SDL3_image/SDL_image.h>

static TextureHandle GetOrLoadTexture(AssetManager *am, const char *path);

// 目录是否存在
static bool DirExists(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

/**
 * 解析资源目录。
 * 以前 ASSET_DIR 是相对“当前工作目录”的：双击 out/game.exe 时工作目录变成 out/，
 * 所有贴图都会静默加载失败。这里按三种常见布局依次探测：
 *   1. exe 同级          —— 发布时把 assets 和 exe 放在一起
 *   2. exe 的上一级       —— 构建输出在 out/ 时的工程结构
 *   3. 当前工作目录       —— 从仓库根目录运行 / 调试器指定了 cwd
 * @param out 输出缓冲
 * @param size 缓冲大小
 */
static void ResolveAssetRoot(char *out, size_t size)
{
    char base[512] = {0};
    const char *raw = SDL_GetBasePath(); // 形如 C:\...\out\，可能为 NULL
    if (raw)
    {
        SDL_strlcpy(base, raw, sizeof(base));
        SDL_free((void *)raw); // SDL_GetBasePath 的返回值需要自己释放

        size_t n = SDL_strlen(base);
        if (n > 0 && base[n - 1] != '/' && base[n - 1] != '\\')
            SDL_strlcat(base, "/", sizeof(base));
    }

    const char *prefixes[] = {"", "../"};
    for (int i = 0; i < 2; i++)
    {
        SDL_snprintf(out, size, "%s%s%s", base, prefixes[i], ASSET_DIR);
        if (DirExists(out))
            return;
    }

    // 兜底：保持旧行为（相对工作目录），找不到就让调用方报错
    SDL_snprintf(out, size, "%s", ASSET_DIR);
}

/**
 * 创建 AssetManager 实例，初始化纹理和素材定义的动态数组。
 * @param renderer SDL_Renderer 指针，用于后续纹理加载和渲染操作
 * @return 指向新创建的 AssetManager 实例的指针，如果创建失败则返回 NULL
 */
AssetManager *Asset_Create(SDL_Renderer *renderer)
{
    AssetManager *am = SDL_malloc(sizeof(AssetManager));
    if (!am)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create AssetManager: %s", SDL_GetError());
        return NULL;
    }

    SDL_zerop(am);
    am->renderer = renderer;
    am->texture_capacity = TEXTURE_CAPACITY;
    am->texture_pool = SDL_calloc(am->texture_capacity, sizeof(TextureResource));
    am->def_capacity = DEF_CAPACITY;
    am->defs = SDL_calloc(am->def_capacity, sizeof(AssetDef));
    am->texture_count = 0;
    am->def_count = 0;
    am->missing_assets = 0;

    ResolveAssetRoot(am->asset_root, sizeof(am->asset_root));
    SDL_Log("Asset root: %s", am->asset_root);
    if (!DirExists(am->asset_root))
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Asset root not found: %s (tried exe dir, parent, cwd)", am->asset_root);

    return am;
}

/**
 * 注册素材定义并关联纹理资源。
 * 此函数将素材定义添加到 AssetManager 中，并确保相关纹理已加载。
 * @param am 指向 AssetManager 的指针
 * @param id 素材的逻辑 ID，用于后续查询
 * @param filename 关联的纹理文件名（相对于 ASSET_DIR），函数内部会调用 Asset_LoadTexture 来加载纹理并获取索引
 * @param x 素材在图集中的 X 坐标
 * @param y 素材在图集中的 Y 坐标
 * @param w 素材在图集中的宽度
 * @param h 素材在图集中的高度
 * @param px 素材的锚点 X 坐标（0.0~1.0），即素材里“贴地”的那个点
 * @param py 素材的锚点 Y 坐标（0.0~1.0）
 * @param ax 格子内锚点偏移 X（单位：格，(0,0) 是菱形顶点，(0.5,0.5) 是格子中心）
 * @param ay 格子内锚点偏移 Y（单位：格）
 * @param frame_count 如果素材是动画，帧数；如果不是动画，传 1
 * @param frame_duration 每帧持续时间（单位: 秒，例如 0.1
 * @param frames_per_row 图集中每一行的帧数，用于自动计算动画帧的 UV 偏移
 * @return void
 */
void Asset_Register(AssetManager *am, AssetID id, const char *filename,
                    float x, float y, float w, float h,
                    float px, float py,
                    float ax, float ay,
                    int frame_count, float frame_duration, int frames_per_row)
{
    if (am->def_count >= am->def_capacity)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "AssetDef capacity exceeded!");
        return;
    }

    TextureHandle h_tex = GetOrLoadTexture(am, filename);
    if (h_tex == INVALID_HANDLE)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load texture for AssetID %d", id);
        return;
    }

    // 填充定义
    AssetDef *d = &am->defs[am->def_count++];
    d->id = id;
    d->tex_h = h_tex;
    d->src_rect = (SDL_FRect){x, y, w, h};
    d->pivotX = px;
    d->pivotY = py;
    d->anchorX = ax;
    d->anchorY = ay;

    // 3. 动画逻辑处理
    d->is_animated = (frame_count > 1);
    d->frame_count = frame_count;
    d->frame_duration = frame_duration;

    // 工业级防御性编程：如果传入 0，自动设为和 frame_count 一致（单行模式）
    d->frames_per_row = (frames_per_row > 0) ? frames_per_row : frame_count;
}

/**
 * 通过 ID 获取素材定义的只读指针，适用于频繁访问的场景，如渲染循环中的每帧调用。
 * 返回指针而非复制结构体，避免了不必要的内存分配和数据复制，提高性能。
 * 调用者必须保证在使用返回的指针期间，AssetManager 不被销毁或修改，否则可能导致悬空指针问题。
 * @param am 指向 AssetManager 的指针
 * @param id 要查询的 AssetID
 * @return 指向 AssetDef 的指针，如果未找到则返回 NULL
 */
const AssetDef *Asset_GetDef(AssetManager *am, AssetID id)
{
    for (int i = 0; i < am->def_count; i++)
    {
        if (am->defs[i].id == id)
            return &am->defs[i];
    }
    return NULL;
}

/**
 * 通过纹理句柄获取原始 SDL_Texture 指针，供渲染系统使用。
 * 调用者必须保证传入的纹理句柄有效且对应的纹理资源未被卸载，否则可能导致访问无效内存。
 * @param am 指向 AssetManager 的指针
 * @param h 纹理句柄，即 AssetDef 中的 tex_h 字段
 * @return 指向 SDL_Texture 的指针，如果句柄无效则返回 NULL
 */
SDL_Texture *Asset_GetRawTexture(AssetManager *am, TextureHandle h)
{
    if (h < 0 || h >= am->texture_count)
        return NULL;
    return am->texture_pool[h].texture;
}

/**
 * 私有函数：加载纹理并返回其在 AssetManager 中的索引。
 * @param am 指向 AssetManager 的指针
 * @param filename 纹理文件名（相对于启动时解析出的资源目录 asset_root）
 * @return 纹理在 AssetManager 中的索引，如果加载失败则返回 -1
 */
static TextureHandle GetOrLoadTexture(AssetManager *am, const char *filename)
{
    char path[512];
    SDL_snprintf(path, sizeof(path), "%s%s", am->asset_root, filename);

    // 1. 查找是否已存在
    for (int i = 0; i < am->texture_count; i++)
    {
        if (SDL_strcmp(am->texture_pool[i].path, path) == 0)
        {
            am->texture_pool[i].ref_count++;
            SDL_Log("Asset Info: Texture already loaded: %s (ref_count=%d)", path, am->texture_pool[i].ref_count);
            return (TextureHandle)i;
        }
    }

    // 2. 容错检查
    if (am->texture_count >= am->texture_capacity)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Asset Error: Texture pool capacity exceeded");
        return INVALID_HANDLE;
    }

    // 3. 真正调用 SDL_image 加载
    SDL_Texture *texture = IMG_LoadTexture(am->renderer, path);
    if (!texture)
    {
        am->missing_assets++;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Asset Error: Failed to load %s", path);
        return INVALID_HANDLE;
    }

    // 4. 添加到纹理池
    int index = am->texture_count++;
    am->texture_pool[index].texture = texture;
    SDL_strlcpy(am->texture_pool[index].path, path, sizeof(am->texture_pool[index].path));
    am->texture_pool[index].ref_count = 1;

    return index;
}

/**
 * 销毁 AssetManager 实例，释放所有关联的纹理资源和定义数组。
 * @param am 指向要销毁的 AssetManager 的指针
 * @return void
 */
void Asset_Destroy(AssetManager *am)
{
    if (!am)
        return;
    // 销毁所有纹理
    for (int i = 0; i < am->texture_count; i++)
    {
        if (am->texture_pool[i].texture)
        {
            SDL_DestroyTexture(am->texture_pool[i].texture);
        }
    }
    SDL_free(am->texture_pool);
    SDL_free(am->defs);
    SDL_free(am);
}