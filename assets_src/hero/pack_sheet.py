"""
把 Blender 渲染的 8 方向 × 6 帧拼成引擎用的图集。

约定：
  - 帧图来自 ../frames/hero_<方向>_<帧>.png（320×320，Blender 原始输出，帧从 01 开始）
  - 输出到 仓库根/assets/textures/hero/hero_sheet.png（960×1280，6 列 × 8 行）
  - 行序按引擎 Direction 枚举：N, NE, E, SE, S, SW, W, NW
  - 相机起始方位是 45°，所以渲染编号比游戏方向大 3（见 ROW_TO_FRAME）

用法（在任意目录）：
  python assets_src/hero/pack_sheet.py
"""
from pathlib import Path
import json

from PIL import Image

HERE = Path(__file__).resolve().parent          # assets_src/hero
FRAMES = HERE / "frames"
OUT_DIR = HERE.parents[1] / "assets" / "textures" / "hero"
OUT_DIR.mkdir(parents=True, exist_ok=True)

CELL = 160                 # 游戏里一格的像素尺寸
COLS = 6                   # 走路循环 6 帧
PIVOT_Y = 130              # 脚底在单元格里的 y（必须是整数）
ROW_TO_FRAME = [3, 4, 5, 6, 7, 0, 1, 2]   # N,NE,E,SE,S,SW,W,NW -> 渲染方向编号


def main():
    sheet = Image.new("RGBA", (CELL * COLS, CELL * 8), (0, 0, 0, 0))

    for row, dir_no in enumerate(ROW_TO_FRAME):
        for col in range(COLS):
            src = FRAMES / f"hero_{dir_no}_{col + 1:02d}.png"
            if not src.exists():
                raise SystemExit(f"缺少渲染文件: {src}")
            im = Image.open(src).convert("RGBA").resize((CELL, CELL), Image.LANCZOS)
            sheet.paste(im, (col * CELL, row * CELL))

    sheet.save(OUT_DIR / "hero_sheet.png")

    meta = {
        "image": "hero_sheet.png",
        "cell": {"w": CELL, "h": CELL},
        "pivot": {"x": CELL // 2, "y": PIVOT_Y},
        "cols": COLS,
        "rows": 8,
        "fps": 10,
        "frame_order": [1, 2, 3, 4, 5, 6],
        "row_order": ["N", "NE", "E", "SE", "S", "SW", "W", "NW"],
        "source_dirs": ROW_TO_FRAME,
        "note": "行=方向（已按引擎枚举重排），列=走路帧；源文件按相机 0/45/.../315 度渲染",
    }
    with open(OUT_DIR / "hero_sheet.json", "w", encoding="utf-8") as f:
        json.dump(meta, f, ensure_ascii=False, indent=2)

    print(f"OK -> {OUT_DIR/'hero_sheet.png'} ({sheet.width}x{sheet.height})")


if __name__ == "__main__":
    main()
