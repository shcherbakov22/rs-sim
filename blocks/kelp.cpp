// KelpBlock; KelpPlantBlock asks its head

#include "blocks/handlers.h"

bool kelp_can_attach_to(state_id attached) {
    return !state_is(attached, BLOCK_MAGMA_BLOCK);
}
