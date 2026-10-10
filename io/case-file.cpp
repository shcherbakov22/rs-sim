#include "io/case-file.h"

#include "engine/level-ticks.h"

#include <cerrno>
#include <map>

static std::vector<std::string_view> split_ws(std::string_view line) {
    std::vector<std::string_view> out;
    size_t                        i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
            i++;
        }
        size_t j = i;
        while (j < line.size() && line[j] != ' ' && line[j] != '\t') {
            j++;
        }
        if (j > i) {
            out.push_back(line.substr(i, j - i));
        }
        i = j;
    }
    return out;
}

static std::string_view strip(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

static bool parse_i64(std::string_view s, int64_t & out) {
    std::string str(s);
    char *      end = nullptr;
    errno           = 0;
    out             = strtoll(str.c_str(), &end, 10);
    return !str.empty() && *end == '\0' && errno == 0;
}

static bool parse_i32(std::string_view s, int32_t & out) {
    int64_t v;
    if (!parse_i64(s, v) || v < INT32_MIN || v > INT32_MAX) {
        return false;
    }
    out = (int32_t) v;
    return true;
}

static bool parse_pos(const std::vector<std::string_view> & t, size_t i, block_pos & out) {
    return t.size() >= i + 3 && parse_i32(t[i], out.x) && parse_i32(t[i + 1], out.y) && parse_i32(t[i + 2], out.z);
}

static bool parse_input(const std::vector<std::string_view> & t, case_input & in, std::string & err) {
    if (!t[0].starts_with("@") || !parse_i32(t[0].substr(1), in.at) || t.size() < 5 || !parse_pos(t, 2, in.pos)) {
        err = "expected '@<ticks> <verb> x y z ...'";
        return false;
    }
    if (t[1] == "use") {
        in.verb  = CASE_VERB_USE;
        size_t i = 5;
        if (t.size() > i && t[i] != "with") {
            if (!direction_parse(t[i], in.face)) {
                err = "unknown face " + std::string(t[i]);
                return false;
            }
            i++;
        }
        if (t.size() > i) {
            if (t[i] != "with" || t.size() < i + 2) {
                err = "expected 'with <item> [count]'";
                return false;
            }
            in.item  = t[i + 1];
            in.count = 1;
            if (t.size() > i + 2 && !parse_i32(t[i + 2], in.count)) {
                err = "bad count";
                return false;
            }
        }
        return true;
    }
    if (t[1] == "place") {
        in.verb = CASE_VERB_PLACE;
        if (t.size() != 6) {
            err = "expected 'place x y z <state>'";
            return false;
        }
        return state_parse(t[5], in.state, err);
    }
    if (t.size() != 5) {
        err = "unexpected tokens after position";
        return false;
    }
    if (t[1] == "attack") {
        in.verb = CASE_VERB_ATTACK;
    } else if (t[1] == "break") {
        in.verb = CASE_VERB_BREAK;
    } else if (t[1] == "close") {
        in.verb = CASE_VERB_CLOSE;
    } else {
        err = "unknown input " + std::string(t[1]);
        return false;
    }
    return true;
}

// "minecraft:chest[facing=west,type=single]{Items:[]}": state, then optional NBT
static bool parse_key_block(std::string_view text, state_id & st, std::string & snbt, std::string & err) {
    size_t open  = text.find('[');
    size_t brace = text.find('{');
    if (open != std::string_view::npos && open < brace) {
        brace = text.find('{', text.find(']', open));
    }
    if (brace != std::string_view::npos) {
        snbt = text.substr(brace);
        text = text.substr(0, brace);
    }
    return state_parse(text, st, err);
}

// parse position: current section, layer and row, and the key seen so far
struct case_parser {
    std::string section;
    int32_t     layer_y = -1;
    int32_t     row     = 0;

    std::map<std::string, state_id, std::less<>> key_state = {
        { ".", STATE_AIR }
    };
    std::map<std::string, std::string, std::less<>> key_nbt;
};

static bool case_parse_line(case_file & f, case_parser & p, std::string_view line, std::string & err) {
    std::vector<std::string_view> t = split_ws(line);
    if (t[0] == "tmc-case") {
        if (t.size() != 2 || t[1] != "1") {
            err = "unsupported version";
            return false;
        }
        return true;
    }
    if (t[0] == "size") {
        if (!parse_pos(t, 1, f.size) || f.size.x <= 0 || f.size.y <= 0 || f.size.z <= 0) {
            err = "bad size";
            return false;
        }
        f.states.assign((size_t) f.size.x * f.size.y * f.size.z, STATE_AIR);
        return true;
    }
    if (t[0] == "seed") {
        if (t.size() != 2 || !parse_i64(t[1], f.seed)) {
            err = "bad seed";
            return false;
        }
        return true;
    }
    if (t[0] == "gaussian") {
        std::string s(t.size() == 2 ? t[1] : "");
        char *      end = nullptr;
        f.gaussian      = strtod(s.c_str(), &end);
        f.has_gaussian  = true;
        if (s.empty() || *end != '\0') {
            err = "bad gaussian";
            return false;
        }
        return true;
    }
    if (t[0] == "run") {
        p.section = "";
        if (t.size() != 2 || !parse_i32(t[1], f.run)) {
            err = "bad run";
            return false;
        }
        return true;
    }
    if (t[0] == "key" || t[0] == "ticks" || t[0] == "events" || t[0] == "toggles" || t[0] == "tickers" ||
        t[0] == "inputs") {
        p.section = t[0];
        return true;
    }
    if (t[0] == "layer") {
        p.section = "layer";
        p.row     = 0;
        if (t.size() != 2 || !parse_i32(t[1], p.layer_y) || p.layer_y < 0 || p.layer_y >= f.size.y) {
            err = "bad layer";
            return false;
        }
        return true;
    }
    if (f.states.empty() && p.section != "key") {
        err = "size must come before " + p.section;
        return false;
    }
    if (p.section == "key") {
        size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            err = "expected '<key> = <block>'";
            return false;
        }
        std::string name(strip(line.substr(0, eq)));
        state_id    st;
        std::string snbt;
        if (!parse_key_block(strip(line.substr(eq + 1)), st, snbt, err)) {
            return false;
        }
        p.key_state[name] = st;
        if (!snbt.empty()) {
            p.key_nbt[name] = snbt;
        }
        return true;
    }
    if (p.section == "layer") {
        if ((int32_t) t.size() != f.size.x) {
            err = "p.row has " + std::to_string(t.size()) + " entries, size x is " + std::to_string(f.size.x);
            return false;
        }
        if (p.row >= f.size.z) {
            err = "too many rows for size z";
            return false;
        }
        for (int32_t x = 0; x < f.size.x; x++) {
            auto it = p.key_state.find(t[x]);
            if (it == p.key_state.end()) {
                err = "unknown key '" + std::string(t[x]) + "'";
                return false;
            }
            f.states[x + f.size.x * (p.row + f.size.z * p.layer_y)] = it->second;
            auto nbt                                                = p.key_nbt.find(t[x]);
            if (nbt != p.key_nbt.end()) {
                f.nbt.push_back({
                    { x, p.layer_y, p.row },
                    nbt->second
                });
            }
        }
        p.row++;
        return true;
    }
    if (p.section == "ticks") {
        case_tick tk;
        tk.block = t.size() == 6 ? block_find(t[3]) : -1;
        if (!parse_pos(t, 0, tk.pos) || tk.block < 0 || !parse_i64(t[4], tk.delay) ||
            !tick_priority_parse(t[5], tk.priority)) {
            err = "expected 'x y z <block> <delay> <priority>'";
            return false;
        }
        f.ticks.push_back(tk);
        return true;
    }
    if (p.section == "events") {
        case_event ev;
        ev.block = t.size() == 6 ? block_find(t[3]) : -1;
        if (!parse_pos(t, 0, ev.pos) || ev.block < 0 || !parse_i32(t[4], ev.param_a) || !parse_i32(t[5], ev.param_b)) {
            err = "expected 'x y z <block> <a> <b>'";
            return false;
        }
        f.events.push_back(ev);
        return true;
    }
    if (p.section == "toggles") {
        case_toggle tg;
        if (t.size() != 4 || !parse_pos(t, 0, tg.pos) || !parse_i64(t[3], tg.when)) {
            err = "expected 'x y z <when>'";
            return false;
        }
        f.toggles.push_back(tg);
        return true;
    }
    if (p.section == "tickers") {
        block_pos p;
        if (t.size() != 3 || !parse_pos(t, 0, p)) {
            err = "expected 'x y z'";
            return false;
        }
        f.tickers.push_back(p);
        return true;
    }
    if (p.section == "inputs") {
        case_input in;
        if (!parse_input(t, in, err)) {
            return false;
        }
        f.inputs.push_back(in);
        return true;
    }
    err = "line outside any p.section";
    return false;
}

bool case_parse(const std::string & text, case_file & out, std::string & err) {
    out = case_file();
    case_parser p;
    int32_t     n     = 0;
    size_t      start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        std::string_view line(text.data() + start, end - start);
        start = end + 1;
        n++;
        // '#' starts a comment; block strings never contain it
        line = strip(line.substr(0, line.find('#')));
        if (line.empty()) {
            continue;
        }
        if (!case_parse_line(out, p, line, err)) {
            err = "line " + std::to_string(n) + ": " + err;
            return false;
        }
    }
    if (out.states.empty()) {
        err = "missing size";
        return false;
    }
    return true;
}
