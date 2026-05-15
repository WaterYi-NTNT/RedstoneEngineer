from __future__ import annotations
"""下载 MC item 贴图，用于物品栏显示"""

import shutil
from pathlib import Path
from typing import Dict, Tuple, List

# item 贴图源目录（需要从 mcasset.cloud 下载 item 目录）
# 如果本地没有，脚本会提示手动下载
LOCAL_ITEM_DIR = Path(r"C:\Users\lenovo\Downloads\InventivetalentDev minecraft-assets 1.21 assets-minecraft_textures_item")
LOCAL_BLOCK_DIR = Path(r"C:\Users\lenovo\Downloads\InventivetalentDev minecraft-assets 1.21 assets-minecraft_textures_block")

OUT_DIR  = Path(__file__).parent.parent / "resources" / "textures"
QRC_PATH = Path(__file__).parent.parent / "resources" / "resources.qrc"

PNG_MAGIC = b'\x89PNG'

# block_key → (item文件名, 来源目录('item'或'block'), 说明)
ITEM_TEXTURES: Dict[str, Tuple[str, str, str]] = {
    # 有专属 item 贴图的方块
    # key 命名规则：原 key + "_item" 后缀，避免与 block 贴图冲突
    "redstone_wire_item":                    ("redstone.png",                        "item",  "红石粉物品"),
    "redstone_torch_item":                   ("redstone_torch.png",                  "item",  "红石火把物品"),
    "repeater_item":                         ("repeater.png",                        "item",  "中继器物品"),
    "comparator_item":                       ("comparator.png",                      "item",  "比较器物品"),
    "lever_item":                            ("lever.png",                           "item",  "拉杆物品"),
    "stone_button_item":                     ("stone_button.png",                    "item",  "石头按钮物品"),
    "wood_button_item":                      ("oak_button.png",                      "item",  "木头按钮物品"),
    "stone_pressure_plate_item":             ("stone_pressure_plate.png",            "item",  "石质压力板物品"),
    "wood_pressure_plate_item":              ("oak_pressure_plate.png",              "item",  "木质压力板物品"),
    "light_weighted_pressure_plate_item":    ("light_weighted_pressure_plate.png",   "item",  "金质压力板物品"),
    "heavy_weighted_pressure_plate_item":    ("heavy_weighted_pressure_plate.png",   "item",  "铁质压力板物品"),
    "tripwire_hook_item":                    ("tripwire_hook.png",                   "item",  "绊线钩物品"),
    "tripwire_item":                         ("string.png",                          "item",  "绊线物品"),
    "iron_door_item":                        ("iron_door.png",                       "item",  "铁门物品"),
    "iron_trapdoor_item":                    ("iron_trapdoor.png",                   "item",  "铁活板门物品"),
    "powered_rail_item":                     ("powered_rail.png",                    "item",  "充能铁轨物品"),
    "hopper_item":                           ("hopper.png",                          "item",  "漏斗物品"),
    "trapped_chest_item":                    ("trapped_chest.png",                   "item",  "陷阱箱物品"),
    "cobblestone":                           ("cobblestone.png",                     "block", "圆石（楼梯用）"),
}

def is_valid_png(data: bytes) -> bool:
    return len(data) >= 4 and data[:4] == PNG_MAGIC

def get_src_dir(src_type: str) -> Path:
    return LOCAL_ITEM_DIR if src_type == "item" else LOCAL_BLOCK_DIR

def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    if not LOCAL_ITEM_DIR.exists():
        print(f"❌ item 目录不存在：{LOCAL_ITEM_DIR}")
        print("请先从以下地址下载 item 贴图目录：")
        print("https://mcasset.cloud/1.21.1/assets/minecraft/textures/item")
        print("下载后解压到上述路径再运行此脚本")
        return

    copied: List[str] = []
    missing: List[Tuple[str, str]] = []

    for key, (filename, src_type, desc) in ITEM_TEXTURES.items():
        dest = OUT_DIR / f"{key}.png"

        if dest.exists() and is_valid_png(dest.read_bytes()):
            print(f"  ⏭  已存在：{desc}")
            copied.append(key)
            continue

        src = get_src_dir(src_type) / filename
        if src.exists():
            data = src.read_bytes()
            if is_valid_png(data):
                shutil.copy2(src, dest)
                print(f"  ✓  {desc:<24} ← {src_type}/{filename}")
                copied.append(key)
            else:
                print(f"  ✗  无效文件：{filename}")
                missing.append((key, desc))
        else:
            print(f"  ❌ 找不到：{src_type}/{filename}  ({desc})")
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

    print(f"\n✅ 成功：{len(copied)} 个，缺失：{len(missing)} 个")
    if missing:
        for key, desc in missing:
            print(f"   ❌ {desc}")
    print("✅ QRC 已更新")

if __name__ == "__main__":
    main()