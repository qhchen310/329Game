"""
8 方向 × 6 帧 批量渲染。

在 Blender 里运行（build_rig.py 之后）。输出到 //frames/（相对 .blend 文件）。
"""
import math
import os

import bpy

scene = bpy.context.scene
rig = bpy.data.objects["CAM_RIG"]
out_dir = bpy.path.abspath("//frames")
os.makedirs(out_dir, exist_ok=True)

if not scene.render.use_file_extension:
    scene.render.use_file_extension = True

for d in range(8):
    rig.rotation_euler[2] = math.radians(d * 45)
    for f in range(1, 7):
        scene.frame_set(f)
        scene.render.filepath = os.path.join(out_dir, "hero_%d_%02d.png" % (d, f))
        bpy.ops.render.render(write_still=True)
        print("写了:", scene.render.filepath)

rig.rotation_euler[2] = 0
print("48 张渲染完成:", out_dir)
