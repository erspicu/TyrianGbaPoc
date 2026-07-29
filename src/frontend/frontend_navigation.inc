static const u16 frontend_planet_x[21] = {
    200, 150, 240, 300, 270, 280, 320, 260, 220, 150, 160,
    210, 80, 240, 220, 180, 310, 330, 150, 240, 200
};

static const u16 frontend_planet_y[21] = {
    40, 90, 90, 80, 170, 30, 50, 130, 120, 150, 220,
    200, 80, 50, 160, 10, 55, 55, 90, 90, 40
};

static const u16 frontend_planet_graphic[21] = {
    4, 1, 2, 3, 20, 36, 52, 68, 84, 100, 116,
    132, 151, 151, 151, 151, 52, 52, 1, 2, 4
};

static const u8 frontend_planet_animated[21] = {
    1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1
};

static u8 frontend_nav_obj_phase_component(s16 coordinate)
{
    s16 phase = (s16)(coordinate % FRONTEND_NAV_OBJ_SCALE_PHASES);

    if (phase < 0) phase += FRONTEND_NAV_OBJ_SCALE_PHASES;
    return (u8)phase;
}

static const FrontendNavObjMeta *frontend_nav_obj_metadata(
    u16 catalog,
    u8 phase
)
{
    const FrontendNavObjMeta *records =
        (const FrontendNavObjMeta *)(const void *)frontend_nav_obj_meta;

    if (
        catalog >= FRONTEND_NAV_OBJ_CATALOG_COUNT ||
        phase >= FRONTEND_NAV_OBJ_PHASE_COUNT
    ) {
        return 0;
    }
    return &records[
        (u32)catalog * FRONTEND_NAV_OBJ_PHASE_COUNT + phase
    ];
}

static u16 frontend_nav_obj_graphic(u8 planet_index)
{
    u16 graphic;

    if (
        planet_index >= 21 ||
        frontend_planet_graphic[planet_index] == 0
    ) {
        return UINT16_MAX;
    }
    graphic = (u16)(frontend_planet_graphic[planet_index] - 1u);
    if (
        frontend_planet_animated[planet_index] &&
        frontend_nav_planet_animation < 15
    ) {
        graphic = (u16)(
            graphic + frontend_nav_planet_animation
        );
    }
    return graphic;
}

static u8 frontend_nav_obj_source_position(
    u8 planet_index,
    u16 graphic,
    const FrontendNavObjMeta **metadata,
    u8 *phase,
    s16 *source_x,
    s16 *source_y
)
{
    const FrontendNavObjMeta *base;
    const FrontendNavObjMeta *resolved;
    u8 resolved_phase;
    s16 x;
    s16 y;

    if (
        planet_index >= 21 ||
        graphic >= FRONTEND_NAV_OBJ_PLANET_CATALOG_COUNT
    ) {
        return 0;
    }
    base = frontend_nav_obj_metadata(graphic, 0);
    if (
        base == 0 ||
        base->source_width == 0 ||
        base->source_height == 0
    ) {
        return 0;
    }
    x = (s16)(
        frontend_planet_x[planet_index] +
        66 -
        frontend_nav_presented_x -
        (s16)(base->source_width / 2u)
    );
    y = (s16)(
        frontend_planet_y[planet_index] +
        85 -
        frontend_nav_presented_y -
        (s16)(base->source_height / 2u)
    );
    resolved_phase = (u8)(
        frontend_nav_obj_phase_component(y) *
            FRONTEND_NAV_OBJ_SCALE_PHASES +
        frontend_nav_obj_phase_component(x)
    );
    resolved = frontend_nav_obj_metadata(graphic, resolved_phase);
    if (
        resolved == 0 ||
        resolved->tile_bytes == 0 ||
        resolved->source_width == 0 ||
        resolved->source_height == 0
    ) {
        return 0;
    }
    if (metadata != 0) *metadata = resolved;
    if (phase != 0) *phase = resolved_phase;
    if (source_x != 0) *source_x = x;
    if (source_y != 0) *source_y = y;
    return 1;
}

static u8 frontend_nav_obj_planet_visible(
    s16 source_x,
    s16 source_y,
    const FrontendNavObjMeta *metadata
)
{
    if (metadata == 0) return 0;
    /*
     * Preserve JE_drawNavLines' source-space rejection before the hardware
     * window performs the final ornate-frame clip.
     */
    return !(
        source_x <= -7 ||
        source_x + (s16)metadata->source_width >= 170 ||
        source_y <= 0 ||
        source_y >= 160
    );
}

static u8 frontend_nav_obj_collect_planets(
    u8 planets[FRONTEND_NAV_OBJ_SLOT_CAPACITY]
)
{
    u32 present = 0;
    u8 count = 0;
    u8 planet;
    u8 choice_count = frontend_next_choice_count();

    for (planet = 0; planet < 11; planet++) {
        planets[count++] = planet;
        present |= 1u << planet;
    }
    for (planet = 0; planet < choice_count; planet++) {
        u8 map_planet = frontend_map.map_planet[
            frontend_next_map_choice(planet)
        ];
        u8 index;

        if (map_planet <= 11 || map_planet > 21) continue;
        index = (u8)(map_planet - 1u);
        if (present & (1u << index)) continue;
        planets[count++] = index;
        present |= 1u << index;
    }
    if (
        frontend_map.map_origin > 11 &&
        frontend_map.map_origin <= 21
    ) {
        u8 index = (u8)(frontend_map.map_origin - 1u);

        if (!(present & (1u << index))) {
            planets[count++] = index;
        }
    }
    return count;
}

static u16 frontend_nav_obj_planet_reserve(u8 planet_index)
{
    u16 graphic = (u16)(
        frontend_planet_graphic[planet_index] - 1u
    );
    u8 frame_count =
        frontend_planet_animated[planet_index] ? 15 : 1;
    u16 reserve;
    u8 frame;

    if (frontend_nav_obj_reserve_cache[planet_index] != 0) {
        return frontend_nav_obj_reserve_cache[planet_index];
    }
    reserve = 0;
    for (frame = 0; frame < frame_count; frame++) {
        u8 phase;

        for (
            phase = 0;
            phase < FRONTEND_NAV_OBJ_PHASE_COUNT;
            phase++
        ) {
            const FrontendNavObjMeta *metadata =
                frontend_nav_obj_metadata(
                    (u16)(graphic + frame),
                    phase
                );

            if (
                metadata != 0 &&
                metadata->tile_bytes > reserve
            ) {
                reserve = metadata->tile_bytes;
            }
        }
    }
    frontend_nav_obj_reserve_cache[planet_index] = reserve;
    return reserve;
}

static void frontend_nav_obj_schedule_upload(
    const FrontendNavObjMeta *metadata,
    u16 vram_offset
)
{
    FrontendNavObjUpload *upload;

    if (
        metadata == 0 ||
        metadata->tile_bytes == 0
    ) {
        return;
    }
    if (
        frontend_nav_obj_upload_count >=
            FRONTEND_NAV_OBJ_UPLOAD_CAPACITY
    ) {
        telemetry_frontend_nav_obj_overflows++;
        return;
    }
    upload = &frontend_nav_obj_upload[
        frontend_nav_obj_upload_count++
    ];
    upload->source =
        frontend_nav_obj_tiles + metadata->tile_offset;
    upload->vram_offset = vram_offset;
    upload->bytes = metadata->tile_bytes;
}

static u8 frontend_nav_obj_axis_chunk(u8 remaining)
{
    if (remaining > 24) return 32;
    if (remaining > 8) return 16;
    return 8;
}

static void frontend_nav_obj_put(
    s16 x,
    s16 y,
    u16 tile,
    u8 width,
    u8 height
)
{
    OBJATTR *object;
    u16 shape;
    u16 size;

    if (
        oam_count >= HARDWARE_OAM_ENTRIES ||
        x <= -(s16)width || x >= SCREEN_WIDTH ||
        y <= -(s16)height || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    if (width == height) {
        shape = ATTR0_SQUARE;
        size =
            width == 32 ? ATTR1_SIZE_32 :
            (width == 16 ? ATTR1_SIZE_16 : ATTR1_SIZE_8);
    } else if (width > height) {
        shape = ATTR0_WIDE;
        size =
            width == 32 && height == 16 ? ATTR1_SIZE_32 :
            (width == 32 ? ATTR1_SIZE_16 : ATTR1_SIZE_8);
    } else {
        shape = ATTR0_TALL;
        size =
            width == 16 && height == 32 ? ATTR1_SIZE_32 :
            (height == 32 ? ATTR1_SIZE_16 : ATTR1_SIZE_8);
    }
    object = &oam_shadow[oam_count++];
    object->attr0 =
        (y & 0x00ff) |
        ATTR0_NORMAL |
        ATTR0_COLOR_256 |
        shape;
    object->attr1 = (x & 0x01ff) | size;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(0);
    object->dummy = 0;
}

static void frontend_nav_obj_draw_slot(
    const FrontendNavObjSlot *slot
)
{
    const FrontendNavObjMeta *metadata;
    s16 source_x;
    s16 source_y;
    s16 screen_x;
    s16 screen_y;
    u16 asset_offset = 0;
    u8 y = 0;

    if (
        slot == 0 ||
        !frontend_nav_obj_source_position(
            slot->planet_index,
            slot->graphic,
            &metadata,
            0,
            &source_x,
            &source_y
        ) ||
        !frontend_nav_obj_planet_visible(
            source_x,
            source_y,
            metadata
        )
    ) {
        return;
    }
    screen_x = frontend_source_to_screen_x(source_x);
    screen_y = frontend_source_to_screen_y(source_y);
    while (y < metadata->screen_height) {
        u8 chunk_height = frontend_nav_obj_axis_chunk(
            (u8)(metadata->screen_height - y)
        );
        u8 x = 0;

        while (x < metadata->screen_width) {
            u8 chunk_width = frontend_nav_obj_axis_chunk(
                (u8)(metadata->screen_width - x)
            );

            frontend_nav_obj_put(
                (s16)(screen_x + x),
                (s16)(screen_y + y),
                (u16)(
                    FRONTEND_NAV_OBJ_VRAM_TILE_BASE +
                    (slot->vram_offset + asset_offset) / 32u
                ),
                chunk_width,
                chunk_height
            );
            asset_offset = (u16)(
                asset_offset +
                (u16)chunk_width * chunk_height
            );
            x = (u8)(x + chunk_width);
        }
        y = (u8)(y + chunk_height);
    }
}

static void frontend_nav_obj_draw_dots(void)
{
    u8 route;
    u8 route_count = frontend_next_choice_count();
    u8 origin = frontend_map.map_origin;

    if (origin == 0 || origin > 21) return;
    for (route = 0; route < route_count; route++) {
        u8 map_choice = frontend_next_map_choice(route);
        u8 destination = frontend_map.map_planet[map_choice];
        u8 dots = frontend_nav_dot_count(destination);
        u8 dot;

        for (dot = 0; dot < dots; dot++) {
            s16 x = frontend_nav_part_way(
                (s16)frontend_planet_x[origin - 1u],
                (s16)frontend_planet_x[destination - 1u],
                dots,
                dot
            );
            s16 y = frontend_nav_part_way(
                (s16)frontend_planet_y[origin - 1u],
                (s16)frontend_planet_y[destination - 1u],
                dots,
                dot
            );
            u16 tile;

            x = (s16)(x - frontend_nav_presented_x + 64);
            y = (s16)(y - frontend_nav_presented_y + 83);
            if (x <= 0 || x >= 140 || y <= 0 || y >= 168) {
                continue;
            }
            tile = (u16)(
                FRONTEND_NAV_OBJ_VRAM_TILE_BASE +
                (
                    (
                        route == frontend_selection &&
                        dot < frontend_nav_dot_animation
                    ) ?
                        FRONTEND_NAV_OBJ_DOT_ON_OFFSET :
                        FRONTEND_NAV_OBJ_DOT_OFF_OFFSET
                ) /
                32u
            );
            frontend_nav_obj_put(
                frontend_source_to_screen_x(x),
                frontend_source_to_screen_y(y),
                tile,
                FRONTEND_NAV_OBJ_DOT_DIM,
                FRONTEND_NAV_OBJ_DOT_DIM
            );
        }
    }
}

static void frontend_nav_obj_render_oam(void)
{
    u8 old_oam_count = previous_oam_count;
    u8 slot = frontend_nav_obj_slot_count;

    if (!frontend_nav_obj_active) return;
    oam_count = 0;
    /*
     * Software drew later planets over earlier ones.  Equal-priority GBA
     * OBJ uses the lower OAM index in front, so emit the source list in
     * reverse and append route dots behind every planet.
     */
    while (slot != 0) {
        frontend_nav_obj_draw_slot(
            &frontend_nav_obj_slot[--slot]
        );
    }
    frontend_nav_obj_draw_dots();
    {
        u8 new_oam_count = oam_count;

        while (oam_count < old_oam_count) {
            oam_shadow[oam_count].attr0 = ATTR0_DISABLED;
            oam_shadow[oam_count].attr1 = 0;
            oam_shadow[oam_count].attr2 = 0;
            oam_shadow[oam_count].dummy = 0;
            oam_count++;
        }
        previous_oam_count = new_oam_count;
        oam_count = new_oam_count;
    }
    oam_dirty = 1;
    telemetry_frontend_nav_obj_updates++;
}

static void frontend_nav_obj_rebuild(void)
{
    u8 planets[FRONTEND_NAV_OBJ_SLOT_CAPACITY];
    u8 candidate_count = frontend_nav_obj_collect_planets(planets);
    u16 vram_offset = FRONTEND_NAV_OBJ_DOT_BYTES;
    u8 candidate;

    frontend_nav_obj_active = 1;
    frontend_nav_obj_palette_pending = 1;
    frontend_nav_obj_dot_tiles_pending = 1;
    frontend_nav_obj_upload_count = 0;
    frontend_nav_obj_slot_count = 0;
    for (candidate = 0; candidate < candidate_count; candidate++) {
        u8 planet_index = planets[candidate];
        u16 graphic = frontend_nav_obj_graphic(planet_index);
        const FrontendNavObjMeta *metadata;
        s16 source_x;
        s16 source_y;
        u8 phase;
        u16 reserve;
        FrontendNavObjSlot *slot;

        if (
            graphic == UINT16_MAX ||
            !frontend_nav_obj_source_position(
                planet_index,
                graphic,
                &metadata,
                &phase,
                &source_x,
                &source_y
            ) ||
            !frontend_nav_obj_planet_visible(
                source_x,
                source_y,
                metadata
            )
        ) {
            continue;
        }
        reserve = frontend_nav_obj_planet_reserve(planet_index);
        if (
            reserve == 0 ||
            vram_offset + reserve > FRONTEND_NAV_OBJ_VRAM_BYTES ||
            frontend_nav_obj_slot_count >=
                FRONTEND_NAV_OBJ_SLOT_CAPACITY
        ) {
            telemetry_frontend_nav_obj_overflows++;
            continue;
        }
        slot = &frontend_nav_obj_slot[
            frontend_nav_obj_slot_count++
        ];
        slot->planet_index = planet_index;
        slot->graphic = (u8)graphic;
        slot->phase = phase;
        slot->animated = frontend_planet_animated[planet_index];
        slot->vram_offset = vram_offset;
        slot->reserve_bytes = reserve;
        frontend_nav_obj_schedule_upload(metadata, vram_offset);
        vram_offset = (u16)(vram_offset + reserve);
    }
    frontend_nav_obj_render_oam();
}

static u8 frontend_nav_obj_has_visible_layout_change(void)
{
    u8 planets[FRONTEND_NAV_OBJ_SLOT_CAPACITY];
    u8 candidate_count = frontend_nav_obj_collect_planets(planets);
    u8 visible_count = 0;
    u8 candidate;

    for (candidate = 0; candidate < candidate_count; candidate++) {
        u8 planet_index = planets[candidate];
        u16 graphic = frontend_nav_obj_graphic(planet_index);
        const FrontendNavObjMeta *metadata;
        s16 source_x;
        s16 source_y;

        if (
            graphic == UINT16_MAX ||
            !frontend_nav_obj_source_position(
                planet_index,
                graphic,
                &metadata,
                0,
                &source_x,
                &source_y
            ) ||
            !frontend_nav_obj_planet_visible(
                source_x,
                source_y,
                metadata
            )
        ) {
            continue;
        }
        if (
            visible_count >= frontend_nav_obj_slot_count ||
            frontend_nav_obj_slot[visible_count].planet_index !=
                planet_index
        ) {
            return 1;
        }
        visible_count++;
    }
    return visible_count != frontend_nav_obj_slot_count;
}

static void frontend_nav_obj_animate(void)
{
    u8 slot_index;

    if (
        !frontend_nav_obj_active ||
        frontend_nav_obj_has_visible_layout_change()
    ) {
        frontend_nav_obj_rebuild();
        return;
    }
    frontend_nav_obj_upload_count = 0;
    for (
        slot_index = 0;
        slot_index < frontend_nav_obj_slot_count;
        slot_index++
    ) {
        FrontendNavObjSlot *slot =
            &frontend_nav_obj_slot[slot_index];
        u16 graphic;
        const FrontendNavObjMeta *metadata;
        u8 phase;

        if (!slot->animated) continue;
        graphic = frontend_nav_obj_graphic(slot->planet_index);
        if (
            graphic == UINT16_MAX ||
            !frontend_nav_obj_source_position(
                slot->planet_index,
                graphic,
                &metadata,
                &phase,
                0,
                0
            ) ||
            metadata->tile_bytes > slot->reserve_bytes
        ) {
            frontend_nav_obj_rebuild();
            return;
        }
        slot->graphic = (u8)graphic;
        slot->phase = phase;
        frontend_nav_obj_schedule_upload(
            metadata,
            slot->vram_offset
        );
    }
    frontend_nav_obj_render_oam();
}

static void frontend_nav_obj_commit_vblank(void)
{
    u8 upload;

    if (!frontend_nav_obj_active) {
        frontend_nav_obj_upload_count = 0;
        frontend_nav_obj_palette_pending = 0;
        frontend_nav_obj_dot_tiles_pending = 0;
        return;
    }
    if (frontend_nav_obj_palette_pending) {
        dmaCopy(
            frontend_nav_obj_palette,
            SPRITE_PALETTE,
            FRONTEND_NAV_OBJ_PALETTE_BYTES
        );
        frontend_nav_obj_palette_pending = 0;
    }
    if (frontend_nav_obj_dot_tiles_pending) {
        const FrontendNavObjMeta *off =
            frontend_nav_obj_metadata(
                FRONTEND_NAV_OBJ_DOT_OFF_CATALOG,
                0
            );
        const FrontendNavObjMeta *on =
            frontend_nav_obj_metadata(
                FRONTEND_NAV_OBJ_DOT_ON_CATALOG,
                0
            );

        if (off != 0 && on != 0) {
            dmaCopy(
                frontend_nav_obj_tiles + off->tile_offset,
                FRONTEND_NAV_OBJ_VRAM +
                    FRONTEND_NAV_OBJ_DOT_OFF_OFFSET,
                off->tile_bytes
            );
            dmaCopy(
                frontend_nav_obj_tiles + on->tile_offset,
                FRONTEND_NAV_OBJ_VRAM +
                    FRONTEND_NAV_OBJ_DOT_ON_OFFSET,
                on->tile_bytes
            );
            telemetry_frontend_nav_obj_uploads += 2;
            telemetry_frontend_nav_obj_upload_bytes +=
                off->tile_bytes + on->tile_bytes;
        }
        frontend_nav_obj_dot_tiles_pending = 0;
    }
    for (
        upload = 0;
        upload < frontend_nav_obj_upload_count;
        upload++
    ) {
        const FrontendNavObjUpload *pending =
            &frontend_nav_obj_upload[upload];

        dmaCopy(
            pending->source,
            FRONTEND_NAV_OBJ_VRAM + pending->vram_offset,
            pending->bytes
        );
        telemetry_frontend_nav_obj_uploads++;
        telemetry_frontend_nav_obj_upload_bytes += pending->bytes;
    }
    frontend_nav_obj_upload_count = 0;
}

static void frontend_nav_obj_disable(void)
{
    if (
        frontend_nav_obj_active ||
        previous_oam_count != 0
    ) {
        hide_all_sprites();
    }
    frontend_nav_obj_active = 0;
    frontend_nav_obj_slot_count = 0;
    frontend_nav_obj_upload_count = 0;
    frontend_nav_obj_palette_pending = 0;
    frontend_nav_obj_dot_tiles_pending = 0;
}

static u8 frontend_nav_planet_dimensions(
    u8 planet,
    u16 *width,
    u16 *height
)
{
    const FrontendNavObjMeta *metadata;
    u16 graphic;

    if (
        planet == 0 ||
        planet > 21
    ) {
        return 0;
    }
    graphic = (u16)(frontend_planet_graphic[planet - 1u] - 1u);
    metadata = frontend_nav_obj_metadata(graphic, 0);
    if (
        metadata == 0 ||
        metadata->source_width == 0 ||
        metadata->source_height == 0
    ) {
        return 0;
    }
    if (width != 0) *width = metadata->source_width;
    if (height != 0) *height = metadata->source_height;
    return 1;
}

static void frontend_nav_set_target(void)
{
    u8 choice_count = frontend_next_choice_count();
    u8 origin = frontend_map.map_origin;
    u8 destination;
    u16 origin_width;
    u16 origin_height;
    u16 destination_width;
    u16 destination_height;

    if (
        frontend_selection >= choice_count ||
        origin == 0 ||
        origin > 21
    ) {
        return;
    }
    destination = frontend_map.map_planet[
        frontend_next_map_choice(frontend_selection)
    ];
    if (
        destination == 0 ||
        destination > 21 ||
        !frontend_nav_planet_dimensions(
            origin,
            &origin_width,
            &origin_height
        ) ||
        !frontend_nav_planet_dimensions(
            destination,
            &destination_width,
            &destination_height
        )
    ) {
        return;
    }
    frontend_nav_target_x = (s16)(
        (
            (s16)frontend_planet_x[origin - 1u] -
                (s16)(origin_width / 2u) +
            (s16)frontend_planet_x[destination - 1u] -
                (s16)(destination_width / 2u)
        ) /
        2
    );
    frontend_nav_target_y = (s16)(
        (
            (s16)frontend_planet_y[origin - 1u] -
                (s16)(origin_height / 2u) +
            (s16)frontend_planet_y[destination - 1u] -
                (s16)(destination_height / 2u)
        ) /
        2
    );
}

static void frontend_nav_reset(void)
{
    u8 origin = frontend_map.map_origin;

    if (origin == 0 || origin > 21) origin = 1;
    frontend_nav_x = (s16)frontend_planet_x[origin - 1u];
    frontend_nav_y = (s16)frontend_planet_y[origin - 1u];
    frontend_nav_presented_x = frontend_nav_x;
    frontend_nav_presented_y = frontend_nav_y;
    frontend_nav_target_x = frontend_nav_x;
    frontend_nav_target_y = frontend_nav_y;
    frontend_nav_planet_animation = 0;
    frontend_nav_animation_wait = 0;
    frontend_nav_dot_animation = 0;
    frontend_nav_dot_wait = 0;
    frontend_nav_set_target();
}

static u8 frontend_nav_dot_count(u8 destination)
{
    s32 dx;
    s32 dy;
    u32 squared;
    u8 root;
    u8 best = 0;
    u32 best_distance = UINT32_MAX;
    u8 origin = frontend_map.map_origin;

    if (
        origin == 0 ||
        origin > 21 ||
        destination == 0 ||
        destination > 21
    ) {
        return 0;
    }
    dx =
        (s32)frontend_planet_x[destination - 1u] -
        frontend_planet_x[origin - 1u];
    dy =
        (s32)frontend_planet_y[destination - 1u] -
        frontend_planet_y[origin - 1u];
    squared = (u32)(dx * dx + dy * dy);
    if (squared == 0) return 0;
    for (root = 1; root <= 20; root++) {
        u32 square = (u32)root * root;
        u32 fourth = square * square;
        u32 distance =
            fourth > squared ?
                fourth - squared :
                squared - fourth;

        if (distance < best_distance) {
            best_distance = distance;
            best = root;
        }
    }
    if (best == 0) return 0;
    best--;
    return best > 10 ? 10 : best;
}

static s16 frontend_nav_part_way(
    s16 start,
    s16 finish,
    u8 dots,
    u8 distance
)
{
    return (s16)(
        (finish - start) / (dots + 2) * (distance + 1) + start
    );
}

static u8 frontend_nav_grid_phase(s16 coordinate)
{
    s16 phase = (s16)(coordinate % FRONTEND_NAV_GRID_PHASES);

    if (phase < 0) phase += FRONTEND_NAV_GRID_PHASES;
    return (u8)phase;
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) void
frontend_nav_copy_bitmap_page(u8 *destination, const u8 *source)
{
    u8 row;

    /*
     * The generated ROM page pads each 126-pixel row to 128 bytes.  Both
     * pointers are therefore word aligned for every iteration, avoiding 138
     * generic memcpy calls and keeping sequential Game Pak prefetch active.
     */
    for (row = 0; row < FRONTEND_NAV_BITMAP_HEIGHT; row++) {
        const u32 *source_words =
            (const u32 *)(const void *)source;
        u32 *destination_words = (u32 *)(void *)destination;
        u8 word;

        for (word = 0; word < 31; word++) {
            destination_words[word] = source_words[word];
        }
        *(u16 *)(void *)(destination + 124) =
            *(const u16 *)(const void *)(source + 124);
        source += FRONTEND_NAV_BITMAP_STRIDE;
        destination += SCREEN_WIDTH;
    }
}

static void frontend_nav_draw(void)
{
    u8 phase_x = frontend_nav_grid_phase(
        (s16)(frontend_nav_presented_x >> 1)
    );
    u8 phase_y = frontend_nav_grid_phase(
        (s16)(frontend_nav_presented_y >> 1)
    );
    const u8 *source =
        frontend_nav_bitmap_pages +
        (
            (u32)phase_y * FRONTEND_NAV_GRID_PHASES +
            phase_x
        ) *
        FRONTEND_NAV_BITMAP_PAGE_BYTES;
    frontend_nav_copy_bitmap_page(frontend_frame_scratch, source);
    telemetry_frontend_nav_bitmap_redraws++;
}

static void frontend_render_next_level_item(u8 selection)
{
    u8 choice_count = frontend_next_choice_count();
    const char *label;
    u16 y;

    if (selection < choice_count) {
        y = (u16)(
            FRONTEND_NEXT_CHOICE_Y +
            selection * FRONTEND_NEXT_CHOICE_STEP
        );
        label = frontend_next_choice_label(selection);
    } else {
        y = FRONTEND_NEXT_EXIT_Y;
        label = (
            frontend_text_ready &&
            frontend_text.misc_text[5][0] != '\0'
        ) ?
            frontend_text.misc_text[5] :
            "Exit to Game Menu";
    }
    frontend_fill_rect(
        frontend_frame_scratch,
        FRONTEND_NEXT_PANEL_X,
        y,
        FRONTEND_NEXT_PANEL_WIDTH,
        FRONTEND_NEXT_ITEM_HEIGHT,
        FRONTEND_NEXT_BACKGROUND_COLOUR
    );
    frontend_draw_menu_text_clipped(
        frontend_frame_scratch,
        label,
        FRONTEND_NEXT_PANEL_X,
        (s16)y,
        FRONTEND_NEXT_PANEL_X + FRONTEND_NEXT_PANEL_WIDTH,
        selection == frontend_selection ?
            FRONTEND_NEXT_SELECTED_COLOUR :
            FRONTEND_NEXT_TEXT_COLOUR
    );
}

static void frontend_prepare_next_level_base(void)
{
    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(FRONTEND_FRAME_MENU_CHROME),
        FRONTEND_FRAME_BYTES
    );
    frontend_nav_set_target();
    frontend_nav_presented_x = frontend_nav_x;
    frontend_nav_presented_y = frontend_nav_y;
}

static void frontend_prepare_next_level_text_base(void)
{
    frontend_draw_menu_text_centered(
        frontend_frame_scratch,
        "Next Level",
        TYRIAN_GBA_LAYOUT_NEXT_TITLE_CENTER_X,
        TYRIAN_GBA_LAYOUT_NEXT_TITLE_Y,
        FRONTEND_NEXT_TITLE_COLOUR
    );
    frontend_fill_rect(
        frontend_frame_scratch,
        FRONTEND_NEXT_PANEL_X,
        FRONTEND_NEXT_PANEL_Y,
        FRONTEND_NEXT_PANEL_WIDTH,
        FRONTEND_NEXT_PANEL_HEIGHT,
        FRONTEND_NEXT_BACKGROUND_COLOUR
    );
}

static void frontend_prepare_next_level_text(void)
{
    u8 choice_count = frontend_next_choice_count();
    u8 choice;

    frontend_prepare_next_level_text_base();
    for (choice = 0; choice < choice_count; choice++) {
        frontend_render_next_level_item(choice);
    }
    frontend_render_next_level_item(choice_count);
}

static void frontend_render_next_level(void)
{
    frontend_prepare_next_level_base();
    frontend_nav_draw();
    frontend_prepare_next_level_text();
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_NEXT_LEVEL_0)
    );
    frontend_nav_obj_rebuild();
}

static void frontend_render_next_level_selection(u8 old_selection)
{
    u8 choice_count = frontend_next_choice_count();

    frontend_nav_dot_animation = 0;
    frontend_nav_dot_wait = 0;
    frontend_nav_set_target();
    frontend_render_next_level_item(old_selection);
    frontend_render_next_level_item(frontend_selection);
    frontend_dirty_begin();
    {
        u16 old_y =
            old_selection < choice_count ?
                (u16)(
                    FRONTEND_NEXT_CHOICE_Y +
                    old_selection * FRONTEND_NEXT_CHOICE_STEP
                ) :
                FRONTEND_NEXT_EXIT_Y;
        u16 new_y =
            frontend_selection < choice_count ?
                (u16)(
                    FRONTEND_NEXT_CHOICE_Y +
                    frontend_selection *
                        FRONTEND_NEXT_CHOICE_STEP
                ) :
                FRONTEND_NEXT_EXIT_Y;

        frontend_dirty_add(
            FRONTEND_NEXT_PANEL_X,
            (s16)old_y,
            FRONTEND_NEXT_PANEL_WIDTH,
            FRONTEND_NEXT_ITEM_HEIGHT
        );
        frontend_dirty_add(
            FRONTEND_NEXT_PANEL_X,
            (s16)new_y,
            FRONTEND_NEXT_PANEL_WIDTH,
            FRONTEND_NEXT_ITEM_HEIGHT
        );
    }
    frontend_dirty_present();
    /*
     * Selecting "Exit" does not move the navigation camera.  The old path
     * nevertheless decoded and redrew the complete 126x138 map on every
     * cursor press.  Route dots are OBJ, so selection changes only need OAM
     * plus the two affected native-text rows; genuine destination changes
     * are picked up by the camera update below.
     */
    frontend_nav_obj_render_oam();
}

static void frontend_render_next_level_nav(void)
{
    frontend_nav_presented_x = frontend_nav_x;
    frontend_nav_presented_y = frontend_nav_y;
    frontend_nav_draw();
    frontend_dirty_begin();
    frontend_dirty_add(
        FRONTEND_NAV_DIRTY_X,
        FRONTEND_NAV_DIRTY_Y,
        FRONTEND_NAV_DIRTY_WIDTH,
        FRONTEND_NAV_DIRTY_HEIGHT
    );
    frontend_dirty_present();
    frontend_nav_obj_rebuild();
}

static u8 frontend_stats_cube_total(void)
{
    u32 total = source_parity_level.data_cube_pickup_count;

    return total > FRONTEND_STATS_MAX_CUBES ?
        FRONTEND_STATS_MAX_CUBES :
        (u8)total;
}

static u8 *frontend_stats_tile_scratch(void)
{
    return frontend_frame_scratch;
}
