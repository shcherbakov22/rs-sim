// PrimedTnt: falls, slides and floats until its fuse runs out, then explodes. Its tick skips Entity.baseTick, and
// no portal is in scope.

#include "engine/explosion.h"
#include "engine/level.h"
#include "entities/handlers.h"

#include <cmath>

// the constructor draws its push angle from the level random
entity * primed_tnt_new(level & lv, vec3 pos) {
    entity * e   = entity_new(lv, ENTITY_TYPE_TNT, pos);
    double   rot = rng_next_double(lv.random) * (float) (3.141592653589793 * 2);
    e->delta     = { -std::sin(rot) * 0.02, 0.2F, -std::cos(rot) * 0.02 };
    e->fuse      = 80;
    return e;
}

// PrimedTnt.explode at a sixteenth of its height
static void primed_tnt_explode(level & lv, entity & e) {
    vec3 center = { e.pos.x, e.pos.y + entity_type_defs[e.type].height * 0.0625, e.pos.z };
    level_explode(lv, &e, center, 4.0F, EXPLOSION_BLOCK_INTERACTION_DESTROY);
}

void primed_tnt_tick(level & lv, entity & e) {
    entity_apply_gravity(e);
    entity_move(lv, e, MOVER_TYPE_SELF, e.delta);
    entity_apply_effects_from_blocks(lv, e);
    e.delta = vec3_scale(e.delta, 0.98);
    if (e.on_ground) e.delta = vec3_multiply(e.delta, { 0.7, -0.5, 0.7 });
    e.fuse--;
    if (e.fuse <= 0) {
        entity_discard(lv, e);
        primed_tnt_explode(lv, e);
    } else {
        entity_update_in_water_state_and_do_fluid_pushing(lv, e);
    }
}
