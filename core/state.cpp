#include "core/state.h"

int32_t block_find(std::string_view name) {
    if (name.starts_with("minecraft:")) {
        name.remove_prefix(10);
    }
    for (int32_t i = 0; i < n_block_defs; i++) {
        if (name == block_defs[i].name) {
            return i;
        }
    }
    return -1;
}

int32_t state_block(state_id st) {
    TMC_ASSERT(st < n_states);
    int32_t lo = 0;
    int32_t hi = n_block_defs - 1;
    while (lo < hi) {
        int32_t mid = (lo + hi + 1) / 2;
        if (block_defs[mid].state_first <= st) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return lo;
}

bool block_instanceof(int32_t block, vclass cls) {
    for (int32_t c = block_defs[block].cls; c != VCLASS_NONE; c = vclass_super[c]) {
        if (c == cls) {
            return true;
        }
    }
    return false;
}

int32_t block_field_get(int32_t block, block_field field) {
    const block_def & b = block_defs[block];
    for (int32_t i = 0; i < b.n_fields; i++) {
        if (block_field_values[b.fields_first + i].field == field) {
            return block_field_values[b.fields_first + i].value;
        }
    }
    TMC_ABORT("%s has no field %d", b.name, field);
}

static const block_prop * state_find_prop(state_id st, prop_id prop) {
    const block_def & b = block_defs[state_block(st)];
    for (int32_t i = 0; i < b.n_props; i++) {
        if (block_props[b.props_first + i].prop == prop) {
            return &block_props[b.props_first + i];
        }
    }
    return nullptr;
}

static const block_prop & state_need_prop(state_id st, prop_id prop) {
    const block_prop * bp = state_find_prop(st, prop);
    if (bp == nullptr) {
        TMC_ABORT("%s has no property %s", state_to_string(st).c_str(), prop_defs[prop].name);
    }
    return *bp;
}

static int32_t state_value_index(state_id st, const block_prop & bp) {
    int32_t first = block_defs[state_block(st)].state_first;
    return (st - first) / bp.stride % prop_defs[bp.prop].n_values;
}

static state_id state_with_index(state_id st, const block_prop & bp, int32_t index) {
    return (state_id) (st + (index - state_value_index(st, bp)) * bp.stride);
}

bool state_has_prop(state_id st, prop_id prop) {
    return state_find_prop(st, prop) != nullptr;
}

int32_t state_get(state_id st, prop_id prop) {
    const block_prop & bp = state_need_prop(st, prop);
    return prop_defs[prop].codes[state_value_index(st, bp)];
}

state_id state_set(state_id st, prop_id prop, int32_t value) {
    const block_prop & bp = state_need_prop(st, prop);
    const prop_def &   p  = prop_defs[prop];
    for (int32_t i = 0; i < p.n_values; i++) {
        if (p.codes[i] == value) {
            return state_with_index(st, bp, i);
        }
    }
    TMC_ABORT("no value %d for property %s", value, p.name);
}

state_id state_cycle(state_id st, prop_id prop) {
    const block_prop & bp = state_need_prop(st, prop);
    return state_with_index(st, bp, (state_value_index(st, bp) + 1) % prop_defs[prop].n_values);
}

std::string state_to_string(state_id st) {
    const block_def & b     = block_defs[state_block(st)];
    std::string       out   = b.name;
    int32_t           index = st - b.state_first;
    for (int32_t i = 0; i < b.n_props; i++) {
        const block_prop & bp = block_props[b.props_first + i];
        const prop_def &   p  = prop_defs[bp.prop];
        out += i == 0 ? "[" : ",";
        out += p.name;
        out += "=";
        out += p.values[index / bp.stride % p.n_values];
    }
    if (b.n_props > 0) {
        out += "]";
    }
    return out;
}

bool state_parse(std::string_view text, state_id & out, std::string & err) {
    size_t  bracket = text.find('[');
    int32_t block   = block_find(text.substr(0, bracket));
    if (block < 0) {
        err = "unknown block " + std::string(text.substr(0, bracket));
        return false;
    }
    const block_def & b     = block_defs[block];
    int32_t           index = b.state_default - b.state_first;
    if (bracket == std::string_view::npos) {
        out = b.state_default;
        return true;
    }
    if (!text.ends_with(']')) {
        err = "expected ']' in " + std::string(text);
        return false;
    }
    std::string_view rest = text.substr(bracket + 1, text.size() - bracket - 2);
    while (!rest.empty()) {
        size_t           comma = rest.find(',');
        std::string_view pair  = rest.substr(0, comma);
        rest                   = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
        size_t eq              = pair.find('=');
        if (eq == std::string_view::npos) {
            err = "expected name=value, got " + std::string(pair);
            return false;
        }
        std::string_view name  = pair.substr(0, eq);
        std::string_view value = pair.substr(eq + 1);
        bool             found = false;
        for (int32_t i = 0; i < b.n_props && !found; i++) {
            const block_prop & bp = block_props[b.props_first + i];
            const prop_def &   p  = prop_defs[bp.prop];
            if (name != p.name) {
                continue;
            }
            for (int32_t v = 0; v < p.n_values; v++) {
                if (value == p.values[v]) {
                    index += (v - index / bp.stride % p.n_values) * bp.stride;
                    found = true;
                    break;
                }
            }
            if (!found) {
                err = "bad value " + std::string(value) + " for " + std::string(name);
                return false;
            }
        }
        if (!found) {
            err = std::string(b.name) + " has no property " + std::string(name);
            return false;
        }
    }
    out = (state_id) (b.state_first + index);
    return true;
}
