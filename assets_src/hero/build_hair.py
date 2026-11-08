"""
生成头发（独立物体，绑到 head 骨）。

在 Blender 里运行（build_rig.py + materials.py 之后）。
头骨尺寸从 Body 网格实测，改身体比例后重跑也不会错位。
"""
import math

import bmesh
import bpy

body = bpy.data.objects["Body"]
rig = bpy.data.objects["HeroRig"]

old = bpy.data.objects.get("Hair")
if old:
    bpy.data.objects.remove(old, do_unlink=True)

# ---------- 量头骨 ----------
head_pts = [v.co for v in body.data.vertices if v.co.z > 1.30]
top = max(p.z for p in head_pts)
half_w = max(abs(p.x) for p in head_pts)
half_d = max(abs(p.y) for p in head_pts)
print("头骨: 顶 %.3f 半宽 %.3f 半深 %.3f" % (top, half_w, half_d))

r = half_w * 1.08
cap_z = top - half_w * 0.70     # 发际线：盖住头顶约 35%
parts = []


def cut_sphere(loc, scale, keep):
    """建球并只保留 keep(local_co) 为真的部分。"""
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=16, radius=1.0, location=loc)
    o = bpy.context.object
    o.scale = scale

    bm = bmesh.new()
    bm.from_mesh(o.data)
    dead = [v for v in bm.verts if not keep(v.co)]
    if dead:
        bmesh.ops.delete(bm, geom=dead, context='VERTS')
    bm.to_mesh(o.data)
    bm.free()

    if len(o.data.vertices) == 0:
        raise SystemExit("切出来是空的，检查 keep 条件")
    parts.append(o)
    return o


# 1. 头顶：水平切（只留上半），发际线在 cap_z
cut_sphere((0, 0.00, cap_z), (r, r, r * 0.78), lambda c: c.z >= 0.0)

# 2. 后脑：只留后半（y >= 0），贴着后脑勺往下延伸，不会在两侧凸出来
cut_sphere((0, 0.06, top - 1.05 * r), (r * 0.96, r * 0.90, r * 1.02), lambda c: c.y >= 0.0)

# 3. 三撮碎发（很短，只做表面起伏）
def cone(loc, radius, depth, rot):
    bpy.ops.mesh.primitive_cone_add(vertices=8, radius1=radius, radius2=0.0,
                                    depth=depth, location=loc, rotation=rot)
    o = bpy.context.object
    parts.append(o)
    return o


for loc, rot in [
    ((0.00, 0.03, top - 0.22 * r), (math.radians(-20), 0, 0)),
    ((0.09, 0.01, top - 0.26 * r), (math.radians(-20), 0, math.radians(30))),
    ((-0.09, 0.01, top - 0.26 * r), (math.radians(-20), 0, math.radians(-30))),
]:
    cone(loc, r * 0.14, r * 0.42, rot)

# 合并
bpy.ops.object.select_all(action='DESELECT')
for p in parts:
    p.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
hair = bpy.context.object
hair.name = "Hair"

bpy.context.scene.cursor.location = (0, 0, 0)
bpy.ops.object.origin_set(type='ORIGIN_CURSOR')

# 材质：深棕
mat = bpy.data.materials.get("头发") or bpy.data.materials.new("头发")
mat.use_nodes = True
bsdf = next((n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
if bsdf:
    def lin(c):
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    rgb = tuple(lin(v / 255.0) for v in (0x6B, 0x5B, 0x52))
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.75
hair.data.materials.append(mat)

# 绑到 head 骨
vg = hair.vertex_groups.new(name="head")
vg.add([v.index for v in hair.data.vertices], 1.0, 'REPLACE')

mod = hair.modifiers.new("armature", 'ARMATURE')
mod.object = rig
hair.parent = rig

print("头发完成：", hair.name, "顶点", len(hair.data.vertices))
