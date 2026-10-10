// LightBlock: only a player who may use game master blocks changes the level; the virtual player may not

#include "blocks/handlers.h"

interaction_result light_use_without_item() {
    return INTERACTION_RESULT_CONSUME;
}
