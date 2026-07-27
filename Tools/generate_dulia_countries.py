#!/usr/bin/env python3
"""Generate a deterministic 195-country segmentation for Planet Dulia.

Outputs are deliberately data-only so the generated country layout can be
replaced later without changing simulation code.
"""

from __future__ import annotations

import json
import math
import random
from collections import deque
from pathlib import Path

import numpy as np
from PIL import Image

PROJECT_ROOT = Path(r"F:\Unreal Projects\Democracy")
SOURCE_IMAGE = PROJECT_ROOT / "dulia.png"
OUTPUT_DIR = PROJECT_ROOT / "Content" / "World" / "Dulia" / "Generated"
COUNTRY_COUNT = 195
RANDOM_SEED = 7242026


def is_water_like(rgb: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    r = rgb[..., 0].astype(np.int16)
    g = rgb[..., 1].astype(np.int16)
    b = rgb[..., 2].astype(np.int16)
    blue_ocean = (b >= 130) & (g >= 105) & (b > r + 28) & (b > g + 12)
    transparent = alpha < 16
    return blue_ocean | transparent


def connected_components(mask: np.ndarray) -> tuple[np.ndarray, list[int]]:
    h, w = mask.shape
    comp = np.zeros((h, w), dtype=np.int32)
    sizes: list[int] = []
    cid = 0
    for y in range(h):
        for x in range(w):
            if not mask[y, x] or comp[y, x] != 0:
                continue
            cid += 1
            q: deque[tuple[int, int]] = deque([(y, x)])
            comp[y, x] = cid
            count = 0
            while q:
                cy, cx = q.popleft()
                count += 1
                for ny, nx in ((cy - 1, cx), (cy + 1, cx), (cy, cx - 1), (cy, cx + 1)):
                    if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and comp[ny, nx] == 0:
                        comp[ny, nx] = cid
                        q.append((ny, nx))
            sizes.append(count)
    return comp, sizes


def allocate_country_counts(component_sizes: list[int], total: int) -> dict[int, int]:
    indexed = [(idx + 1, size) for idx, size in enumerate(component_sizes)]
    indexed.sort(key=lambda item: item[1], reverse=True)
    # Ignore tiny specks as their own country unless they are among the only land available.
    major = [(cid, size) for cid, size in indexed if size >= 75]
    if not major:
        major = indexed[:]
    if len(major) > total:
        major = major[:total]
    land_total = sum(size for _, size in major)
    allocation: dict[int, int] = {}
    remainders: list[tuple[float, int]] = []
    assigned = 0
    for cid, size in major:
        exact = (size / land_total) * total
        count = max(1, int(math.floor(exact)))
        allocation[cid] = count
        assigned += count
        remainders.append((exact - math.floor(exact), cid))
    if assigned > total:
        ordered = [cid for _, cid in sorted(remainders)]
        while assigned > total:
            changed = False
            for cid in ordered:
                if assigned <= total:
                    break
                if allocation[cid] > 1:
                    allocation[cid] -= 1
                    assigned -= 1
                    changed = True
            if not changed:
                raise RuntimeError("Unable to reduce country allocation to requested total")
    elif assigned < total:
        ordered = [cid for _, cid in sorted(remainders, reverse=True)]
        while assigned < total:
            for cid in ordered:
                if assigned >= total:
                    break
                allocation[cid] += 1
                assigned += 1
    return allocation


def choose_seeds(coords: np.ndarray, count: int, rng: random.Random) -> list[tuple[int, int]]:
    if count <= 0 or coords.size == 0:
        return []
    if count == 1:
        yx = coords[len(coords) // 2]
        return [(int(yx[0]), int(yx[1]))]

    start = rng.randrange(len(coords))
    seeds = [coords[start]]
    distances = np.sum((coords - seeds[0]) ** 2, axis=1).astype(np.float64)
    for _ in range(1, count):
        next_idx = int(np.argmax(distances))
        next_seed = coords[next_idx]
        seeds.append(next_seed)
        next_dist = np.sum((coords - next_seed) ** 2, axis=1)
        distances = np.minimum(distances, next_dist)
    return [(int(seed[0]), int(seed[1])) for seed in seeds]


def grow_regions(mask: np.ndarray, seeds: list[tuple[int, int]]) -> np.ndarray:
    h, w = mask.shape
    labels = np.zeros((h, w), dtype=np.uint16)
    q: deque[tuple[int, int]] = deque()
    for index, (y, x) in enumerate(seeds, start=1):
        if not mask[y, x]:
            continue
        labels[y, x] = index
        q.append((y, x))
    while q:
        cy, cx = q.popleft()
        label = labels[cy, cx]
        for ny, nx in ((cy - 1, cx), (cy + 1, cx), (cy, cx - 1), (cy, cx + 1)):
            if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and labels[ny, nx] == 0:
                labels[ny, nx] = label
                q.append((ny, nx))
    return labels


def country_color(country_id: int) -> tuple[int, int, int]:
    # Stable visually distinct palette from multiplicative hash values.
    r = 70 + ((country_id * 73) % 170)
    g = 70 + ((country_id * 151) % 170)
    b = 70 + ((country_id * 211) % 170)
    return int(r), int(g), int(b)


def make_outputs(source: Image.Image, land_mask: np.ndarray, labels: np.ndarray) -> dict:
    rgb = np.asarray(source.convert("RGB"), dtype=np.uint8)
    h, w = land_mask.shape
    preview = rgb.copy()
    id_rgb = np.zeros((h, w, 3), dtype=np.uint8)
    boundary = np.zeros((h, w), dtype=bool)
    manifest_countries = []

    for country_id in range(1, COUNTRY_COUNT + 1):
        ys, xs = np.where(labels == country_id)
        if len(xs) == 0:
            raise RuntimeError(f"Country {country_id} has no pixels")
        color = country_color(country_id)
        color_arr = np.array(color, dtype=np.uint8)
        preview[ys, xs] = (preview[ys, xs].astype(np.uint16) * 45 // 100 + color_arr.astype(np.uint16) * 55 // 100).astype(np.uint8)
        id_rgb[ys, xs, 0] = country_id & 255
        id_rgb[ys, xs, 1] = (country_id >> 8) & 255
        id_rgb[ys, xs, 2] = 0
        manifest_countries.append({
            "id": country_id,
            "name": f"Dulia Country {country_id:03d}",
            "color": {"r": color[0], "g": color[1], "b": color[2]},
            "pixelArea": int(len(xs)),
            "centroid": {"x": round(float(xs.mean()), 2), "y": round(float(ys.mean()), 2)},
            "bounds": {
                "minX": int(xs.min()),
                "minY": int(ys.min()),
                "maxX": int(xs.max()),
                "maxY": int(ys.max()),
            },
            "initialGovernmentType": "Unassigned",
            "initialDiplomacyStatus": "Neutral",
            "continentName": "Unassigned",
        })

    boundary[1:, :] |= labels[1:, :] != labels[:-1, :]
    boundary[:, 1:] |= labels[:, 1:] != labels[:, :-1]
    boundary &= land_mask
    preview[boundary] = np.array([18, 18, 18], dtype=np.uint8)

    manifest = {
        "planet": "Dulia",
        "sourceImage": str(SOURCE_IMAGE),
        "countryCount": COUNTRY_COUNT,
        "imageSize": {"width": w, "height": h},
        "countryIdEncoding": "RGB little-endian: id = R + (G * 256); ocean/background = 0",
        "notes": [
            "Generated from dulia.png by Tools/generate_dulia_countries.py.",
            "Country names, governments, continents, and diplomacy can be overwritten later without regenerating masks.",
            "The original source map is not modified.",
        ],
        "countries": manifest_countries,
    }
    return {"preview": preview, "id_rgb": id_rgb, "manifest": manifest}


def main() -> None:
    if not SOURCE_IMAGE.exists():
        raise FileNotFoundError(SOURCE_IMAGE)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    source = Image.open(SOURCE_IMAGE).convert("RGBA")
    arr = np.asarray(source)
    water = is_water_like(arr[..., :3], arr[..., 3])
    land = ~water

    components, sizes = connected_components(land)
    allocation = allocate_country_counts(sizes, COUNTRY_COUNT)
    rng = random.Random(RANDOM_SEED)
    seeds: list[tuple[int, int]] = []
    for component_id, count in sorted(allocation.items(), key=lambda item: item[0]):
        coords = np.argwhere(components == component_id)
        seeds.extend(choose_seeds(coords, count, rng))
    if len(seeds) != COUNTRY_COUNT:
        raise RuntimeError(f"Expected {COUNTRY_COUNT} seeds, got {len(seeds)}")

    labels = grow_regions(land, seeds)
    if int(labels.max()) != COUNTRY_COUNT:
        raise RuntimeError(f"Expected max label {COUNTRY_COUNT}, got {int(labels.max())}")

    outputs = make_outputs(source, land, labels)
    Image.fromarray(outputs["preview"], "RGB").save(OUTPUT_DIR / "Dulia_Country_Regions_Preview.png")
    Image.fromarray(outputs["id_rgb"], "RGB").save(OUTPUT_DIR / "Dulia_Country_Id_Map.png")
    Image.fromarray((land.astype(np.uint8) * 255), "L").save(OUTPUT_DIR / "Dulia_Land_Mask.png")
    with (OUTPUT_DIR / "Dulia_Country_Manifest.json").open("w", encoding="utf-8") as fh:
        json.dump(outputs["manifest"], fh, indent=2)

    print(f"Generated {COUNTRY_COUNT} Dulia countries")
    print(f"Output: {OUTPUT_DIR}")
    print(f"Land pixels: {int(land.sum())}")
    print(f"Components used: {len(allocation)}")


if __name__ == "__main__":
    main()