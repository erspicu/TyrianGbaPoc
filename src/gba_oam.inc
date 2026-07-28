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

#if TYRIAN_GBA_STRESS_LOADOUT && \
    !TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER
static void put_sprite_8bpp_64_blended(
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
        y <= -64 || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL |
                    ATTR0_TYPE_BLENDED |
                    ATTR0_COLOR_256 | ATTR0_SQUARE;
    object->attr1 = (x & 0x01ff) | ATTR1_SIZE_64;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority);
    object->dummy = 0;
}
#endif

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
