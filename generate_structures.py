"""Generate .vxstruct files for village and city structures."""
import json
import os

def make_struct(name, author, category, requires_flat, tags, size, blocks):
    return {
        "name": name,
        "author": author,
        "category": category,
        "requires_flat": requires_flat,
        "min_ground_coverage": 0.8,
        "tags": tags,
        "size": {"x": size[0], "y": size[1], "z": size[2]},
        "blocks": [{"x": b[0], "y": b[1], "z": b[2], "type": b[3]} for b in blocks]
    }

def save(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        json.dump(data, f, indent=2)
    print(f"  Created: {path} ({len(data['blocks'])} blocks)")

# Helper: generate a rectangular floor
def floor_rect(x1, z1, x2, z2, y, block):
    blocks = []
    for x in range(x1, x2+1):
        for z in range(z1, z2+1):
            blocks.append((x, y, z, block))
    return blocks

# Helper: generate walls (perimeter) at height y
def walls_rect(x1, z1, x2, z2, y, block, door_pos=None):
    blocks = []
    for x in range(x1, x2+1):
        for z in [z1, z2]:
            if door_pos and (x, z) == door_pos:
                continue
            blocks.append((x, y, z, block))
    for z in range(z1+1, z2):
        for x in [x1, x2]:
            blocks.append((x, y, z, block))
    return blocks

# Helper: wall with windows (glass at regular intervals)
def walls_with_windows(x1, z1, x2, z2, y, wall_block, window_block="glass", door_pos=None, window_interval=2):
    blocks = []
    # Front and back walls (z=z1, z=z2)
    for z in [z1, z2]:
        for x in range(x1, x2+1):
            if door_pos and (x, z) == door_pos:
                continue
            if (x - x1) % window_interval == (window_interval // 2) and x != x1 and x != x2:
                blocks.append((x, y, z, window_block))
            else:
                blocks.append((x, y, z, wall_block))
    # Side walls (x=x1, x=x2)
    for x in [x1, x2]:
        for z in range(z1+1, z2):
            if (z - z1) % window_interval == (window_interval // 2) and z != z1 and z != z2:
                blocks.append((x, y, z, window_block))
            else:
                blocks.append((x, y, z, wall_block))
    return blocks

# Helper: pitched roof (ridge along X axis)
def pitched_roof_x(x1, z1, x2, z2, y_start, block):
    """Roof with ridge running along X axis"""
    blocks = []
    width = z2 - z1 + 1
    half = width // 2
    for layer in range(half + 1):
        zy1 = z1 + layer
        zy2 = z2 - layer
        y = y_start + layer
        if zy1 > zy2:
            break
        for x in range(x1, x2 + 1):
            blocks.append((x, y, zy1, block))
            if zy2 != zy1:
                blocks.append((x, y, zy2, block))
    return blocks

# Helper: flat roof
def flat_roof(x1, z1, x2, z2, y, block):
    return floor_rect(x1, z1, x2, z2, y, block)

# Helper: corner pillars
def pillars(corners, y1, y2, block):
    blocks = []
    for (x, z) in corners:
        for y in range(y1, y2+1):
            blocks.append((x, y, z, block))
    return blocks

# ==================== VILLAGE STRUCTURES ====================

def village_large_house():
    """Large 2-story house: 9x8x9"""
    sx, sy, sz = 9, 8, 9
    blocks = []
    # Floor
    blocks += floor_rect(0, 0, 8, 8, 0, "cobblestone")
    # Interior floor
    blocks += floor_rect(1, 1, 7, 7, 0, "oak_planks")
    # Corner logs
    blocks += pillars([(0,0),(8,0),(0,8),(8,8)], 1, 6, "oak_log")
    # Walls with windows - floor 1
    for y in [1, 2, 3]:
        door = (4, 0) if y <= 2 else None
        if y == 2:
            blocks += walls_with_windows(0, 0, 8, 8, y, "oak_planks", "glass", door, 2)
        else:
            blocks += walls_rect(0, 0, 8, 8, y, "oak_planks", door)
    # Second floor
    blocks += floor_rect(1, 1, 7, 7, 4, "oak_planks")
    # Walls - floor 2
    for y in [4, 5, 6]:
        if y == 5:
            blocks += walls_with_windows(0, 0, 8, 8, y, "oak_planks", "glass", None, 2)
        else:
            blocks += walls_rect(0, 0, 8, 8, y, "oak_planks")
    # Roof
    blocks += pitched_roof_x(0, 0, 8, 8, 7, "spruce_planks")
    return make_struct("Large House", "generator", "village_house", True,
                      ["house", "large", "residential"], [sx, sy, sz], blocks)

def village_library():
    """Library: 8x6x7"""
    sx, sy, sz = 8, 6, 7
    blocks = []
    # Floor
    blocks += floor_rect(0, 0, 7, 6, 0, "cobblestone")
    blocks += floor_rect(1, 1, 6, 5, 0, "oak_planks")
    # Corner logs
    blocks += pillars([(0,0),(7,0),(0,6),(7,6)], 1, 4, "oak_log")
    # Walls
    for y in [1, 2, 3, 4]:
        door = (3, 0) if y <= 2 else None
        if y == 2 or y == 3:
            blocks += walls_with_windows(0, 0, 7, 6, y, "oak_planks", "glass", door, 2)
        else:
            blocks += walls_rect(0, 0, 7, 6, y, "oak_planks", door)
    # Bookshelves inside
    for z in [1, 5]:
        for x in [1, 2, 3, 4, 5, 6]:
            blocks.append((x, 1, z, "bookshelf"))
            blocks.append((x, 2, z, "bookshelf"))
    # Roof
    blocks += pitched_roof_x(0, 0, 7, 6, 5, "spruce_planks")
    return make_struct("Village Library", "generator", "village_building", True,
                      ["library", "building", "education"], [sx, sy, sz], blocks)

def village_church():
    """Church with tower: 7x10x10"""
    sx, sy, sz = 7, 10, 10
    blocks = []
    # Main body floor
    blocks += floor_rect(0, 0, 6, 6, 0, "cobblestone")
    blocks += floor_rect(1, 1, 5, 5, 0, "stone_bricks")
    # Walls - main body
    for y in [1, 2, 3, 4, 5]:
        door = (3, 0) if y <= 2 else None
        if y == 3:
            blocks += walls_with_windows(0, 0, 6, 6, y, "stone_bricks", "glass", door, 2)
        else:
            blocks += walls_rect(0, 0, 6, 6, y, "stone_bricks", door)
    # Main roof
    blocks += pitched_roof_x(0, 0, 6, 6, 6, "stone_bricks")
    # Bell tower (back corner 0-2, 5-7)
    for y in range(1, 9):
        blocks.append((0, y, 5, "stone_bricks"))
        blocks.append((2, y, 5, "stone_bricks"))
        blocks.append((0, y, 7, "stone_bricks"))
        blocks.append((2, y, 7, "stone_bricks"))
        if y >= 6:
            # Open arches at top
            pass
        else:
            blocks.append((1, y, 5, "stone_bricks"))
            blocks.append((1, y, 7, "stone_bricks"))
            blocks.append((0, y, 6, "stone_bricks"))
            blocks.append((2, y, 6, "stone_bricks"))
    # Tower cap
    blocks += floor_rect(0, 5, 2, 7, 9, "stone_bricks")
    # Glowstone inside (light)
    blocks.append((1, 8, 6, "glowstone"))
    return make_struct("Village Church", "generator", "village_building", True,
                      ["church", "building", "religious"], [sx, sy, sz], blocks)

def village_market_stall():
    """Market stall: 6x4x5"""
    sx, sy, sz = 6, 4, 5
    blocks = []
    # Counter at y=0
    for x in range(6):
        blocks.append((x, 0, 0, "oak_planks"))
        blocks.append((x, 0, 4, "oak_planks"))
    for z in range(5):
        blocks.append((0, 0, z, "oak_planks"))
        blocks.append((5, 0, z, "oak_planks"))
    # Crafting tables as display
    blocks.append((2, 0, 2, "crafting_table"))
    blocks.append((3, 0, 2, "crafting_table"))
    # Support posts
    blocks += pillars([(0,0),(5,0),(0,4),(5,4)], 1, 3, "oak_log")
    # Wool awning (colored)
    for x in range(6):
        for z in range(5):
            color = "red_wool" if (x + z) % 2 == 0 else "white_wool"
            blocks.append((x, 3, z, color))
    return make_struct("Market Stall", "generator", "village_building", True,
                      ["market", "stall", "trade"], [sx, sy, sz], blocks)

def village_barn():
    """Big barn: 10x7x8"""
    sx, sy, sz = 10, 7, 8
    blocks = []
    # Floor
    blocks += floor_rect(0, 0, 9, 7, 0, "cobblestone")
    blocks += floor_rect(1, 1, 8, 6, 0, "dirt")
    # Corner pillars
    blocks += pillars([(0,0),(9,0),(0,7),(9,7)], 1, 5, "spruce_log")
    # Mid pillars
    blocks += pillars([(0,3),(0,4),(9,3),(9,4)], 1, 5, "spruce_log")
    # Walls
    for y in [1, 2, 3, 4, 5]:
        door = (4, 0) if y <= 3 else None
        door2 = (5, 0) if y <= 3 else None
        if y == 2 or y == 4:
            b = walls_with_windows(0, 0, 9, 7, y, "spruce_planks", "glass", door, 3)
        else:
            b = walls_rect(0, 0, 9, 7, y, "spruce_planks", door)
        # Remove door2 blocks
        if door2 and y <= 3:
            b = [(bx,by,bz,bt) for (bx,by,bz,bt) in b if not (bx == door2[0] and bz == door2[1])]
        blocks += b
    # Hay bales inside (use yellow wool as hay)
    for x in [1, 2]:
        for z in [1, 2]:
            blocks.append((x, 1, z, "yellow_wool"))
            blocks.append((x, 2, z, "yellow_wool"))
    for x in [7, 8]:
        for z in [5, 6]:
            blocks.append((x, 1, z, "yellow_wool"))
            blocks.append((x, 2, z, "yellow_wool"))
    # Roof
    blocks += pitched_roof_x(0, 0, 9, 7, 6, "spruce_planks")
    return make_struct("Village Barn", "generator", "village_house", True,
                      ["barn", "large", "agriculture"], [sx, sy, sz], blocks)

def village_watchtower():
    """Watchtower: 5x12x5"""
    sx, sy, sz = 5, 12, 5
    blocks = []
    # Base floor
    blocks += floor_rect(0, 0, 4, 4, 0, "cobblestone")
    # Walls going up
    for y in range(1, 9):
        door = (2, 0) if y <= 2 else None
        blocks += walls_rect(0, 0, 4, 4, y, "cobblestone", door)
    # Platform floor
    blocks += floor_rect(0, 0, 4, 4, 9, "oak_planks")
    # Railing
    blocks += walls_rect(0, 0, 4, 4, 10, "oak_planks")
    # Roof posts
    blocks += pillars([(0,0),(4,0),(0,4),(4,4)], 10, 11, "oak_log")
    # Roof
    for x in range(-1, 6):
        for z in range(-1, 6):
            if 0 <= x <= 4 and 0 <= z <= 4:
                blocks.append((x, 11, z, "spruce_planks"))
    # Glowstone for light
    blocks.append((2, 10, 2, "glowstone"))
    return make_struct("Watchtower", "generator", "village_building", True,
                      ["watchtower", "tower", "defense"], [sx, sy, sz], blocks)

def village_stable():
    """Stable: 8x4x6"""
    sx, sy, sz = 8, 4, 6
    blocks = []
    # Floor
    blocks += floor_rect(0, 0, 7, 5, 0, "cobblestone")
    blocks += floor_rect(1, 1, 6, 4, 0, "dirt")
    # Back wall and sides
    for y in [1, 2, 3]:
        # Back wall
        for x in range(8):
            blocks.append((x, y, 5, "spruce_planks"))
        # Side walls
        for z in range(6):
            blocks.append((0, y, z, "spruce_planks"))
            blocks.append((7, y, z, "spruce_planks"))
    # Dividers (stalls)
    for x in [2, 5]:
        for y in [1, 2]:
            for z in range(1, 5):
                blocks.append((x, y, z, "oak_planks"))
    # Support beams
    blocks += pillars([(0,0),(7,0),(0,5),(7,5),(3,5),(4,5)], 1, 3, "spruce_log")
    # Roof (flat with slight overhang)
    for x in range(-1, 9):
        for z in range(-1, 7):
            if 0 <= x <= 7 or 0 <= z <= 5:
                if -1 <= x <= 8 and -1 <= z <= 6:
                    blocks.append((x, 3, z, "spruce_planks"))
    return make_struct("Village Stable", "generator", "village_house", True,
                      ["stable", "animals", "agriculture"], [sx, sy, sz], blocks)

def village_garden():
    """Decorative garden: 8x2x8"""
    sx, sy, sz = 8, 2, 8
    blocks = []
    # Grass floor
    blocks += floor_rect(0, 0, 7, 7, 0, "grass")
    # Path through center
    for x in range(8):
        blocks.append((x, 0, 3, "cobblestone"))
        blocks.append((x, 0, 4, "cobblestone"))
    for z in range(8):
        blocks.append((3, 0, z, "cobblestone"))
        blocks.append((4, 0, z, "cobblestone"))
    # Flowers
    for x in [1, 2, 5, 6]:
        for z in [1, 2, 5, 6]:
            blocks.append((x, 1, z, "rose"))
    return make_struct("Village Garden", "generator", "village_decoration", True,
                      ["garden", "decoration", "nature"], [sx, sy, sz], blocks)

def village_well_large():
    """Larger village well with plaza: 7x5x7"""
    sx, sy, sz = 7, 5, 7
    blocks = []
    # Stone plaza
    blocks += floor_rect(0, 0, 6, 6, 0, "cobblestone")
    # Well walls
    for y in [0, 1, 2]:
        blocks.append((2, y, 2, "stone_bricks"))
        blocks.append((4, y, 2, "stone_bricks"))
        blocks.append((2, y, 4, "stone_bricks"))
        blocks.append((4, y, 4, "stone_bricks"))
        blocks.append((3, y, 2, "stone_bricks"))
        blocks.append((3, y, 4, "stone_bricks"))
        blocks.append((2, y, 3, "stone_bricks"))
        blocks.append((4, y, 3, "stone_bricks"))
    # Water inside
    blocks.append((3, 0, 3, "water"))
    # Support pillars
    blocks += pillars([(2,2),(4,2),(2,4),(4,4)], 3, 4, "oak_log")
    # Roof over well
    blocks += floor_rect(1, 1, 5, 5, 4, "spruce_planks")
    return make_struct("Large Well", "generator", "village_well", True,
                      ["well", "water", "plaza"], [sx, sy, sz], blocks)

def village_inn():
    """Inn/tavern: 10x7x8"""
    sx, sy, sz = 10, 7, 8
    blocks = []
    # Floor
    blocks += floor_rect(0, 0, 9, 7, 0, "cobblestone")
    blocks += floor_rect(1, 1, 8, 6, 0, "oak_planks")
    # Corner logs
    blocks += pillars([(0,0),(9,0),(0,7),(9,7)], 1, 5, "oak_log")
    # Mid pillars
    blocks += pillars([(4,0),(5,0)], 1, 5, "oak_log")
    # Walls floor 1
    for y in [1, 2, 3]:
        door = (4, 0) if y <= 2 else None
        if y == 2:
            blocks += walls_with_windows(0, 0, 9, 7, y, "oak_planks", "glass", door, 2)
        else:
            blocks += walls_rect(0, 0, 9, 7, y, "oak_planks", door)
    # Second floor
    blocks += floor_rect(1, 1, 8, 6, 4, "oak_planks")
    # Walls floor 2
    for y in [4, 5]:
        if y == 5:
            blocks += walls_with_windows(0, 0, 9, 7, y, "oak_planks", "glass", None, 2)
        else:
            blocks += walls_rect(0, 0, 9, 7, y, "oak_planks")
    # Bar counter inside
    for x in range(2, 8):
        blocks.append((x, 1, 3, "oak_planks"))
    # Glowstone lights
    blocks.append((3, 3, 2, "glowstone"))
    blocks.append((6, 3, 5, "glowstone"))
    # Roof
    blocks += pitched_roof_x(0, 0, 9, 7, 6, "spruce_planks")
    return make_struct("Village Inn", "generator", "village_building", True,
                      ["inn", "tavern", "building"], [sx, sy, sz], blocks)

def village_farm_large():
    """Large farm plot: 10x2x8"""
    sx, sy, sz = 10, 2, 8
    blocks = []
    # Border
    for x in range(10):
        blocks.append((x, 0, 0, "oak_log"))
        blocks.append((x, 0, 7, "oak_log"))
    for z in range(8):
        blocks.append((0, 0, z, "oak_log"))
        blocks.append((9, 0, z, "oak_log"))
    # Farmland rows
    for x in range(1, 9):
        for z in range(1, 7):
            if z == 3 or z == 4:
                blocks.append((x, 0, z, "water"))
            else:
                blocks.append((x, 0, z, "farmland"))
    return make_struct("Large Farm", "generator", "village_farm", True,
                      ["farm", "agriculture", "large"], [sx, sy, sz], blocks)


# ==================== CITY STRUCTURES ====================

def city_office_tower():
    """Tall office building: 10x18x10"""
    sx, sy, sz = 10, 18, 10
    blocks = []
    # Foundation
    blocks += floor_rect(0, 0, 9, 9, 0, "stone_bricks")
    # Each floor: 4 blocks tall (floor slab + 3 wall)
    for floor_num in range(4):
        base_y = floor_num * 4 + 1
        # Floor slab
        if floor_num > 0:
            blocks += floor_rect(0, 0, 9, 9, base_y - 1, "stone_bricks")
        # Walls with glass facade
        for y_off in range(3):
            y = base_y + y_off
            door = (4, 0) if floor_num == 0 and y_off <= 1 else None
            door2 = (5, 0) if floor_num == 0 and y_off <= 1 else None
            if y_off == 1:
                # Window row - mostly glass
                b = []
                for side_z in [0, 9]:
                    for x in range(10):
                        if door and (x, side_z) == door: continue
                        if door2 and (x, side_z) == door2: continue
                        if x == 0 or x == 9:
                            b.append((x, y, side_z, "iron_block"))
                        else:
                            b.append((x, y, side_z, "glass"))
                for side_x in [0, 9]:
                    for z in range(1, 9):
                        if side_x == 0 or side_x == 9:
                            b.append((side_x, y, z, "iron_block"))
                        else:
                            b.append((side_x, y, z, "glass"))
                # Side walls - glass
                for z in range(1, 9):
                    b.append((0, y, z, "iron_block"))
                    b.append((9, y, z, "iron_block"))
                blocks += b
            else:
                # Solid rows with iron frame
                b = []
                for side_z in [0, 9]:
                    for x in range(10):
                        if door and (x, side_z) == door: continue
                        if door2 and (x, side_z) == door2: continue
                        b.append((x, y, side_z, "stone_bricks"))
                for z in range(1, 9):
                    b.append((0, y, z, "stone_bricks"))
                    b.append((9, y, z, "stone_bricks"))
                blocks += b
    # Roof
    blocks += floor_rect(0, 0, 9, 9, 16, "stone_bricks")
    blocks += floor_rect(1, 1, 8, 8, 17, "stone_bricks")
    # Roof lights
    blocks.append((3, 17, 3, "glowstone"))
    blocks.append((6, 17, 6, "glowstone"))
    return make_struct("Office Tower", "generator", "city_building", True,
                      ["office", "tower", "tall"], [sx, sy, sz], blocks)

def city_apartment():
    """Apartment building: 8x14x8"""
    sx, sy, sz = 8, 14, 8
    blocks = []
    # Foundation
    blocks += floor_rect(0, 0, 7, 7, 0, "stone_bricks")
    # 3 floors, each 4 blocks
    for floor_num in range(3):
        base_y = floor_num * 4 + 1
        if floor_num > 0:
            blocks += floor_rect(0, 0, 7, 7, base_y - 1, "stone_bricks")
        for y_off in range(3):
            y = base_y + y_off
            door = (3, 0) if floor_num == 0 and y_off <= 1 else None
            if y_off == 1:
                # Window row
                blocks += walls_with_windows(0, 0, 7, 7, y, "bricks", "glass", door, 2)
            else:
                blocks += walls_rect(0, 0, 7, 7, y, "bricks", door)
    # Top floor/roof
    blocks += floor_rect(0, 0, 7, 7, 13, "stone_bricks")
    # Balcony railing on top
    blocks += walls_rect(0, 0, 7, 7, 13, "stone_bricks")
    return make_struct("Apartment Building", "generator", "city_building", True,
                      ["apartment", "residential", "tall"], [sx, sy, sz], blocks)

def city_skyscraper():
    """Tall skyscraper: 8x22x8"""
    sx, sy, sz = 8, 22, 8
    blocks = []
    # Foundation
    blocks += floor_rect(0, 0, 7, 7, 0, "iron_block")
    # 5 floors, each 4 blocks
    for floor_num in range(5):
        base_y = floor_num * 4 + 1
        if floor_num > 0:
            blocks += floor_rect(0, 0, 7, 7, base_y - 1, "stone_bricks")
        for y_off in range(3):
            y = base_y + y_off
            door = (3, 0) if floor_num == 0 and y_off <= 1 else None
            door2 = (4, 0) if floor_num == 0 and y_off <= 1 else None
            # Glass curtain wall with iron pillars at corners
            b = []
            for side_z in [0, 7]:
                for x in range(8):
                    if door and (x, side_z) == door: continue
                    if door2 and (x, side_z) == door2: continue
                    if x == 0 or x == 7:
                        b.append((x, y, side_z, "iron_block"))
                    else:
                        b.append((x, y, side_z, "glass"))
            for z in range(1, 7):
                b.append((0, y, z, "iron_block"))
                b.append((7, y, z, "iron_block"))
                if y_off == 1:
                    # Inner glass on sides too
                    pass
            blocks += b
    # Roof
    blocks += floor_rect(0, 0, 7, 7, 21, "iron_block")
    # Spire
    blocks.append((3, 21, 3, "iron_block"))
    blocks.append((4, 21, 4, "iron_block"))
    blocks.append((3, 21, 4, "iron_block"))
    blocks.append((4, 21, 3, "iron_block"))
    return make_struct("Skyscraper", "generator", "city_skyscraper", True,
                      ["skyscraper", "tall", "modern"], [sx, sy, sz], blocks)

def city_warehouse():
    """Large warehouse: 12x6x10"""
    sx, sy, sz = 12, 6, 10
    blocks = []
    # Floor
    blocks += floor_rect(0, 0, 11, 9, 0, "stone_bricks")
    # Walls
    for y in range(1, 5):
        door = (5, 0) if y <= 2 else None
        door2 = (6, 0) if y <= 2 else None
        b = walls_rect(0, 0, 11, 9, y, "stone_bricks", door)
        if door2 and y <= 2:
            b = [(bx,by,bz,bt) for (bx,by,bz,bt) in b if not (bx == door2[0] and bz == door2[1])]
        blocks += b
    # Flat roof
    blocks += floor_rect(0, 0, 11, 9, 5, "stone_bricks")
    # Crates inside (bookshelves and crafting tables as crates)
    for x in [2, 3, 8, 9]:
        for z in [2, 3, 6, 7]:
            blocks.append((x, 1, z, "oak_planks"))
            if (x + z) % 3 != 0:
                blocks.append((x, 2, z, "oak_planks"))
    # Lights
    blocks.append((3, 4, 3, "glowstone"))
    blocks.append((8, 4, 6, "glowstone"))
    return make_struct("Warehouse", "generator", "city_building", True,
                      ["warehouse", "storage", "industrial"], [sx, sy, sz], blocks)

def city_hall():
    """City hall: 12x10x10"""
    sx, sy, sz = 12, 10, 10
    blocks = []
    # Grand foundation
    blocks += floor_rect(0, 0, 11, 9, 0, "stone_bricks")
    blocks += floor_rect(1, 1, 10, 8, 0, "chiseled_stone_bricks")
    # Front columns (pillars along z=0)
    for x in [1, 3, 5, 6, 8, 10]:
        blocks += pillars([(x, 0)], 1, 6, "stone_bricks")
    # Walls
    for y in range(1, 7):
        door = (5, 0) if y <= 2 else None
        door2 = (6, 0) if y <= 2 else None
        # Back wall
        for x in range(12):
            blocks.append((x, y, 9, "stone_bricks"))
        # Side walls
        for z in range(1, 9):
            blocks.append((0, y, z, "stone_bricks"))
            blocks.append((11, y, z, "stone_bricks"))
        # Front - openings between columns (window level)
        if y == 3 or y == 4:
            for x in range(12):
                if door and x == door[0]: continue
                if door2 and x == door2[0]: continue
                if x in [2, 4, 7, 9]:
                    blocks.append((x, y, 0, "glass"))
                else:
                    blocks.append((x, y, 0, "stone_bricks"))
        else:
            for x in range(12):
                if door and x == door[0] and y <= 2: continue
                if door2 and x == door2[0] and y <= 2: continue
                blocks.append((x, y, 0, "stone_bricks"))
    # Internal floors
    blocks += floor_rect(1, 1, 10, 8, 4, "stone_bricks")
    # Roof with pediment
    blocks += floor_rect(0, 0, 11, 9, 7, "stone_bricks")
    # Roof edge ornamentation
    blocks += walls_rect(0, 0, 11, 9, 8, "stone_bricks")
    # Roof top
    blocks += floor_rect(1, 1, 10, 8, 9, "stone_bricks")
    # Lights
    blocks.append((5, 6, 4, "glowstone"))
    blocks.append((6, 6, 4, "glowstone"))
    return make_struct("City Hall", "generator", "city_building", True,
                      ["city_hall", "government", "grand"], [sx, sy, sz], blocks)

def city_shop():
    """Shop building: 7x5x7"""
    sx, sy, sz = 7, 5, 7
    blocks = []
    # Floor
    blocks += floor_rect(0, 0, 6, 6, 0, "stone_bricks")
    blocks += floor_rect(1, 1, 5, 5, 0, "oak_planks")
    # Walls
    for y in [1, 2, 3]:
        door = (3, 0) if y <= 2 else None
        if y == 2:
            # Big glass storefront on front wall
            b = []
            for x in range(7):
                if door and x == door[0]: continue
                if x >= 1 and x <= 5:
                    b.append((x, y, 0, "glass"))
                else:
                    b.append((x, y, 0, "bricks"))
            for x in range(7):
                b.append((x, y, 6, "bricks"))
            for z in range(1, 6):
                b.append((0, y, z, "bricks"))
                b.append((6, y, z, "bricks"))
            blocks += b
        else:
            blocks += walls_rect(0, 0, 6, 6, y, "bricks", door)
    # Counter inside
    for x in range(2, 5):
        blocks.append((x, 1, 4, "oak_planks"))
    # Flat roof
    blocks += floor_rect(0, 0, 6, 6, 4, "stone_bricks")
    # Awning detail on front
    blocks.append((1, 3, 0, "red_wool"))
    blocks.append((2, 3, 0, "red_wool"))
    blocks.append((4, 3, 0, "red_wool"))
    blocks.append((5, 3, 0, "red_wool"))
    return make_struct("City Shop", "generator", "city_building", True,
                      ["shop", "commercial", "small"], [sx, sy, sz], blocks)

def city_park():
    """City park with benches: 10x3x10"""
    sx, sy, sz = 10, 3, 10
    blocks = []
    # Grass base
    blocks += floor_rect(0, 0, 9, 9, 0, "grass")
    # Paths
    for x in range(10):
        blocks.append((x, 0, 4, "cobblestone"))
        blocks.append((x, 0, 5, "cobblestone"))
    for z in range(10):
        blocks.append((4, 0, z, "cobblestone"))
        blocks.append((5, 0, z, "cobblestone"))
    # Trees (oak logs + leaves)
    for (tx, tz) in [(1, 1), (8, 1), (1, 8), (8, 8)]:
        blocks.append((tx, 1, tz, "oak_log"))
        blocks.append((tx, 2, tz, "oak_log"))
        # Small leaf canopy
        for dx in [-1, 0, 1]:
            for dz in [-1, 0, 1]:
                lx, lz = tx + dx, tz + dz
                if 0 <= lx <= 9 and 0 <= lz <= 9:
                    blocks.append((lx, 2, lz, "oak_leaves"))
        blocks.append((tx, 2, tz, "oak_log"))  # trunk through leaves
    # Flowers
    for x in [2, 3, 6, 7]:
        for z in [2, 3, 6, 7]:
            blocks.append((x, 1, z, "rose"))
    # Benches (oak stairs = oak_planks on cobblestone)
    blocks.append((2, 1, 4, "oak_planks"))
    blocks.append((7, 1, 5, "oak_planks"))
    # Lamp posts
    blocks.append((4, 1, 4, "cobblestone"))
    blocks.append((4, 2, 4, "glowstone"))
    blocks.append((5, 1, 5, "cobblestone"))
    blocks.append((5, 2, 5, "glowstone"))
    return make_struct("City Park", "generator", "city_park", True,
                      ["park", "nature", "recreation"], [sx, sy, sz], blocks)

def city_tall_apartment():
    """Tall modern apartment: 10x16x8"""
    sx, sy, sz = 10, 16, 8
    blocks = []
    # Foundation
    blocks += floor_rect(0, 0, 9, 7, 0, "stone_bricks")
    # 4 floors
    for floor_num in range(4):
        base_y = floor_num * 4 + 1
        if floor_num > 0:
            blocks += floor_rect(0, 0, 9, 7, base_y - 1, "stone_bricks")
        for y_off in range(3):
            y = base_y + y_off
            door = (4, 0) if floor_num == 0 and y_off <= 1 else None
            if y_off == 1:
                # Alternating glass and white wool (concrete look)
                b = []
                for side_z in [0, 7]:
                    for x in range(10):
                        if door and (x, side_z) == door: continue
                        if x % 2 == 0:
                            b.append((x, y, side_z, "glass"))
                        else:
                            b.append((x, y, side_z, "white_wool"))
                for z in range(1, 7):
                    b.append((0, y, z, "white_wool"))
                    b.append((9, y, z, "white_wool"))
                blocks += b
            else:
                b = []
                for side_z in [0, 7]:
                    for x in range(10):
                        if door and (x, side_z) == door and y <= 2: continue
                        b.append((x, y, side_z, "white_wool"))
                for z in range(1, 7):
                    b.append((0, y, z, "white_wool"))
                    b.append((9, y, z, "white_wool"))
                blocks += b
    # Roof
    blocks += floor_rect(0, 0, 9, 7, 16, "stone_bricks")
    return make_struct("Tall Apartment", "generator", "city_building", True,
                      ["apartment", "modern", "tall"], [sx, sy, sz], blocks)

def city_tower_block():
    """Massive tower block: 10x20x10"""
    sx, sy, sz = 10, 20, 10
    blocks = []
    # Foundation
    blocks += floor_rect(0, 0, 9, 9, 0, "iron_block")
    # 5 floors
    for floor_num in range(5):
        base_y = floor_num * 4 + 1
        if floor_num > 0:
            blocks += floor_rect(0, 0, 9, 9, base_y - 1, "stone_bricks")
        for y_off in range(3):
            y = base_y + y_off
            door = (4, 0) if floor_num == 0 and y_off <= 1 else None
            door2 = (5, 0) if floor_num == 0 and y_off <= 1 else None
            b = []
            for side_z in [0, 9]:
                for x in range(10):
                    if door and (x, side_z) == door: continue
                    if door2 and (x, side_z) == door2: continue
                    if y_off == 1 and x > 0 and x < 9:
                        b.append((x, y, side_z, "glass"))
                    elif x == 0 or x == 9:
                        b.append((x, y, side_z, "gray_wool"))
                    else:
                        b.append((x, y, side_z, "gray_wool"))
            for z in range(1, 9):
                if y_off == 1:
                    b.append((0, y, z, "gray_wool"))
                    b.append((9, y, z, "gray_wool"))
                else:
                    b.append((0, y, z, "gray_wool"))
                    b.append((9, y, z, "gray_wool"))
            blocks += b
    # Roof
    blocks += floor_rect(0, 0, 9, 9, 20, "gray_wool")
    # Lights
    for fy in range(5):
        base_y = fy * 4 + 1
        blocks.append((4, base_y + 2, 4, "glowstone"))
        blocks.append((5, base_y + 2, 5, "glowstone"))
    return make_struct("Tower Block", "generator", "city_skyscraper", True,
                      ["tower", "residential", "massive"], [sx, sy, sz], blocks)

def city_road_section():
    """Road section: 10x1x10"""
    sx, sy, sz = 10, 1, 10
    blocks = []
    # Sidewalks
    for x in range(10):
        blocks.append((x, 0, 0, "stone_bricks"))
        blocks.append((x, 0, 1, "stone_bricks"))
        blocks.append((x, 0, 8, "stone_bricks"))
        blocks.append((x, 0, 9, "stone_bricks"))
    # Road
    for x in range(10):
        for z in range(2, 8):
            if z == 4 or z == 5:
                blocks.append((x, 0, z, "gravel"))  # Center line
            else:
                blocks.append((x, 0, z, "cobblestone"))
    return make_struct("Road Section", "generator", "city_road", True,
                      ["road", "infrastructure"], [sx, sy, sz], blocks)

def city_lamppost():
    """Street lamp: 1x5x1"""
    sx, sy, sz = 1, 5, 1
    blocks = []
    blocks.append((0, 0, 0, "stone_bricks"))
    blocks.append((0, 1, 0, "cobblestone"))
    blocks.append((0, 2, 0, "cobblestone"))
    blocks.append((0, 3, 0, "cobblestone"))
    blocks.append((0, 4, 0, "glowstone"))
    return make_struct("Street Lamp", "generator", "city_decoration", True,
                      ["lamp", "lighting", "street"], [sx, sy, sz], blocks)

def city_monument():
    """Small monument/statue: 5x7x5"""
    sx, sy, sz = 5, 7, 5
    blocks = []
    # Base platform
    blocks += floor_rect(0, 0, 4, 4, 0, "stone_bricks")
    blocks += floor_rect(1, 1, 3, 3, 1, "stone_bricks")
    # Pillar
    for y in range(2, 6):
        blocks.append((2, y, 2, "chiseled_stone_bricks"))
    # Top
    blocks += floor_rect(1, 1, 3, 3, 6, "stone_bricks")
    # Decorative corners
    blocks.append((1, 6, 1, "glowstone"))
    blocks.append((3, 6, 1, "glowstone"))
    blocks.append((1, 6, 3, "glowstone"))
    blocks.append((3, 6, 3, "glowstone"))
    return make_struct("Monument", "generator", "city_decoration", True,
                      ["monument", "statue", "decoration"], [sx, sy, sz], blocks)


# ==================== GENERATE ALL ====================

base_dir = os.path.dirname(os.path.abspath(__file__))
village_dir = os.path.join(base_dir, "assets", "structures", "village")
city_dir = os.path.join(base_dir, "assets", "structures", "city")

print("=== Generating Village Structures ===")
save(os.path.join(village_dir, "large_house.vxstruct"), village_large_house())
save(os.path.join(village_dir, "library.vxstruct"), village_library())
save(os.path.join(village_dir, "church.vxstruct"), village_church())
save(os.path.join(village_dir, "market_stall.vxstruct"), village_market_stall())
save(os.path.join(village_dir, "barn.vxstruct"), village_barn())
save(os.path.join(village_dir, "watchtower.vxstruct"), village_watchtower())
save(os.path.join(village_dir, "stable.vxstruct"), village_stable())
save(os.path.join(village_dir, "garden.vxstruct"), village_garden())
save(os.path.join(village_dir, "large_well.vxstruct"), village_well_large())
save(os.path.join(village_dir, "inn.vxstruct"), village_inn())
save(os.path.join(village_dir, "large_farm.vxstruct"), village_farm_large())

print("\n=== Generating City Structures ===")
save(os.path.join(city_dir, "office_tower.vxstruct"), city_office_tower())
save(os.path.join(city_dir, "apartment.vxstruct"), city_apartment())
save(os.path.join(city_dir, "skyscraper.vxstruct"), city_skyscraper())
save(os.path.join(city_dir, "warehouse.vxstruct"), city_warehouse())
save(os.path.join(city_dir, "city_hall.vxstruct"), city_hall())
save(os.path.join(city_dir, "shop.vxstruct"), city_shop())
save(os.path.join(city_dir, "park.vxstruct"), city_park())
save(os.path.join(city_dir, "tall_apartment.vxstruct"), city_tall_apartment())
save(os.path.join(city_dir, "tower_block.vxstruct"), city_tower_block())
save(os.path.join(city_dir, "road_section.vxstruct"), city_road_section())
save(os.path.join(city_dir, "lamppost.vxstruct"), city_lamppost())
save(os.path.join(city_dir, "monument.vxstruct"), city_monument())

print(f"\nDone! Generated {11 + 12} structure files.")

# Also clean up None entries in blocks from bad code
import glob
for path in glob.glob(os.path.join(base_dir, "assets", "structures", "**", "*.vxstruct"), recursive=True):
    with open(path, 'r') as f:
        data = json.load(f)
    original_count = len(data['blocks'])
    data['blocks'] = [b for b in data['blocks'] if b is not None]
    cleaned_count = len(data['blocks'])
    if original_count != cleaned_count:
        with open(path, 'w') as f:
            json.dump(data, f, indent=2)
        print(f"  Cleaned {original_count - cleaned_count} null blocks from {os.path.basename(path)}")
