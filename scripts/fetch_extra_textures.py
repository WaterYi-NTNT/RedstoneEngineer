from __future__ import annotations
"""补充额外贴图"""

import shutil
from pathlib import Path
from typing import List, Tuple, Dict

SRC_DIR  = Path(r"E:\Code\RedstoneEngineer\data\InventivetalentDev minecraft-assets 1.21 assets-minecraft_textures_block")
OUT_DIR  = Path(__file__).parent.parent / "resources" / "textures"
QRC_PATH = Path(__file__).parent.parent / "resources" / "resources.qrc"

PNG_MAGIC = b'\x89PNG'

EXTRA: Dict[str, Tuple[str, str]] = {
    "redstone_dust_dot":              ("redstone_dust_dot.png",              "红石粉·点"),
    "redstone_dust_line0":            ("redstone_dust_line0.png",            "红石粉·线(南北)"),
    "redstone_dust_overlay":          ("redstone_dust_overlay.png",          "红石粉·十字遮罩"),
    "piston_top_normal":              ("piston_top.png",                     "活塞推出面"),
    "piston_top_sticky":              ("piston_top_sticky.png",              "粘性活塞推出面"),
    "piston_side":                    ("piston_side.png",                    "活塞侧面"),
    "piston_bottom":                  ("piston_bottom.png",                  "活塞底面"),
    "repeater_top":                   ("repeater.png",                       "中继器顶面"),
    "comparator_top":                 ("comparator.png",                     "比较器顶面"),
    "observer_top":                   ("observer_top.png",                   "侦测器顶面"),
    "observer_front":                 ("observer_front.png",                 "侦测器检测面"),
    "observer_back":                  ("observer_back.png",                  "侦测器背面"),
    "dropper_front":                  ("dropper_front.png",                  "投掷器正面(水平)"),
    "dropper_front_vertical":         ("dropper_front_vertical.png",         "投掷器正面(垂直)"),
    "dispenser_front":                ("dispenser_front.png",                "发射器正面(水平)"),
    "dispenser_front_vertical":       ("dispenser_front_vertical.png",       "发射器正面(垂直)"),
    "hopper_top":                     ("hopper_top.png",                     "漏斗顶面"),
    "hopper_side":                    ("hopper_outside.png",                 "漏斗侧面"),
    "lever":                          ("lever.png",                          "拉杆"),
    "redstone_torch":                 ("redstone_torch.png",                 "红石火把"),
    "lectern_top":                    ("lectern_top.png",                    "讲台顶面"),
    "cobblestone":                    ("cobblestone.png",                    "圆石（楼梯用）"),
}

def is_valid_png(data: bytes) -> bool:
    return len(data) >= 4 and data[:4] == PNG_MAGIC

def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    if not SRC_DIR.exists():
        print(f"❌ 源目录不存在：{SRC_DIR}"); return

    copied, missing = [], []

    for key, (src_file, desc) in EXTRA.items():
        dest = OUT_DIR / f"{key}.png"
        if dest.exists() and is_valid_png(dest.read_bytes()):
            print(f"  ⏭  已存在：{desc}")
            copied.append(key); continue

        src = SRC_DIR / src_file
        if src.exists():
            data = src.read_bytes()
            if is_valid_png(data):
                shutil.copy2(src, dest)
                print(f"  ✓  {desc:<28} ← {src_file}")
                copied.append(key)
            else:
                print(f"  ✗  无效：{src_file}")
                missing.append((key, desc))
        else:
            print(f"  ❌ 找不到：{src_file}  ({desc})")
            missing.append((key, desc))

    # 更新 QRC
    existing_keys: set = set()
    if QRC_PATH.exists():
        for line in QRC_PATH.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line.startswith("<file>"):
                k = line.replace("<file>textures/", "").replace(".png</file>", "")
                existing_keys.add(k)

    all_keys = sorted(existing_keys | set(copied))
    lines = ['<RCC>', '    <qresource prefix="/">']
    for k in all_keys:
        lines.append(f'        <file>textures/{k}.png</file>')
    lines += ['    </qresource>', '</RCC>']
    QRC_PATH.write_text("\n".join(lines), encoding="utf-8")

    print(f"\n✅ 成功 {len(copied)} 个，缺失 {len(missing)} 个")
    print("✅ QRC 已更新")

if __name__ == "__main__":
    main()