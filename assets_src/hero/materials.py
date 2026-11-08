"""
给通用身体上基础材质 + 补光。

在 Blender 里运行（build_rig.py 之后）：文本编辑器 → 打开本文件 → ▶ 运行。
"""
import math

import bpy


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hex_rgb(h):
    h = h.lstrip("#")
    return tuple(srgb_to_linear(int(h[i:i + 2], 16) / 255.0) for i in (0, 2, 4))


def principled_of(mat):
    """按节点类型找 Principled BSDF——中文界面下它叫「原理化 BSDF」，不能按名字找。"""
    for n in mat.node_tree.nodes:
        if n.type == 'BSDF_PRINCIPLED':
            return n
    # 极端情况：材料里没有，就手动建一个并接到输出
    n = mat.node_tree.nodes.new("ShaderNodeBsdfPrincipled")
    for out in mat.node_tree.nodes:
        if out.type == 'OUTPUT_MATERIAL':
            mat.node_tree.links.new(n.outputs[0], out.inputs["Surface"])
            break
    return n


def set_input(node, name, index, value):
    """优先按名字设值，名字不对（本地化）就按索引。"""
    if name in node.inputs:
        node.inputs[name].default_value = value
    else:
        node.inputs[index].default_value = value


def make_mat(name, hex_color, rough=0.7):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    bsdf = principled_of(m)

    rgb = hex_rgb(hex_color)
    set_input(bsdf, "Base Color", 0, (*rgb, 1.0))
    set_input(bsdf, "Metallic", 1, 0.0)
    set_input(bsdf, "Roughness", 2, rough)

    m.diffuse_color = (*rgb, 1.0)   # 视口预览色
    return m


body = bpy.data.objects["Body"]

skin = make_mat("皮肤", "#E9B79A", 0.65)
cloth = make_mat("内衬", "#3E4552", 0.85)

body.data.materials.clear()
body.data.materials.append(skin)    # index 0
body.data.materials.append(cloth)   # index 1

# 按面中心分区：头 + 手 = 皮肤，其余 = 内衬
for poly in body.data.polygons:
    c = poly.center
    if c.z > 1.25:
        poly.material_index = 0
    elif 0.58 < c.z < 0.75 and abs(c.x) > 0.17:
        poly.material_index = 0
    else:
        poly.material_index = 1

# 补光：必须挂在 CAM_RIG 下，才会跟着相机一起转
rig = bpy.data.objects["CAM_RIG"]
if "Fill" not in bpy.data.objects:
    ld = bpy.data.lights.new("Fill", 'SUN')
    ld.energy = 0.8
    fill = bpy.data.objects.new("Fill", ld)
    bpy.context.collection.objects.link(fill)
    fill.parent = rig
    fill.rotation_euler = (math.radians(55), 0, math.radians(160))

print("材质 + 补光完成")
