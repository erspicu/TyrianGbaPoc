/* Low-level GBA OAM shape and projectile presentation adapter. */

static void put_sprite_with_attr1(
    s16 x,
    s16 y,
    u16 tile,
    u8 palette,
    u16 size,
    u8 priority,
    u16 attr1_flags
)
{
    OBJATTR *object;
    s16 dimension = size == ATTR1_SIZE_64 ? 64 :
                    (size == ATTR1_SIZE_32 ? 32 :
                    (size == ATTR1_SIZE_16 ? 16 : 8));
    if (oam_count >= SPRITE_LIMIT) return;
    if (x <= -dimension || x >= SCREEN_WIDTH ||
        y <= -dimension || y >= SCREEN_HEIGHT) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00FF) | ATTR0_NORMAL |
                    ATTR0_COLOR_16 | ATTR0_SQUARE;
    object->attr1 = (x & 0x01FF) | size | attr1_flags;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority) |
                    ATTR2_PALETTE(palette);
    object->dummy = 0;
}

static void put_sprite(
    s16 x,
    s16 y,
    u16 tile,
    u8 palette,
    u16 size,
    u8 priority
)
{
    put_sprite_with_attr1(x, y, tile, palette, size, priority, 0);
}

static void put_sprite_shaped(
    s16 x,
    s16 y,
    u16 tile,
    u8 palette,
    u16 shape,
    u16 size,
    s16 width,
    s16 height,
    u8 priority
)
{
    OBJATTR *object;
    if (oam_count >= SPRITE_LIMIT) return;
    if (
        x <= -width || x >= SCREEN_WIDTH ||
        y <= -height || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00FF) | ATTR0_NORMAL |
                    ATTR0_COLOR_16 | shape;
    object->attr1 = (x & 0x01FF) | size;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority) |
                    ATTR2_PALETTE(palette);
    object->dummy = 0;
}

static void render_enemy_shot(const EnemyShot *shot, u8 priority)
{
    u16 tile;
    u16 palette;
    u16 shape;
    u16 size;
    s16 offset_x;
    s16 offset_y;
    s16 width;
    s16 height;

    switch (shot->graphic) {
    case PC_SHOT_GRAPHIC_DART:
        tile = OBJ_TILE_PROJECTILE_058;
        palette = OBJ_PAL_PROJECTILE_058;
        offset_x = OBJ_PROJECTILE_OFFSET_X_058;
        offset_y = OBJ_PROJECTILE_OFFSET_Y_058;
        width = OBJ_PROJECTILE_WIDTH_058;
        height = OBJ_PROJECTILE_HEIGHT_058;
        break;
    case PC_SHOT_GRAPHIC_RED:
        tile = OBJ_TILE_PROJECTILE_112 + shot->animate;
        palette = OBJ_PAL_PROJECTILE_112;
        offset_x = OBJ_PROJECTILE_OFFSET_X_112;
        offset_y = OBJ_PROJECTILE_OFFSET_Y_112;
        width = OBJ_PROJECTILE_WIDTH_112;
        height = OBJ_PROJECTILE_HEIGHT_112;
        break;
    case PC_SHOT_GRAPHIC_LASER_LEFT:
        tile = OBJ_TILE_PROJECTILE_145;
        palette = OBJ_PAL_PROJECTILE_145;
        offset_x = OBJ_PROJECTILE_OFFSET_X_145;
        offset_y = OBJ_PROJECTILE_OFFSET_Y_145;
        width = OBJ_PROJECTILE_WIDTH_145;
        height = OBJ_PROJECTILE_HEIGHT_145;
        break;
    case PC_SHOT_GRAPHIC_LASER_DOWN:
        tile = OBJ_TILE_PROJECTILE_146;
        palette = OBJ_PAL_PROJECTILE_146;
        offset_x = OBJ_PROJECTILE_OFFSET_X_146;
        offset_y = OBJ_PROJECTILE_OFFSET_Y_146;
        width = OBJ_PROJECTILE_WIDTH_146;
        height = OBJ_PROJECTILE_HEIGHT_146;
        break;
    case PC_SHOT_GRAPHIC_LASER_RIGHT:
        tile = OBJ_TILE_PROJECTILE_147;
        palette = OBJ_PAL_PROJECTILE_147;
        offset_x = OBJ_PROJECTILE_OFFSET_X_147;
        offset_y = OBJ_PROJECTILE_OFFSET_Y_147;
        width = OBJ_PROJECTILE_WIDTH_147;
        height = OBJ_PROJECTILE_HEIGHT_147;
        break;
    case PC_SHOT_GRAPHIC_DART_LEFT:
        tile = OBJ_TILE_PROJECTILE_201;
        palette = OBJ_PAL_PROJECTILE_201;
        offset_x = OBJ_PROJECTILE_OFFSET_X_201;
        offset_y = OBJ_PROJECTILE_OFFSET_Y_201;
        width = OBJ_PROJECTILE_WIDTH_201;
        height = OBJ_PROJECTILE_HEIGHT_201;
        break;
    case PC_SHOT_GRAPHIC_DART_RIGHT:
        tile = OBJ_TILE_PROJECTILE_202;
        palette = OBJ_PAL_PROJECTILE_202;
        offset_x = OBJ_PROJECTILE_OFFSET_X_202;
        offset_y = OBJ_PROJECTILE_OFFSET_Y_202;
        width = OBJ_PROJECTILE_WIDTH_202;
        height = OBJ_PROJECTILE_HEIGHT_202;
        break;
    default:
        return;
    }
    shape = width == height ? ATTR0_SQUARE :
            (width > height ? ATTR0_WIDE : ATTR0_TALL);
    size = width == 16 && height == 16 ? ATTR1_SIZE_16 : ATTR1_SIZE_8;
    put_sprite_shaped(
        shot->x + offset_x,
        shot->y + offset_y,
        tile,
        palette,
        shape,
        size,
        width,
        height,
        priority
    );
}
