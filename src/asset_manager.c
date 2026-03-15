#include <asset_manager.h>
#include <config.h>
#include <SDL3_image/SDL_image.h>

static int Asset_LoadTexture(AssetManager *am, const char *filename);

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

    am->renderer = renderer;
    am->texture_capacity = 16;
    am->textures = SDL_calloc(am->texture_capacity, sizeof(TextureResource));
    am->def_capacity = 128;
    am->defs = SDL_calloc(am->def_capacity, sizeof(AssetDef));
    am->texture_count = 0;
    am->def_count = 0;
    return am;
}

/**
 * 注册素材定义并关联纹理资源。
 * 此函数将素材定义添加到 AssetManager 中，并确保相关纹理已加载。
 * @param am 指向 AssetManager 的指针
 * @param def 要注册的 AssetDef 结构体，包含素材的逻辑定义
 * @param texName 关联的纹理文件名（相对于 ASSET_DIR），函数内部会调用 Asset_LoadTexture 来加载纹理并获取索引
 * @return void
 */
void Asset_RegisterDef(AssetManager *am, AssetDef def, const char *texName)
{
    int texIdx = Asset_LoadTexture(am, texName);
    if (texIdx == -1)
        return;

    def.texture_index = texIdx;
    am->defs[am->def_count++] = def; // 实际开发中需做动态扩容检查
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
 * 私有函数：加载纹理并返回其在 AssetManager 中的索引。
 * @param am 指向 AssetManager 的指针
 * @param filename 纹理文件名（相对于 ASSET_DIR）
 * @return 纹理在 AssetManager 中的索引，如果加载失败则返回 -1
 */
static int Asset_LoadTexture(AssetManager *am, const char *filename)
{
    // 1. 检查是否已经加载过
    for (int i = 0; i < am->texture_count; i++)
    {
        if (SDL_strcmp(am->textures[i].path, filename) == 0)
        {
            am->textures[i].ref_count++;
            return i;
        }
    }

    // 2. 执行加载 (假设路径在 assets/textures/)
    char fullPath[512];
    SDL_snprintf(fullPath, sizeof(fullPath), "%s%s", ASSET_DIR, filename);

    // 3. 加载纹理
    SDL_Texture *tex = IMG_LoadTexture(am->renderer, fullPath);
    if (!tex)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load texture '%s': %s", fullPath, SDL_GetError());
        return -1;
    }

    // 4. 存入池中
    int idx = am->texture_count++;
    am->textures[idx].texture = tex;
    SDL_strlcpy(am->textures[idx].path, filename, 256);
    am->textures[idx].ref_count = 1;
    return idx;
}