#ifndef DENSE_NAV_COLLISION_BRIDGE_H
#define DENSE_NAV_COLLISION_BRIDGE_H

/*
 * Optional bridge from libdense_collision's authoritative static
 * world to libdense_nav walkability. The only libdense_nav header
 * that includes dense_collision.h; build with
 *   make DENSE_COLLISION_DIR=/path/to/libdense_collision
 */

#include "dense_nav.h"

#include <dense_collision.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Rasterize the static solids intersecting each tile (tile rectangle
 * inflated by agent_radius; Chebyshev inflation, conservative on
 * diagonals) into DNAV_BLOCKED / DNAV_COST_DEFAULT. Overwrites the
 * costs of every tile in the rectangle - paint terrain costs after
 * rasterizing. Games with several agent radii keep one grid per
 * radius class.
 */
DNAV_API dnav_result dnav_rasterize_collision_rect(
    dnav_grid *grid,
    dc_world *world,
    dc_layer_mask layers,
    dc_coord agent_radius,
    dnav_tile rect_min,
    dnav_tile rect_max
);
DNAV_API dnav_result dnav_rasterize_collision(
    dnav_grid *grid,
    dc_world *world,
    dc_layer_mask layers,
    dc_coord agent_radius
);

#ifdef __cplusplus
}
#endif

#endif
