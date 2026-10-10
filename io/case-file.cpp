#include "io/case-file.h"

#include "engine/level-ticks.h"

#include <charconv>
#include <map>

static std::vector<std::string_view> split_ws(std::string_view line) {
    std::vector<std::string_view> out;
    size_t                        i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
        size_t j = i;
        while (j < line.size() && line[j] != ' ' && line[j] != '\t') j++;
        if (j > i) out.push_back(line.substr(i, j - i));
        i = j;
    }
    return out;
}

static std::string_view strip(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

static bool parse_i64(std::string_view s, int64_t & out) {
    std::from_chars_result r = std::from_chars(s.data(), s.data() + s.size(), out);
    return r.ec == std::errc() && r.ptr == s.data() + s.size();
}

static bool parse_i32(std::string_view s, int32_t & out) {
    std::from_chars_result r = std::from_chars(s.data(), s.data() + s.size(), out);
    return r.ec == std::errc() && r.ptr == s.data() + s.size();
}

static bool parse_pos(const std::vector<std::string_view> & t, size_t i, block_pos & out) {
    return t.size() >= i + 3 && parse_i32(t[i], out.x) && parse_i32(t[i + 1], out.y) && parse_i32(t[i + 2], out.z);
}

static bool case_error(std::string & err, const std::string & message) {
    err = message;
    return false;
}

static bool parse_input(const std::vector<std::string_view> & t, case_input & in, std::string & err) {
    if (!t[0].starts_with("@") || !parse_i32(t[0].substr(1), in.at) || t.size() < 5 || !parse_pos(t, 2, in.pos)) {
        return case_error(err, "expected '@<ticks> <verb> x y z ...'");
    }
    if (t[1] == "place") {
        in.verb = CASE_VERB_PLACE;
        if (t.size() != 6) return case_error(err, "expected 'place x y z <state>'");
        return state_parse(t[5], in.state, err);
    }
    if (t.size() != 5) return case_error(err, "unexpected tokens after position");
    if (t[1] == "break") {
        in.verb = CASE_VERB_BREAK;
    } else if (t[1] == "pull") {
        in.verb = CASE_VERB_PULL;
    } else {
        return case_error(err, "unknown input " + std::string(t[1]));
    }
    return true;
}

// parse position: current section, layer and row, and the key seen so far
struct case_parser {
    std::string section;
    int32_t     layer_y = -1;
    int32_t     row     = 0;

    std::map<std::string, state_id, std::less<>> key_state = {
        { ".", STATE_AIR }
    };
};

static bool case_parse_line(case_file & f, case_parser & p, std::string_view line, std::string & err) {
    std::vector<std::string_view> t = split_ws(line);
    if (t[0] == "tmc-case") {
        if (t.size() != 2 || t[1] != "2") return case_error(err, "unsupported version");
    } else if (t[0] == "size") {
        if (!parse_pos(t, 1, f.size) || f.size.x <= 0 || f.size.y <= 0 || f.size.z <= 0) {
            return case_error(err, "bad size");
        }
        f.states.assign((size_t) f.size.x * f.size.y * f.size.z, STATE_AIR);
    } else if (t[0] == "seed") {
        if (t.size() != 2 || !parse_i64(t[1], f.seed)) return case_error(err, "bad seed");
    } else if (t[0] == "run") {
        p.section = "";
        if (t.size() != 2 || !parse_i32(t[1], f.run)) return case_error(err, "bad run");
    } else if (t[0] == "key" || t[0] == "items" || t[0] == "data" || t[0] == "hidden" || t[0] == "ticks" ||
               t[0] == "events" || t[0] == "toggles" || t[0] == "tickers" || t[0] == "inputs") {
        p.section = t[0];
    } else if (t[0] == "layer") {
        p.section = "layer";
        p.row     = 0;
        if (t.size() != 2 || !parse_i32(t[1], p.layer_y) || p.layer_y < 0 || p.layer_y >= f.size.y) {
            return case_error(err, "bad layer");
        }
    } else if (f.states.empty() && p.section != "key") {
        return case_error(err, "size must come before " + p.section);
    } else if (p.section == "key") {
        size_t eq = line.find('=');
        if (eq == std::string_view::npos) return case_error(err, "expected '<key> = <block>'");
        return state_parse(strip(line.substr(eq + 1)), p.key_state[std::string(strip(line.substr(0, eq)))], err);
    } else if (p.section == "layer") {
        if ((int32_t) t.size() != f.size.x) {
            return case_error(
                err, "row has " + std::to_string(t.size()) + " entries, size x is " + std::to_string(f.size.x));
        }
        if (p.row >= f.size.z) return case_error(err, "too many rows for size z");
        for (int32_t x = 0; x < f.size.x; x++) {
            auto it = p.key_state.find(t[x]);
            if (it == p.key_state.end()) return case_error(err, "unknown key '" + std::string(t[x]) + "'");
            f.states[x + f.size.x * (p.row + f.size.z * p.layer_y)] = it->second;
        }
        p.row++;
    } else if (p.section == "items") {
        case_item it = { {}, 0, t.size() >= 5 ? item_find(t[4]) : -1, 0, -1 };
        if (t.size() < 6 || t.size() > 7 || !parse_pos(t, 0, it.pos) || !parse_i32(t[3], it.slot) || it.item < 0 ||
            !parse_i32(t[5], it.count) || (t.size() == 7 && !parse_i32(t[6], it.damage))) {
            return case_error(err, "expected 'x y z <slot> <item> <count> [damage]'");
        }
        f.items.push_back(it);
    } else if (p.section == "data" || p.section == "hidden") {
        // the oracle reads values with TagParser, so a case that has a trace holds only valid ones
        case_data d = { {}, t.size() >= 5 ? std::string(t[3]) : "", "", p.section == "hidden" };
        if (t.size() < 5 || !parse_pos(t, 0, d.pos)) return case_error(err, "expected 'x y z <key> <value>'");
        d.value = line.substr(t[4].data() - line.data());
        f.data.push_back(d);
    } else if (p.section == "ticks") {
        case_tick tk;
        tk.block = t.size() == 6 ? block_find(t[3]) : -1;
        if (!parse_pos(t, 0, tk.pos) || tk.block < 0 || !parse_i64(t[4], tk.delay) || tk.delay < 1 ||
            !tick_priority_parse(t[5], tk.priority)) {
            return case_error(err, "expected 'x y z <block> <delay >= 1> <priority>'");
        }
        f.ticks.push_back(tk);
    } else if (p.section == "events") {
        case_event ev;
        ev.block = t.size() == 6 ? block_find(t[3]) : -1;
        if (!parse_pos(t, 0, ev.pos) || ev.block < 0 || !parse_i32(t[4], ev.param_a) || !parse_i32(t[5], ev.param_b)) {
            return case_error(err, "expected 'x y z <block> <a> <b>'");
        }
        f.events.push_back(ev);
    } else if (p.section == "toggles") {
        case_toggle tg;
        if (t.size() != 4 || !parse_pos(t, 0, tg.pos) || !parse_i64(t[3], tg.when)) {
            return case_error(err, "expected 'x y z <when>'");
        }
        f.toggles.push_back(tg);
    } else if (p.section == "tickers") {
        block_pos pos;
        if (t.size() != 3 || !parse_pos(t, 0, pos)) return case_error(err, "expected 'x y z'");
        f.tickers.push_back(pos);
    } else if (p.section == "inputs") {
        case_input in;
        if (!parse_input(t, in, err)) return false;
        f.inputs.push_back(in);
    } else {
        return case_error(err, "line outside any section");
    }
    return true;
}

bool case_parse(const std::string & text, case_file & out, std::string & err) {
    out = case_file();
    case_parser p;
    int32_t     n     = 0;
    size_t      start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string_view line(text.data() + start, end - start);
        start = end + 1;
        n++;
        // '#' starts a comment; block strings never contain it
        line = strip(line.substr(0, line.find('#')));
        if (line.empty()) continue;
        if (!case_parse_line(out, p, line, err)) return case_error(err, "line " + std::to_string(n) + ": " + err);
    }
    if (out.states.empty()) return case_error(err, "missing size");
    return true;
}
