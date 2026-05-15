from __future__ import annotations
"""
RedstoneEngineer - 本地贴图复制脚本
"""

import shutil
from pathlib import Path
from typing import List, Tuple, Dict

SRC_DIR  = Path(r"E:\Code\RedstoneEngineer\data\InventivetalentDev minecraft-assets 1.21 assets-minecraft_textures_block")
OUT_DIR  = Path(__file__).parent.parent / "resources" / "textures"
QRC_PATH = Path(__file__).parent.parent / "resources" / "resources.qrc"

PNG_MAGIC = b'\x89PNG'

BLOCK_TEXTURES: Dict[str, Tuple[str, str]] = {
    "lever":                          ("lever.png",                           "拉杆"),
    "stone_button":                   ("stone_button.png",                    "石头按钮"),
    "wood_button":                    ("oak_button.png",                      "木头按钮"),
    "stone_pressure_plate":           ("stone_pressure_plate.png",            "石质压力板"),
    "wood_pressure_plate":            ("oak_pressure_plate.png",              "木质压力板"),
    "light_weighted_pressure_plate":  ("light_weighted_pressure_plate.png",   "金质压力板"),
    "heavy_weighted_pressure_plate":  ("heavy_weighted_pressure_plate.png",   "铁质压力板"),
    "daylight_detector":              ("daylight_detector_top.png",           "阳光传感器"),
    "target_block":                   ("target_top.png",                      "目标方块"),
    "sculk_sensor":                   ("sculk_sensor_top.png",                "幽匿传感器"),
    "calibrated_sculk_sensor":        ("calibrated_sculk_sensor_top.png",     "校准幽匿传感器"),
    "tripwire_hook":                  ("tripwire_hook.png",                   "绊线钩"),
    "tripwire":                       ("tripwire.png",                        "绊线"),
    "trapped_chest":                  ("oak_planks.png",                      "陷阱箱"),
    "lectern":                        ("lectern_top.png",                     "讲台"),
    "redstone_wire":                  ("redstone_dust_dot.png",               "红石粉"),
    "redstone_torch":                 ("redstone_torch.png",                  "红石火把"),
    "redstone_block":                 ("redstone_block.png",                  "红石块"),
    "repeater":                       ("repeater.png",                        "中继器"),
    "comparator":                     ("comparator.png",                      "比较器"),
    "observer":                       ("observer_front.png",                  "侦测器"),
    "piston":                         ("piston_top.png",                      "活塞"),
    "sticky_piston":                  ("piston_top_sticky.png",               "粘性活塞"),
    "dropper":                        ("dropper_front.png",                   "投掷器"),
    "dispenser":                      ("dispenser_front.png",                 "发射器"),
    "hopper":                         ("hopper_top.png",                      "漏斗"),
    "tnt":                            ("tnt_side.png",                        "TNT"),
    "redstone_lamp":                  ("redstone_lamp.png",                   "红石灯"),
    "iron_door":                      ("iron_door_top.png",                   "铁门"),
    "iron_trapdoor":                  ("iron_trapdoor.png",                   "铁活板门"),
    "fence_gate":                     ("oak_planks.png",                      "栅栏门"),
    "note_block":                     ("note_block.png",                      "音符盒"),
    "powered_rail":                   ("powered_rail.png",                    "充能铁轨"),
    "stone":                          ("stone.png",                           "石头"),
    "glass":                          ("glass.png",                           "玻璃"),
    "slab_top":                       ("smooth_stone_slab_side.png",          "台阶（上半）"),
    "slab_bottom":                    ("smooth_stone.png",                    "台阶（下半）"),
    "stair":                          ("stone.png",                           "楼梯"),
    "other":                          ("bedrock.png",                         "其他方块"),
}

def is_valid_png(data: bytes) -> bool:
    return len(data) >= 4 and data[:4] == PNG_MAGIC

def generate_qrc(keys: List[str]) -> None:
    lines = ['<RCC>', '    <qresource prefix="/">']
    for key in sorted(keys):
        lines.append(f'        <file>textures/{key}.png</file>')
    lines += ['    </qresource>', '</RCC>']
    QRC_PATH.write_text("\n".join(lines), encoding="utf-8")
    print(f"\n✅ QRC 已生成：{QRC_PATH}")

def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    if not SRC_DIR.exists():
        print(f"❌ 源目录不存在：{SRC_DIR}"); return

    print(f"📂 源目录：{SRC_DIR}")
    copied, missing = [], []

    for block_key, (src_file, display_name) in BLOCK_TEXTURES.items():
        dest = OUT_DIR / f"{block_key}.png"
        if dest.exists() and is_valid_png(dest.read_bytes()):
            print(f"  ⏭  已存在：{display_name}")
            copied.append(block_key); continue

        src = SRC_DIR / src_file
        if src.exists():
            data = src.read_bytes()
            if is_valid_png(data):
                shutil.copy2(src, dest)
                print(f"  ✓  {display_name:<20} ← {src_file}")
                copied.append(block_key)
            else:
                print(f"  ✗  无效：{src_file}")
                missing.append((block_key, display_name))
        else:
            print(f"  ❌ 找不到：{src_file}  ({display_name})")
            missing.append((block_key, display_name))

    if missing:
        print(f"\n❌ 缺失 {len(missing)} 个")
        copied += [k for k, _ in missing]

    generate_qrc(copied)
    print("🎉 完成！")

if __name__ == "__main__":
    main()