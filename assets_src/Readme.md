# 角色素材管线（Blender → 图集 → 引擎）

这份文档记录了从 Blender 渲染到游戏里显示的完整流程。**改任何一环之前先读一遍**，尤其是「方向映射」和「规格」两节。

---

## 1. 规格（改之前要想清楚）

| 项目 | 值 | 说明 |
|---|---|---|
| 相机 | 正交，旋转 X=60°、Z=45°，正交缩放 2.6 | X=60 表示俯 30°（`sin30°=0.5` → 2:1 菱形） |
| 渲染分辨率 | 320 × 320 | 是单元格的 2 倍，缩小后有超采样效果 |
| 渲染格式 | PNG，RGBA，透明背景，色彩管理 Standard | 帧率 10 |
| 单元格 | 160 × 160 | 游戏里 1:1 显示 |
| 角色高度 | 96 px | 1.7 m 经 30° 俯视投影后约 190 px（320 图），缩一半即 96 |
| 脚底位置 | 单元格 y = 132 | `pivotY = 132/160 = 0.825`，必须是整数 |
| 图集 | 960 × 1280（**6 列 × 8 行**） | 行 = 朝向，列 = 走路帧 |
| 走路动画 | 6 帧，每帧 0.1 秒（10fps） | 和 Blender 场景帧率一致 |

> `pivot × 绘制尺寸` 必须是整数，否则 1:1 像素对齐时角色会相对地砖抖 1px。

---

## 2. 方向映射（最容易错）

**相机起始方位是 45°，所以渲染文件编号和游戏方向差 3。**

| 游戏方向 | 行号 | 渲染文件 |
|---|---|---|
| N（上） | 0 | `hero_3_01..06.png` |
| NE | 1 | `hero_4_01..06.png` |
| E | 2 | `hero_5_01..06.png` |
| SE | 3 | `hero_6_01..06.png` |
| S（下） | 4 | `hero_7_01..06.png` |
| SW | 5 | `hero_0_01..06.png` |
| W | 6 | `hero_1_01..06.png` |
| NW | 7 | `hero_2_01..06.png` |

这套映射由 `pack_sheet.py` 里的 `ROW_TO_FRAME = [3,4,5,6,7,0,1,2]` 实现——**行序在拼图集时就排好，引擎里 `dir` 直接当行号**，不需要偏移常量。

如果以后改了相机初始方位、或者换一台相机重新渲染，这个数组要重算。

---

## 3. 目录结构

```
assets_src/
  Readme.md                  ← 本文档
  hero/
    hero.blend               ← Blender 工程
    frames/                  ← Blender 原始渲染（320×320，8 方向 × 6 帧 = 48 张）
      hero_<方向>_<帧>.png ...
    pack_sheet.py            ← 拼图集脚本

assets/textures/hero/
  hero_sheet.png             ← 引擎加载的图集（960×1280，6 列 × 8 行）
  hero_sheet.json            ← 元数据（格子尺寸、pivot、行序）
```

---

## 4. 重新生成流程

### 4.1 Blender 渲染

打开 `hero.blend`，顶部工作区切到 **脚本**，运行：

```python
import bpy, math, os

scene = bpy.context.scene
rig = bpy.data.objects["CAM_RIG"]
out_dir = bpy.path.abspath("//frames")
os.makedirs(out_dir, exist_ok=True)
scene.render.fps = 10

for d in range(8):
    rig.rotation_euler[2] = math.radians(d * 45)
    for f in range(1, 7):                    # 6 帧走路
        scene.frame_set(f)
        scene.render.filepath = os.path.join(out_dir, "hero_%d_%02d.png" % (d, f))
        bpy.ops.render.render(write_still=True)

rig.rotation_euler[2] = 0
```

前提：
- `CAM_RIG` 是位于原点的空物体，摄像机和灯光都挂在它下面（转支点 = 相机和灯一起转，保证每个方向的光照在屏幕上一致）。
- 骨架 `HeroRig` 上已经做好 6 帧原地走路动画（帧 1~6，10fps）。

### 4.2 拼图集

在仓库根目录执行：

```bash
python assets_src/hero/pack_sheet.py
```

输出：`assets/textures/hero/hero_sheet.png` 和 `hero_sheet.json`。

### 4.3 引擎注册

`src/main.c` 的 `SetupTileLibrary()` 里：

```c
Asset_Register(
    as->assetManager,
    101,                    // AssetID
    "hero/hero_sheet.png",
    0, 0, 160, 160,         // 一帧的源矩形
    0.5f, 0.8125f,          // pivot：130/160，脚底
    0, 0,                   // 格子锚点不用
    6, 0.1f, 6              // 6 帧走路，每帧 0.1 秒
);
```

绘制时把方向（行）和当前帧（列）都传进去：

```c
Render_Entity_Push(am, 101, x, y, 1.0f, (int)player.dir, player.currentFrame, LAYER_OBJECT);
```

`dir` 对应图集的行，`currentFrame`（0~5）对应列。帧计时器在 `player.c` 里：移动时每 0.1 秒 `currentFrame = (currentFrame + 1) % 6`，停下时归 0。

---

## 5. 常见坑

1. **方向反了**：先确认相机起始方位，再检查 `ROW_TO_FRAME`。判断方法——按 W，角色应该显示背面朝上走（N 行）。
2. **角色抖动 1px**：检查 `pivot × 160` 是不是整数（132 可以，133 不行）。
3. **脚踩不到地面**：`pivot.y` 应该等于“世界原点投影到单元格里的 y”。相机瞄准 `(0,0,0.85)`、正交 2.6 时，这个值是 132。
4. **输出路径**：Blender 里用 `//frames/`（相对 .blend），不要用 `/tmp/` 这种 Linux 路径。
5. **图集走 LFS**：`*.png` 已在 `.gitattributes` 里配好，提交时自动进 Git LFS。
