// CaveVinesPlantBlock: the body keeps its berries when it turns into a head

#include "blocks/handlers.h"

state_id cave_vines_plant_update_head_after_converted_from_body(state_id body, state_id head) {
    return state_set(head, PROP_BERRIES, state_get(body, PROP_BERRIES));
}
