"""
生成 Q 版通用身体（无装备、无头发）。

在 Blender 里运行：文本编辑器 → 打开本文件 → ▶ 运行。
前提：场景里已有 CAM_RIG（相机 + 灯光挂在它下面），本脚本不会动它。
"""
import bpy

# 清掉旧的身体和骨架，保留相机支点/灯光
for name in ("Body", "HeroRig"):
    o = bpy.data.objects.get(name)
    if o:
        bpy.data.objects.remove(o, do_unlink=True)

V, E, R = [], [], {}


def v(co, r):
    V.append(co)
    R[len(V) - 1] = r
    return len(V) - 1


# ---------- 脊柱：半径是 (X 半宽, Y 半深)，所以躯干是扁的 ----------
hips = v((0, 0, 0.62), (0.16, 0.11))
waist = v((0, 0, 0.80), (0.15, 0.10))
chest = v((0, 0, 0.98), (0.20, 0.11))
neck = v((0, 0, 1.16), (0.055, 0.055))
head = v((0, 0, 1.44), (0.19, 0.19))
htop = v((0, 0, 1.62), (0.09, 0.09))
E += [(hips, waist), (waist, chest), (chest, neck), (neck, head), (head, htop)]

# ---------- 四肢 ----------
for side in (1, -1):
    sh = v((0.19 * side, 0, 1.06), (0.05, 0.05))
    el = v((0.21 * side, 0, 0.88), (0.045, 0.045))
    wr = v((0.21 * side, 0, 0.72), (0.04, 0.04))
    hd = v((0.21 * side, 0, 0.66), (0.045, 0.045))
    E += [(chest, sh), (sh, el), (el, wr), (wr, hd)]

    hip = v((0.10 * side, 0, 0.62), (0.075, 0.075))
    kn = v((0.10 * side, 0, 0.35), (0.065, 0.065))
    an = v((0.10 * side, 0, 0.10), (0.05, 0.05))
    toe = v((0.10 * side, -0.12, 0.05), (0.05, 0.05))
    E += [(hips, hip), (hip, kn), (kn, an), (an, toe)]

mesh = bpy.data.meshes.new("BodyMesh")
mesh.from_pydata(V, E, [])
mesh.update()

body = bpy.data.objects.new("Body", mesh)
bpy.context.collection.objects.link(body)
bpy.context.view_layer.objects.active = body
body.select_set(True)

# Skin：沿骨架线长出身体，按每个点的半径决定粗细
body.modifiers.new("skin", 'SKIN')
skin_data = body.data.skin_vertices[0].data
for i, r in R.items():
    skin_data[i].radius = r
skin_data[hips].use_root = True

# 细分：把方截面磨圆
sub = body.modifiers.new("subsurf", 'SUBSURF')
sub.levels = 2
sub.render_levels = 2

bpy.ops.object.modifier_apply(modifier="skin")
bpy.ops.object.modifier_apply(modifier="subsurf")
bpy.ops.object.shade_smooth()

# ---------- 归一化：细分会让表面整体收缩，必须拉回 1.7 m 并压到地面 ----------
zmin = min(vv.co.z for vv in body.data.vertices)
for vv in body.data.vertices:
    vv.co.z -= zmin

h = max(vv.co.z for vv in body.data.vertices)
s = 1.7 / h
for vv in body.data.vertices:
    vv.co *= s
body.data.update()

bpy.context.scene.cursor.location = (0, 0, 0)
bpy.ops.object.origin_set(type='ORIGIN_CURSOR')

zs = [vv.co.z for vv in body.data.vertices]
print("身体完成：高度 %.3f m，最低点 %.3f m" % (max(zs) - min(zs), min(zs)))
