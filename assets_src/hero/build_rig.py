"""
绑定骨架 + 生成 6 帧走路动画。

在 Blender 里运行（build_body.py 之后）：文本编辑器 → 打开本文件 → ▶ 运行。
"""
import bpy
import math

body = [o for o in bpy.data.objects if o.type == 'MESH'][0]

# 清理旧的绑定，避免重复叠加
for m in list(body.modifiers):
    if m.type == 'ARMATURE':
        body.modifiers.remove(m)
body.vertex_groups.clear()

for o in list(bpy.data.objects):
    if o.type == 'ARMATURE':
        bpy.data.objects.remove(o, do_unlink=True)

# ---------- 骨架 ----------
arm_data = bpy.data.armatures.new("HeroRig")
arm = bpy.data.objects.new("HeroRig", arm_data)
bpy.context.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
arm.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')

eb = arm_data.edit_bones


def bone(name, head, tail, parent=None):
    b = eb.new(name)
    b.head, b.tail = head, tail
    if parent:
        b.parent = eb[parent]
    return b


bone("root", (0, 0, 0), (0, 0, 0.60))
bone("spine", (0, 0, 0.60), (0, 0, 1.15), "root")
bone("head", (0, 0, 1.15), (0, 0, 1.64), "spine")
bone("leg.L", (0.11, 0, 0.60), (0.11, 0, 0), "root")
bone("leg.R", (-0.11, 0, 0.60), (-0.11, 0, 0), "root")
bone("arm.L", (0.235, 0, 1.03), (0.25, 0, 0.62), "spine")
bone("arm.R", (-0.235, 0, 1.03), (-0.25, 0, 0.62), "spine")
bpy.ops.object.mode_set(mode='OBJECT')

# ---------- 自动权重 ----------
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True)
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.parent_set(type='ARMATURE_AUTO')

# ---------- 6 帧走路 ----------
scene = bpy.context.scene
scene.render.fps = 10
scene.frame_start, scene.frame_end = 1, 6

# 每帧两条腿的角度（度）。腿骨朝下，负值 = 向前（角色面向 -Y）
poses = {
    1: (-30, 30),
    2: (-10, 10),
    3: (10, -10),
    4: (30, -30),
    5: (10, -10),
    6: (-10, 10),
}

bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='POSE')
for b in arm.pose.bones:
    b.rotation_mode = 'XYZ'

for f, (leg_l, leg_r) in poses.items():
    scene.frame_set(f)
    arm.pose.bones["leg.L"].rotation_euler = (math.radians(leg_l), 0, 0)
    arm.pose.bones["leg.R"].rotation_euler = (math.radians(leg_r), 0, 0)
    # 手臂与同侧腿反向摆，幅度 0.8 倍
    arm.pose.bones["arm.L"].rotation_euler = (math.radians(-leg_l * 0.8), 0, 0)
    arm.pose.bones["arm.R"].rotation_euler = (math.radians(-leg_r * 0.8), 0, 0)
    for n in ("leg.L", "leg.R", "arm.L", "arm.R"):
        arm.pose.bones[n].keyframe_insert("rotation_euler", frame=f)

bpy.ops.object.mode_set(mode='OBJECT')
print("骨架 + 走路完成")
