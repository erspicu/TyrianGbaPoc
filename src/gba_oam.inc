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

static void put_sprite_blended_with_attr1(
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
    if (
        x <= -dimension || x >= SCREEN_WIDTH ||
        y <= -dimension || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL |
                    ATTR0_TYPE_BLENDED |
                    ATTR0_COLOR_16 | ATTR0_SQUARE;
    object->attr1 = (x & 0x01ff) | size | attr1_flags;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority) |
                    ATTR2_PALETTE(palette);
    object->dummy = 0;
}

static void put_sprite_blended(
    s16 x,
    s16 y,
    u16 tile,
    u8 palette,
    u16 size,
    u8 priority
)
{
    put_sprite_blended_with_attr1(
        x, y, tile, palette, size, priority, 0
    );
}

static void put_sprite_windowed_with_attr1(
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
    if (
        x <= -dimension || x >= SCREEN_WIDTH ||
        y <= -dimension || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL |
                    ATTR0_TYPE_WINDOWED |
                    ATTR0_COLOR_16 | ATTR0_SQUARE;
    object->attr1 = (x & 0x01ff) | size | attr1_flags;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority) |
                    ATTR2_PALETTE(palette);
    object->dummy = 0;
    source_detail_shadow_windows_active = 1;
}

static void put_sprite_8bpp_32(
    s16 x,
    s16 y,
    u16 tile,
    u8 priority
)
{
    OBJATTR *object;

    if (oam_count >= SPRITE_LIMIT) return;
    if (
        x <= -32 || x >= SCREEN_WIDTH ||
        y <= -32 || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL |
                    ATTR0_COLOR_256 | ATTR0_SQUARE;
    object->attr1 = (x & 0x01ff) | ATTR1_SIZE_32;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority);
    object->dummy = 0;
}

static void put_sprite_8bpp_32_with_type(
    s16 x,
    s16 y,
    u16 tile,
    u8 priority,
    u16 type
)
{
    OBJATTR *object;

    if (oam_count >= SPRITE_LIMIT) return;
    if (
        x <= -32 || x >= SCREEN_WIDTH ||
        y <= -32 || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL | type |
                    ATTR0_COLOR_256 | ATTR0_SQUARE;
    object->attr1 = (x & 0x01ff) | ATTR1_SIZE_32;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority);
    object->dummy = 0;
    if (type == ATTR0_TYPE_WINDOWED) {
        source_detail_shadow_windows_active = 1;
    }
}

static void put_sprite_8bpp_32_blended(
    s16 x,
    s16 y,
    u16 tile,
    u8 priority
)
{
    put_sprite_8bpp_32_with_type(
        x, y, tile, priority, ATTR0_TYPE_BLENDED
    );
}

static void put_sprite_8bpp_32_windowed(
    s16 x,
    s16 y,
    u16 tile,
    u8 priority
)
{
    put_sprite_8bpp_32_with_type(
        x, y, tile, priority, ATTR0_TYPE_WINDOWED
    );
}

static void put_sprite_8bpp_16(
    s16 x,
    s16 y,
    u16 tile,
    u8 priority
)
{
    OBJATTR *object;

    if (oam_count >= SPRITE_LIMIT) return;
    if (
        x <= -16 || x >= SCREEN_WIDTH ||
        y <= -16 || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL |
                    ATTR0_COLOR_256 | ATTR0_SQUARE;
    object->attr1 = (x & 0x01ff) | ATTR1_SIZE_16;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority);
    object->dummy = 0;
}

static void put_sprite_8bpp_16_with_type(
    s16 x,
    s16 y,
    u16 tile,
    u8 priority,
    u16 type
)
{
    OBJATTR *object;

    if (oam_count >= SPRITE_LIMIT) return;
    if (
        x <= -16 || x >= SCREEN_WIDTH ||
        y <= -16 || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL | type |
                    ATTR0_COLOR_256 | ATTR0_SQUARE;
    object->attr1 = (x & 0x01ff) | ATTR1_SIZE_16;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority);
    object->dummy = 0;
    if (type == ATTR0_TYPE_WINDOWED) {
        source_detail_shadow_windows_active = 1;
    }
}

#if TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_LOW
static void put_sprite_8bpp_16_blended(
    s16 x,
    s16 y,
    u16 tile,
    u8 priority
)
{
    put_sprite_8bpp_16_with_type(
        x, y, tile, priority, ATTR0_TYPE_BLENDED
    );
}
#endif

static void put_sprite_8bpp_16_windowed(
    s16 x,
    s16 y,
    u16 tile,
    u8 priority
)
{
    put_sprite_8bpp_16_with_type(
        x, y, tile, priority, ATTR0_TYPE_WINDOWED
    );
}

static void put_sprite_8bpp_shaped_blended(
    s16 x,
    s16 y,
    u16 tile,
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
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL |
                    ATTR0_TYPE_BLENDED |
                    ATTR0_COLOR_256 | shape;
    object->attr1 = (x & 0x01ff) | size;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority);
    object->dummy = 0;
}

#if !TYRIAN_GBA_STRESS_LOADOUT
static void put_sprite_8bpp_32x16(
    s16 x,
    s16 y,
    u16 tile,
    u8 priority
)
{
    OBJATTR *object;

    if (oam_count >= SPRITE_LIMIT) return;
    if (
        x <= -32 || x >= SCREEN_WIDTH ||
        y <= -16 || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL |
                    ATTR0_COLOR_256 | ATTR0_WIDE;
    object->attr1 = (x & 0x01ff) | ATTR1_SIZE_32;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority);
    object->dummy = 0;
}

#if TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_LOW
static void put_sprite_8bpp_32x16_blended(
    s16 x,
    s16 y,
    u16 tile,
    u8 priority
)
{
    OBJATTR *object;

    if (oam_count >= SPRITE_LIMIT) return;
    if (
        x <= -32 || x >= SCREEN_WIDTH ||
        y <= -16 || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL |
                    ATTR0_TYPE_BLENDED |
                    ATTR0_COLOR_256 | ATTR0_WIDE;
    object->attr1 = (x & 0x01ff) | ATTR1_SIZE_32;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority);
    object->dummy = 0;
}
#endif
#endif

static void put_sprite_8bpp_64_blended(
    s16 x,
    s16 y,
    u16 tile,
    u8 priority
)
{
    put_sprite_8bpp_shaped_blended(
        x, y, tile,
        ATTR0_SQUARE, ATTR1_SIZE_64,
        64, 64, priority
    );
}

#if !TYRIAN_GBA_STRESS_LOADOUT
static void put_sprite_4bpp_64x32_blended(
    s16 x,
    s16 y,
    u16 tile,
    u8 priority
)
{
    OBJATTR *object;

    if (oam_count >= SPRITE_LIMIT) return;
    if (
        x <= -64 || x >= SCREEN_WIDTH ||
        y <= -32 || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL |
                    ATTR0_TYPE_BLENDED |
                    ATTR0_COLOR_16 | ATTR0_WIDE;
    object->attr1 = (x & 0x01ff) | ATTR1_SIZE_64;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority) |
                    ATTR2_PALETTE(
                        SOURCE_OPTION_PROJECTILE_PALETTE_BANK
                    );
    object->dummy = 0;
}
#endif

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
