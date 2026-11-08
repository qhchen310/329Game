"""
生成眼睛（独立物体，绑到 head 骨）。

在 Blender 里运行（build_hair.py 之后）。
眼睛尺寸/位置从 Body 网格实测，改头型后重跑即可。
"""
import bpy

body = bpy.data.objects["Body"]
rig = bpy.data.objects["HeroRig"]

old = bpy.data.objects.get("Eyes")
if old:
    bpy.data.objects.remove(old, do_unlink=True)

# ---------- 量头骨 ----------
head_pts = [v.co for v in body.data.vertices if v.co.z > 1.30]
top = max(p.z for p in head_pts)
half_w = max(abs(p.x) for p in head_pts)
half_d = max(abs(p.y) for p in head_pts)

center_z = top - half_w          # 头球心
eye_z = center_z - half_w * 0.12 # 中线略偏下（参考图眼位）
eye_x = half_w * 0.34
eye_y = -half_d * 0.93           # 脸在 -Y
print("眼睛: z=%.3f x=%.3f y=%.3f (头 top=%.3f half_w=%.3f)" % (eye_z, eye_x, eye_y, top, half_w))

parts = []

for side in (1, -1):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=10, radius=1.0,
                                         location=(eye_x * side, eye_y, eye_z))
    o = bpy.context.object
    # 扁圆：横向略宽、贴着脸（参考图单眼约占头宽 20%）
    o.scale = (half_w * 0.20, half_d * 0.06, half_w * 0.26)
    parts.append(o)

bpy.ops.object.select_all(action='DESELECT')
for p in parts:
    p.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
eyes = bpy.context.object
eyes.name = "Eyes"

bpy.context.scene.cursor.location = (0, 0, 0)
bpy.ops.object.origin_set(type='ORIGIN_CURSOR')

# 材质：近黑
mat = bpy.data.materials.get("眼睛") or bpy.data.materials.new("眼睛")
mat.use_nodes = True
bsdf = next((n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
if bsdf:
    def lin(c):
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    rgb = tuple(lin(v / 255.0) for v in (0x1A, 0x18, 0x18))
    bsdf.inputs["Base Color"].default_value = (*rgb, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.45
eyes.data.materials.append(mat)

# 绑到 head 骨
vg = eyes.vertex_groups.new(name="head")
vg.add([v.index for v in eyes.data.vertices], 1.0, 'REPLACE')

mod = eyes.modifiers.new("armature", 'ARMATURE')
mod.object = rig
eyes.parent = rig

print("眼睛完成：", eyes.name, "顶点", len(eyes.data.vertices))
