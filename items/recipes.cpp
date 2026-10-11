// Crafting recipes: RecipeManager finds the recipe for a grid and crafts it; one section per recipe class.
// Stacks in the sim carry only their item's default data components.

#include "items/recipes.h"

#include "engine/level.h"

// CraftingInput.ofPositioned
crafting_input crafting_input_of(int32_t width, int32_t height, const item_stack * items) {
    int32_t left   = width - 1;
    int32_t right  = 0;
    int32_t top    = height - 1;
    int32_t bottom = 0;
    for (int32_t y = 0; y < height; y++) {
        bool row_empty = true;
        for (int32_t x = 0; x < width; x++) {
            if (!item_stack_is_empty(items[x + y * width])) {
                left      = std::min(left, x);
                right     = std::max(right, x);
                row_empty = false;
            }
        }
        if (!row_empty) {
            top    = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }
    crafting_input in = { 0, 0, {}, 0 };
    if (right - left + 1 <= 0 || bottom - top + 1 <= 0) return in;
    in.width  = right - left + 1;
    in.height = bottom - top + 1;
    for (int32_t y = 0; y < in.height; y++) {
        for (int32_t x = 0; x < in.width; x++) {
            const item_stack & s = items[x + left + (y + top) * width];
            in.items.push_back(s);
            in.ingredient_count += item_stack_is_empty(s) ? 0 : 1;
        }
    }
    return in;
}

static item_stack crafting_input_get_item(const crafting_input & in, int32_t x, int32_t y) {
    return in.items[x + y * in.width];
}

// Ingredient.test: the empty stack is air, which no ingredient holds
static bool ingredient_test(int32_t ingredient, const item_stack & s) {
    int32_t                item = item_stack_get_item(s);
    const ingredient_def & def  = ingredient_defs[ingredient];
    for (int32_t i = 0; i < def.count; i++) {
        if (ingredient_items[def.first + i] == item) return true;
    }
    return false;
}

static bool recipe_field_test(recipe_field field, const item_stack & s) {
    return ingredient_test(recipe_field_ingredients[field], s);
}

// ShapedRecipe

// ShapedRecipePattern.matches(CraftingInput, boolean)
static bool shaped_recipe_matches_flip(const recipe_def & def, const crafting_input & in, bool x_flip) {
    for (int32_t y = 0; y < def.height; y++) {
        for (int32_t x = 0; x < def.width; x++) {
            int32_t    column   = x_flip ? def.width - x - 1 : x;
            int32_t    expected = recipe_ingredients[def.first + column + y * def.width];
            item_stack actual   = crafting_input_get_item(in, x, y);
            // Ingredient.testOptionalIngredient
            bool       ok       = expected < 0 ? item_stack_is_empty(actual) : ingredient_test(expected, actual);
            if (!ok) return false;
        }
    }
    return true;
}

// ShapedRecipePattern.matches; a symmetrical pattern only skips a mirrored test that gives the same answer
static bool shaped_recipe_matches(int32_t recipe, const crafting_input & in) {
    const recipe_def & def              = recipe_defs[recipe];
    int32_t            ingredient_count = 0;
    for (int32_t i = 0; i < def.n_ingredients; i++) ingredient_count += recipe_ingredients[def.first + i] < 0 ? 0 : 1;
    if (in.ingredient_count != ingredient_count || in.width != def.width || in.height != def.height) return false;
    return shaped_recipe_matches_flip(def, in, true) || shaped_recipe_matches_flip(def, in, false);
}

// ShapelessRecipe

// one augmenting path of StackedContents.tryPick, which finds a full matching whenever one exists
static bool shapeless_recipe_try_assign(const recipe_def & def, const crafting_input & in, int32_t ingredient,
                                        std::vector<bool> & visited, std::vector<int32_t> & slot_owner) {
    for (size_t slot = 0; slot < in.items.size(); slot++) {
        if (visited[slot] || !ingredient_test(recipe_ingredients[def.first + ingredient], in.items[slot])) continue;
        visited[slot] = true;
        if (slot_owner[slot] < 0 || shapeless_recipe_try_assign(def, in, slot_owner[slot], visited, slot_owner)) {
            slot_owner[slot] = ingredient;
            return true;
        }
    }
    return false;
}

// StackedItemContents.canCraft, each non-empty slot counting once
static bool shapeless_recipe_can_craft(const recipe_def & def, const crafting_input & in) {
    std::vector<int32_t> slot_owner(in.items.size(), -1);
    for (int32_t ingredient = 0; ingredient < def.n_ingredients; ingredient++) {
        std::vector<bool> visited(in.items.size(), false);
        if (!shapeless_recipe_try_assign(def, in, ingredient, visited, slot_owner)) return false;
    }
    return true;
}

static bool shapeless_recipe_matches(int32_t recipe, const crafting_input & in) {
    const recipe_def & def = recipe_defs[recipe];
    if (in.ingredient_count != def.n_ingredients) return false;
    if (in.items.size() == 1 && def.n_ingredients == 1) {
        return ingredient_test(recipe_ingredients[def.first], in.items[0]);
    }
    return shapeless_recipe_can_craft(def, in);
}

// TransmuteRecipe; its ingredients are the input, then the material

// TransmuteResult.isResultUnchanged: the input's data components carry over, so with no added components only the item and count differ
static bool transmute_recipe_is_result_unchanged(level & lv, const recipe_def & def, const item_stack & input) {
    if (def.result_components) level_unsupported(lv, "recipe %s: result data components", def.name);
    return def.count == 1 && def.result == item_stack_get_item(input);
}

static bool transmute_recipe_matches(level & lv, int32_t recipe, const crafting_input & in) {
    const recipe_def & def = recipe_defs[recipe];
    if (in.ingredient_count != 2) return false;
    bool found_input    = false;
    bool found_material = false;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) continue;
        if (!found_input && ingredient_test(recipe_ingredients[def.first], s)) {
            if (transmute_recipe_is_result_unchanged(lv, def, s)) return false;
            found_input = true;
        } else {
            if (found_material || !ingredient_test(recipe_ingredients[def.first + 1], s)) return false;
            found_material = true;
        }
    }
    return found_input && found_material;
}

// TransmuteResult.apply: transmuteCopy keeps the input's DAMAGE
static item_stack transmute_recipe_assemble(int32_t recipe, const crafting_input & in) {
    const recipe_def & def = recipe_defs[recipe];
    for (const item_stack & s : in.items) {
        if (!item_stack_is_empty(s) && ingredient_test(recipe_ingredients[def.first], s)) {
            return { def.result, def.count, s.damage };
        }
    }
    return {};
}

// MapExtendingRecipe

// a fresh filled map has no MAP_ID, so MapItem.getSavedData finds no data
static bool map_extending_recipe_matches(level & lv, int32_t recipe, const crafting_input & in) {
    if (!shaped_recipe_matches(recipe, in)) return false;
    for (const item_stack & s : in.items) {
        if (!item_stack_is_empty(s) && (item_defs[s.item].components & ITEM_COMPONENT_MAP_ID)) {
            level_unsupported(lv, "MapItem.getSavedData");
            return false;
        }
    }
    return false;
}

// the special recipes (CustomRecipe)

// the special recipes that take exactly one stack that is_one and at least one other, each of which is_some
static bool custom_recipe_matches_one_and_some(const crafting_input & in, bool (*is_one)(const item_stack & s),
                                               bool (*is_some)(const item_stack & s)) {
    if (in.ingredient_count < 2) return false;
    bool has_one  = false;
    bool has_some = false;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) continue;
        if (is_one(s)) {
            if (has_one) return false;
            has_one = true;
        } else if (is_some(s)) {
            has_some = true;
        } else {
            return false;
        }
    }
    return has_one && has_some;
}

static bool custom_recipe_is_dye(const item_stack & s) { return item_instanceof(s.item, ICLASS_DYE_ITEM); }

// ArmorDyeRecipe
static bool armor_dye_recipe_matches(const crafting_input & in) {
    return custom_recipe_matches_one_and_some(
        in, [](const item_stack & s) { return (item_defs[s.item].tags & ITEM_TAG_DYEABLE) != 0; },
        custom_recipe_is_dye);
}

// BookCloningRecipe
static bool book_cloning_recipe_matches(const crafting_input & in) {
    return custom_recipe_matches_one_and_some(
        in,
        [](const item_stack & s) { return (item_defs[s.item].components & ITEM_COMPONENT_WRITTEN_BOOK_CONTENT) != 0; },
        [](const item_stack & s) { return (item_defs[s.item].tags & ITEM_TAG_BOOK_CLONING_TARGET) != 0; });
}

// MapCloningRecipe
static bool map_cloning_recipe_matches(const crafting_input & in) {
    return custom_recipe_matches_one_and_some(
        in, [](const item_stack & s) { return (item_defs[s.item].components & ITEM_COMPONENT_MAP_ID) != 0; },
        [](const item_stack & s) { return s.item == ITEM_MAP; });
}

// FireworkStarFadeRecipe
static bool firework_star_fade_recipe_matches(const crafting_input & in) {
    return custom_recipe_matches_one_and_some(
        in,
        [](const item_stack & s) {
            return recipe_field_test(RECIPE_FIELD_FIREWORK_STAR_FADE_RECIPE_STAR_INGREDIENT, s);
        },
        custom_recipe_is_dye);
}

// ShieldDecorationRecipe
static bool shield_decoration_recipe_matches(const crafting_input & in) {
    return in.ingredient_count == 2 &&
           custom_recipe_matches_one_and_some(
               in, [](const item_stack & s) { return item_instanceof(s.item, ICLASS_BANNER_ITEM); },
               [](const item_stack & s) {
                   return s.item == ITEM_SHIELD && !(item_defs[s.item].components & ITEM_COMPONENT_BANNER_PATTERNS);
               });
}

// BannerDuplicateRecipe

static bool banner_duplicate_recipe_matches(const crafting_input & in) {
    if (in.ingredient_count != 2) return false;
    int32_t color      = -1;
    bool    has_target = false;
    bool    has_source = false;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) continue;
        if (!item_instanceof(s.item, ICLASS_BANNER_ITEM)) return false;
        // BannerItem.getColor
        int32_t banner_color = block_field_get(item_defs[s.item].block, FIELD_ABSTRACT_BANNER_BLOCK_COLOR);
        if (color < 0) {
            color = banner_color;
        } else if (color != banner_color) {
            return false;
        }
        // only the item's default patterns, never more than 6 layers
        if (item_defs[s.item].components & ITEM_COMPONENT_BANNER_PATTERNS) {
            if (has_source) return false;
            has_source = true;
        } else {
            if (has_target) return false;
            has_target = true;
        }
    }
    return has_source && has_target;
}

// DecoratedPotRecipe

static bool decorated_pot_recipe_is_ingredient(const item_stack & s) {
    return (item_defs[item_stack_get_item(s)].tags & ITEM_TAG_DECORATED_POT_INGREDIENTS) != 0;
}

// the x, y of the back, left, right and front sides
// clang-format off
static constexpr int32_t DECORATED_POT_RECIPE_SIDES[4][2] = { { 1, 0 }, { 0, 1 }, { 2, 1 }, { 1, 2 } };
// clang-format on

static bool decorated_pot_recipe_matches(const crafting_input & in) {
    if (in.width != 3 || in.height != 3 || in.ingredient_count != 4) return false;
    for (const auto & xy : DECORATED_POT_RECIPE_SIDES) {
        if (!decorated_pot_recipe_is_ingredient(crafting_input_get_item(in, xy[0], xy[1]))) return false;
    }
    return true;
}

// PotDecorations stores a brick side as empty, so a pot of four bricks keeps the default PotDecorations.EMPTY
static item_stack decorated_pot_recipe_assemble(level & lv, const crafting_input & in) {
    for (const auto & xy : DECORATED_POT_RECIPE_SIDES) {
        if (crafting_input_get_item(in, xy[0], xy[1]).item != ITEM_BRICK) {
            level_unsupported(lv, "DecoratedPotRecipe.assemble with sherds");
            return {};
        }
    }
    return { ITEM_DECORATED_POT, 1 };
}

// FireworkRocketRecipe

static bool firework_rocket_recipe_matches(const crafting_input & in) {
    if (in.ingredient_count < 2) return false;
    bool    paper      = false;
    int32_t gun_powder = 0;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) continue;
        if (recipe_field_test(RECIPE_FIELD_FIREWORK_ROCKET_RECIPE_PAPER_INGREDIENT, s)) {
            if (paper) return false;
            paper = true;
        } else if (recipe_field_test(RECIPE_FIELD_FIREWORK_ROCKET_RECIPE_GUNPOWDER_INGREDIENT, s)) {
            if (++gun_powder > 3) return false;
        } else if (!recipe_field_test(RECIPE_FIELD_FIREWORK_ROCKET_RECIPE_STAR_INGREDIENT, s)) {
            return false;
        }
    }
    return paper && gun_powder >= 1;
}

// FireworkStarRecipe

static bool firework_star_recipe_matches(const crafting_input & in) {
    if (in.ingredient_count < 2) return false;
    bool gun_powder = false;
    bool colors     = false;
    bool shape      = false;
    bool trail      = false;
    bool twinkle    = false;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) continue;
        if (recipe_field_test(RECIPE_FIELD_FIREWORK_STAR_RECIPE_SHAPE_BY_ITEM, s)) {
            if (shape) return false;
            shape = true;
        } else if (recipe_field_test(RECIPE_FIELD_FIREWORK_STAR_RECIPE_TWINKLE_INGREDIENT, s)) {
            if (twinkle) return false;
            twinkle = true;
        } else if (recipe_field_test(RECIPE_FIELD_FIREWORK_STAR_RECIPE_TRAIL_INGREDIENT, s)) {
            if (trail) return false;
            trail = true;
        } else if (recipe_field_test(RECIPE_FIELD_FIREWORK_STAR_RECIPE_GUNPOWDER_INGREDIENT, s)) {
            if (gun_powder) return false;
            gun_powder = true;
        } else {
            if (!item_instanceof(s.item, ICLASS_DYE_ITEM)) return false;
            colors = true;
        }
    }
    return gun_powder && colors;
}

// RepairItemRecipe

// RepairItemRecipe.canCombine
static bool repair_item_recipe_can_combine(const item_stack & first, const item_stack & second) {
    return second.item == first.item && first.count == 1 && second.count == 1 && item_stack_is_damageable_item(first) &&
           item_stack_is_damageable_item(second);
}

// RepairItemRecipe.getItemsToCombine: false for null
static bool repair_item_recipe_get_items_to_combine(const crafting_input & in, const item_stack *& first,
                                                    const item_stack *& second) {
    if (in.ingredient_count != 2) return false;
    first = nullptr;
    for (const item_stack & s : in.items) {
        if (item_stack_is_empty(s)) continue;
        if (first != nullptr) {
            second = &s;
            return repair_item_recipe_can_combine(*first, s);
        }
        first = &s;
    }
    return false;
}

static bool repair_item_recipe_matches(const crafting_input & in) {
    const item_stack * first;
    const item_stack * second;
    return repair_item_recipe_get_items_to_combine(in, first, second);
}

// both items are of one kind, so they share MAX_DAMAGE; stacks are unenchanted, so no curses carry over
static item_stack repair_item_recipe_assemble(const crafting_input & in) {
    const item_stack * first;
    const item_stack * second;
    if (!repair_item_recipe_get_items_to_combine(in, first, second)) return item_stack();
    int32_t durability = item_defs[first->item].max_damage;
    int32_t remaining  = durability - first->damage + durability - second->damage + durability * 5 / 100;
    return item_stack{ first->item, 1, std::max(durability - remaining, 0) };
}

// TippedArrowRecipe

static bool tipped_arrow_recipe_matches(const crafting_input & in) {
    if (in.width != 3 || in.height != 3 || in.ingredient_count != 9) return false;
    for (int32_t y = 0; y < in.height; y++) {
        for (int32_t x = 0; x < in.width; x++) {
            item_stack ingredient = crafting_input_get_item(in, x, y);
            if (item_stack_is_empty(ingredient)) return false;
            int32_t expected = x == 1 && y == 1 ? ITEM_LINGERING_POTION : ITEM_ARROW;
            if (ingredient.item != expected) return false;
        }
    }
    return true;
}

// RecipeManager

static bool recipe_matches(level & lv, int32_t recipe, const crafting_input & in) {
    switch (recipe_defs[recipe].cls) {
        case RCLASS_SHAPED_RECIPE: return shaped_recipe_matches(recipe, in);
        case RCLASS_SHAPELESS_RECIPE: return shapeless_recipe_matches(recipe, in);
        case RCLASS_TRANSMUTE_RECIPE: return transmute_recipe_matches(lv, recipe, in);
        case RCLASS_MAP_EXTENDING_RECIPE: return map_extending_recipe_matches(lv, recipe, in);
        case RCLASS_ARMOR_DYE_RECIPE: return armor_dye_recipe_matches(in);
        case RCLASS_BANNER_DUPLICATE_RECIPE: return banner_duplicate_recipe_matches(in);
        case RCLASS_BOOK_CLONING_RECIPE: return book_cloning_recipe_matches(in);
        case RCLASS_DECORATED_POT_RECIPE: return decorated_pot_recipe_matches(in);
        case RCLASS_FIREWORK_ROCKET_RECIPE: return firework_rocket_recipe_matches(in);
        case RCLASS_FIREWORK_STAR_FADE_RECIPE: return firework_star_fade_recipe_matches(in);
        case RCLASS_FIREWORK_STAR_RECIPE: return firework_star_recipe_matches(in);
        case RCLASS_MAP_CLONING_RECIPE: return map_cloning_recipe_matches(in);
        case RCLASS_REPAIR_ITEM_RECIPE: return repair_item_recipe_matches(in);
        case RCLASS_SHIELD_DECORATION_RECIPE: return shield_decoration_recipe_matches(in);
        case RCLASS_TIPPED_ARROW_RECIPE: return tipped_arrow_recipe_matches(in);
    }
    TMC_ASSERT(false);
    return false;
}

// CrafterBlock.RECIPE_CACHE only remembers earlier answers, so it is left out
int32_t recipe_manager_get_recipe_for(level & lv, const crafting_input & in) {
    if (in.ingredient_count == 0) return -1;
    for (int32_t r = 0; r < n_recipe_defs; r++) {
        if (recipe_matches(lv, r, in)) return r;
        if (!lv.unsupported.empty()) return -1;
    }
    return -1;
}

// most special recipes build their results from data components the sim does not keep
item_stack recipe_assemble(level & lv, int32_t recipe, const crafting_input & in) {
    const recipe_def & def = recipe_defs[recipe];
    switch (def.kind) {
        case RECIPE_KIND_SHAPED:
        case RECIPE_KIND_SHAPELESS:
            if (def.result_components) {
                level_unsupported(lv, "recipe %s: result data components", def.name);
                return {};
            }
            return { def.result, def.count };
        case RECIPE_KIND_TRANSMUTE: return transmute_recipe_assemble(recipe, in);
        case RECIPE_KIND_SPECIAL: break;
    }
    switch (def.cls) {
        case RCLASS_REPAIR_ITEM_RECIPE: return repair_item_recipe_assemble(in);
        case RCLASS_DECORATED_POT_RECIPE: return decorated_pot_recipe_assemble(lv, in);
        default: level_unsupported(lv, "%s.assemble", recipe_class_names[def.cls]); return {};
    }
}

// CraftingRecipe.defaultCraftingReminder; BannerDuplicateRecipe and BookCloningRecipe override it, but their assemble stops the run first
std::vector<item_stack> recipe_get_remaining_items(const crafting_input & in) {
    std::vector<item_stack> result;
    for (const item_stack & s : in.items) {
        int32_t remainder = item_defs[item_stack_get_item(s)].crafting_remainder;
        result.push_back(remainder < 0 ? item_stack() : item_stack{ remainder, 1 });
    }
    return result;
}
