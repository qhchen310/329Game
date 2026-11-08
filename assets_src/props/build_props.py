"""
生成并渲染两个道具：地砖、树。

用法：
  1. Blender 里 文件 → 新建 → 常规（干净场景）
  2. 文本编辑器 → 打开本文件 → ▶ 运行
输出：
  assets_src/props/out/tile.png  128×128
  assets_src/props/out/tree.png  160×224

规格和角色一致：相机旋转 60°/45°、左前主光；地砖菱形 128×64。
"""
import math
import os

import bpy

OUT = r"C:\Users\cqh329\sdl_game\assets_src\props\out"
os.makedirs(OUT, exist_ok=True)


def lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def rgb(hexv):
    h = hexv.lstrip("#")
    return tuple(lin(int(h[i:i + 2], 16) / 255.0) for i in (0, 2, 4))


def mat(name, hexv, rough=0.85):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    if b:
        b.inputs["Base Color"].default_value = (*rgb(hexv), 1.0)
        b.inputs["Roughness"].default_value = rough
        b.inputs["Metallic"].default_value = 0.0
    return m


# ---------- 清空场景 ----------
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete()

scene = bpy.context.scene
try:
    scene.render.engine = 'BLENDER_EEVEE_NEXT'
except TypeError:
    scene.render.engine = 'BLENDER_EEVEE'
scene.render.film_transparent = True
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'

# ---------- 相机（和角色同一角度）----------
cam_data = bpy.data.cameras.new("Cam")
cam_data.type = 'ORTHO'
cam = bpy.data.objects.new("Cam", cam_data)
scene.collection.objects.link(cam)
cam.rotation_euler = (math.radians(60), 0, math.radians(45))
scene.camera = cam

# ---------- 主光 ----------
sun_data = bpy.data.lights.new("Sun", 'SUN')
sun_data.energy = 3.0
sun = bpy.data.objects.new("Sun", sun_data)
scene.collection.objects.link(sun)
sun.rotation_euler = (math.radians(50), 0, math.radians(-30))

# ---------- 材质 ----------
m_grass = mat("草地", "#6FA83C")
m_soil = mat("泥土", "#6B4A32")
m_trunk = mat("树干", "#5A3B26")
m_leaf = mat("树叶", "#3E7A2E")

# ---------- 地砖：1×1 的板，顶面草地、侧面泥土 ----------
bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, -0.06))
tile = bpy.context.object
tile.name = "Tile"
tile.scale = (1.0, 1.0, 0.12)
tile.data.materials.append(m_grass)
tile.data.materials.append(m_soil)
for p in tile.data.polygons:
    p.material_index = 0 if p.normal.z > 0.5 else 1

# ---------- 树：树干 + 三团树冠 ----------
bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=0.11, depth=1.2, location=(0, 0, 0.6))
trunk = bpy.context.object
trunk.name = "Trunk"
trunk.data.materials.append(m_trunk)

canopy_parts = []
for loc, r in [((0.00, 0.00, 1.55), 0.60),
               ((0.28, 0.10, 1.95), 0.42),
               ((-0.22, -0.12, 1.85), 0.46)]:
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=r, location=loc)
    o = bpy.context.object
    o.data.materials.append(m_leaf)
    canopy_parts.append(o)

bpy.ops.object.select_all(action='DESELECT')
for o in canopy_parts + [trunk]:
    o.select_set(True)
bpy.context.view_layer.objects.active = trunk
bpy.ops.object.join()
tree = bpy.context.object
tree.name = "Tree"

# ---------- 渲染地砖 ----------
tree.hide_render = True
tile.hide_render = False

cam.data.ortho_scale = 1.414          # 1×1 的板 → 菱形宽 128 px
cam.location = (6.12, -6.12, 5.0)     # 看向原点
scene.render.resolution_x = 128
scene.render.resolution_y = 128
scene.render.filepath = os.path.join(OUT, "tile.png")
bpy.ops.render.render(write_still=True)
print("写了:", scene.render.filepath)

# ---------- 渲染树 ----------
tree.hide_render = False
tile.hide_render = True

cam.data.ortho_scale = 3.2
cam.location = (6.12, -6.12, 6.25)    # 看向 (0,0,1.25)
scene.render.resolution_x = 160
scene.render.resolution_y = 224
scene.render.filepath = os.path.join(OUT, "tree.png")
bpy.ops.render.render(write_still=True)
print("写了:", scene.render.filepath)

print("完成 →", OUT)
