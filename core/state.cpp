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
    const block_def & b = block_defs[block];
    for (int32_t i = 0; i < b.n_chain; i++) {
        if (block_chains[b.chain_first + i] == cls) {
            return true;
        }
    }
    return false;
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
