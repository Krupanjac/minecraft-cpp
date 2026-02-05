#!/usr/bin/env python3
"""Audit all .vxstruct files for structural completeness."""

import json
import os
import sys
from collections import defaultdict

STRUCTURES_DIR = "assets/structures"

FILES_TO_CHECK = [
    "city/office_tower.vxstruct",
    "city/apartment.vxstruct",
    "city/skyscraper.vxstruct",
    "city/warehouse.vxstruct",
    "city/city_hall.vxstruct",
    "city/shop.vxstruct",
    "city/park.vxstruct",
    "city/tall_apartment.vxstruct",
    "city/tower_block.vxstruct",
    "city/small_building.vxstruct",
    "city/road_section.vxstruct",
    "city/lamppost.vxstruct",
    "city/monument.vxstruct",
    "city/street_lamp.vxstruct",
    "village/large_house.vxstruct",
    "village/library.vxstruct",
    "village/church.vxstruct",
    "village/barn.vxstruct",
    "village/watchtower.vxstruct",
    "village/stable.vxstruct",
    "village/inn.vxstruct",
    "village/small_house.vxstruct",
    "village/blacksmith.vxstruct",
    "village/market_stall.vxstruct",
]

# Block types that count as "air" or non-structural (decorations on the inside)
INTERIOR_DECORATIONS = {"torch", "crafting_table", "furnace", "chest", "bed", "bookshelf",
                        "flower", "carpet", "sign", "ladder", "anvil", "brewing_stand",
                        "enchanting_table", "cauldron", "item_frame", "painting",
                        "flower_pot", "armor_stand", "hay_bale"}

# Types that don't count as wall blocks (transparent/non-solid) - doors are intentional gaps
DOOR_TYPES = {"door", "oak_door", "iron_door", "spruce_door", "birch_door", "dark_oak_door"}

def audit_structure(filepath):
    """Audit a single structure file. Returns list of issues."""
    issues = []
    
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)
    except Exception as e:
        issues.append(f"ERROR: Cannot parse file: {e}")
        return issues
    
    name = data.get("name", "Unknown")
    size = data.get("size", {})
    sx, sy, sz = size.get("x", 0), size.get("y", 0), size.get("z", 0)
    blocks = data.get("blocks", [])
    
    if not blocks:
        issues.append("NO BLOCKS found in file")
        return issues
    
    # Build a 3D grid of block types
    grid = {}
    for b in blocks:
        key = (b["x"], b["y"], b["z"])
        grid[key] = b["type"]
    
    # Get actual bounds
    all_x = [b["x"] for b in blocks]
    all_y = [b["y"] for b in blocks]
    all_z = [b["z"] for b in blocks]
    min_x, max_x = min(all_x), max(all_x)
    min_y, max_y = min(all_y), max(all_y)
    min_z, max_z = min(all_z), max(all_z)
    
    # Declared size
    info = f"Name: {name}, Declared size: {sx}x{sy}x{sz}, Actual bounds: x=[{min_x},{max_x}] y=[{min_y},{max_y}] z=[{min_z},{max_z}], Total blocks: {len(blocks)}"
    
    # Skip non-building structures
    tags = data.get("tags", [])
    category = data.get("category", "")
    is_building = True
    if any(t in tags for t in ["road", "lamp", "lamppost", "street_lamp", "park", "monument", "market"]):
        is_building = False
    if "road" in name.lower() or "lamp" in name.lower():
        is_building = False
    
    # ===== FLOOR CHECK (y=0) =====
    floor_y = min_y
    floor_missing = []
    for x in range(min_x, max_x + 1):
        for z in range(min_z, max_z + 1):
            if (x, floor_y, z) not in grid:
                floor_missing.append((x, floor_y, z))
    
    if floor_missing and is_building:
        issues.append(f"FLOOR (y={floor_y}): Missing {len(floor_missing)} blocks out of {(max_x-min_x+1)*(max_z-min_z+1)} total. Missing positions: {floor_missing[:20]}{'...' if len(floor_missing) > 20 else ''}")
    
    if not is_building:
        return [info] + (issues if issues else ["Non-building structure - wall/roof checks skipped"])
    
    # ===== WALL CHECK =====
    # For each y level from floor_y+1 to max_y-1 (walls, not roof), check 4 walls
    # Wall at x=min_x (front): all z from min_z to max_z should have blocks
    # Wall at x=max_x (back): all z from min_z to max_z should have blocks
    # Wall at z=min_z (left): all x from min_x to max_x should have blocks
    # Wall at z=max_z (right): all x from min_x to max_x should have blocks
    
    wall_issues = defaultdict(list)
    
    # Determine wall height (from floor+1 to roof-1, or max_y if there's a flat roof at max_y)
    # We check each y level from min_y+1 up to max_y-1 for wall presence
    wall_min_y = floor_y + 1
    wall_max_y = max_y  # include the top since roof might be at max_y
    
    for y in range(wall_min_y, wall_max_y + 1):
        # Front wall (x=min_x)
        for z in range(min_z, max_z + 1):
            if (min_x, y, z) not in grid:
                wall_issues["x=min (front wall)"].append((min_x, y, z))
        
        # Back wall (x=max_x)
        for z in range(min_z, max_z + 1):
            if (max_x, y, z) not in grid:
                wall_issues["x=max (back wall)"].append((max_x, y, z))
        
        # Left wall (z=min_z)
        for x in range(min_x, max_x + 1):
            if (x, y, min_z) not in grid:
                wall_issues["z=min (left wall)"].append((x, y, min_z))
        
        # Right wall (z=max_z)
        for x in range(min_x, max_x + 1):
            if (x, y, max_z) not in grid:
                wall_issues["z=max (right wall)"].append((x, y, max_z))
    
    for wall_name, missing in wall_issues.items():
        if missing:
            # Group by y level for better reporting
            by_y = defaultdict(list)
            for pos in missing:
                by_y[pos[1]].append(pos)
            
            detail_parts = []
            for y_level in sorted(by_y.keys()):
                positions = by_y[y_level]
                detail_parts.append(f"y={y_level}: {len(positions)} gaps at {positions[:10]}{'...' if len(positions) > 10 else ''}")
            
            issues.append(f"WALL {wall_name}: {len(missing)} total missing blocks. By level: {'; '.join(detail_parts[:5])}{'...' if len(detail_parts) > 5 else ''}")
    
    # ===== ROOF CHECK =====
    # Check if the top layer (max_y) covers the full footprint
    roof_y = max_y
    roof_missing = []
    for x in range(min_x, max_x + 1):
        for z in range(min_z, max_z + 1):
            if (x, roof_y, z) not in grid:
                roof_missing.append((x, roof_y, z))
    
    if roof_missing:
        total_roof = (max_x - min_x + 1) * (max_z - min_z + 1)
        present = total_roof - len(roof_missing)
        coverage = present / total_roof * 100 if total_roof > 0 else 0
        issues.append(f"ROOF (y={roof_y}): Missing {len(roof_missing)}/{total_roof} blocks ({coverage:.1f}% coverage). Missing: {roof_missing[:20]}{'...' if len(roof_missing) > 20 else ''}")
    
    # ===== ADDITIONAL: Check for wall completeness per-level =====
    # Check if door gaps are reasonable (should be 1-2 blocks wide at y=1,y=2 on one wall)
    # Count total wall gaps vs door-sized gaps
    total_wall_gaps = sum(len(v) for v in wall_issues.values())
    
    # ===== Check for y-level continuity =====
    blocks_per_y = defaultdict(int)
    for b in blocks:
        blocks_per_y[b["y"]] += 1
    
    y_levels = sorted(blocks_per_y.keys())
    for i in range(len(y_levels) - 1):
        if y_levels[i+1] - y_levels[i] > 1:
            issues.append(f"GAP: No blocks between y={y_levels[i]} and y={y_levels[i+1]}")
    
    # Check for very sparse levels (potential missing walls)
    expected_perimeter = 2 * (max_x - min_x + 1) + 2 * (max_z - min_z + 1) - 4
    for y in range(wall_min_y, wall_max_y):
        count = blocks_per_y.get(y, 0)
        if count < expected_perimeter * 0.5 and count > 0:
            issues.append(f"SPARSE LEVEL: y={y} has only {count} blocks (expected at least ~{expected_perimeter} for perimeter)")
    
    return [info] + issues


def main():
    all_issues = {}
    
    for relpath in FILES_TO_CHECK:
        filepath = os.path.join(STRUCTURES_DIR, relpath)
        if not os.path.exists(filepath):
            all_issues[relpath] = [f"FILE NOT FOUND: {filepath}"]
            continue
        
        result = audit_structure(filepath)
        all_issues[relpath] = result
    
    # Print report
    print("=" * 100)
    print("STRUCTURE AUDIT REPORT")
    print("=" * 100)
    
    files_with_issues = 0
    total_issues = 0
    
    for relpath, issues in all_issues.items():
        print(f"\n{'─' * 80}")
        print(f"FILE: {relpath}")
        print(f"{'─' * 80}")
        
        # First line is always info
        if issues:
            print(f"  {issues[0]}")
            
        actual_issues = [i for i in issues[1:] if not i.startswith("Non-building")]
        
        if len(issues) <= 1 or (len(issues) == 2 and issues[1].startswith("Non-building")):
            print("  ✓ No issues found" if len(issues) <= 1 else f"  {issues[1]}")
        else:
            files_with_issues += 1
            for issue in issues[1:]:
                total_issues += 1
                print(f"  ✗ {issue}")
    
    print(f"\n{'=' * 100}")
    print(f"SUMMARY: {files_with_issues} files with issues, {total_issues} total issues found across {len(FILES_TO_CHECK)} files checked")
    print(f"{'=' * 100}")


if __name__ == "__main__":
    main()
