/*
 * Front-end runtime composition unit.
 *
 * These ordered fragments intentionally share main.c's translation unit:
 * most helpers are static and the split is organizational, not behavioural.
 */
#include "frontend/frontend_core.inc"
#include "frontend/frontend_source_art.inc"
#include "frontend/frontend_menus.inc"
#include "frontend/frontend_data_specs.inc"
#include "frontend/frontend_save.inc"
#include "frontend/frontend_navigation.inc"
#include "frontend/frontend_flow.inc"
