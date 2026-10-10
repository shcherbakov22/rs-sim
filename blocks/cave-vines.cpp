// CaveVinesBlock: the head keeps its berries when it turns into body

#include "blocks/handlers.h"

state_id cave_vines_update_body_after_converted_from_head(state_id head, state_id body) {
    return state_set(body, PROP_BERRIES, state_get(head, PROP_BERRIES));
}
