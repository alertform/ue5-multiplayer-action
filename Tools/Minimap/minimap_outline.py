# -*- coding: utf-8 -*-
"""小地图描边 v2:障碍掩码法。
以全图中值色(=地面)为基准,RGB 距离超阈值的像素视为障碍/结构;网格线颜色贴近地面
自然落在掩码外(v1 的 Sobel 会把任何低对比细线放大成边,已废)。
形态学开运算清杂点 -> 掩码边界描琥珀 -> 地面轻微提亮。重烘后重跑即可。"""
import numpy as np
from PIL import Image

SRC = r"F:\MultiPlayerAction\Saved\T_Minimap_ThirdPersonMap.png"
DST = r"F:\MultiPlayerAction\Saved\T_Minimap_ThirdPersonMap_Outlined.png"

DIST_THRESHOLD = 28.0   # 与地面色的 RGB 距离阈值(网格线之上留余量)
EDGE_COLOR = np.array([235.0, 200.0, 120.0])
EDGE_ALPHA = 0.9
FLOOR_LIFT = 1.25


def window(mask: np.ndarray, size: int) -> np.ndarray:
    pad = size // 2
    p = np.pad(mask, pad, mode="constant")
    return np.lib.stride_tricks.sliding_window_view(p, (size, size))


def dilate(mask: np.ndarray, size: int = 3) -> np.ndarray:
    return window(mask, size).any(axis=(2, 3))


def erode(mask: np.ndarray, size: int = 3) -> np.ndarray:
    return window(mask, size).all(axis=(2, 3))


img = np.asarray(Image.open(SRC).convert("RGB"), dtype=np.float32)

# 黑边框(捕捉方形 vs 场景矩形的留白)不参与地面中值估计
content = img.reshape(-1, 3)
content = content[content.sum(axis=1) > 15]
floor = np.median(content, axis=0)

dist = np.linalg.norm(img - floor, axis=2)
mask = dist > DIST_THRESHOLD
mask = erode(dilate(mask, 9), 9)          # 闭运算:填岩石纹理内部空洞,只留外轮廓
mask = dilate(erode(mask, 3), 3)          # 开运算:清单像素杂点/残线

boundary = mask & ~erode(mask, 5)          # 边界带(~2px)
boundary = dilate(boundary, 3)             # 加粗到小窗口可见

out = np.clip(img * FLOOR_LIFT, 0, 255)
a = boundary[..., None] * EDGE_ALPHA
out = out * (1 - a) + EDGE_COLOR * a

Image.fromarray(out.astype(np.uint8), "RGB").save(DST)
ratio = mask.mean() * 100
print(f"floor={floor.astype(int)} mask={ratio:.1f}% boundary_px={int(boundary.sum())}")
