#include "io/snbt.h"

#include <cstdlib>

struct snbt_reader {
    std::string_view text;
    size_t           at = 0;
    std::string      err;
};

static bool snbt_fail(snbt_reader & r, const char * what) {
    if (r.err.empty()) {
        r.err = std::string(what) + " at offset " + std::to_string(r.at);
    }
    return false;
}

static bool snbt_eat(snbt_reader & r, char c) {
    if (r.at < r.text.size() && r.text[r.at] == c) {
        r.at++;
        return true;
    }
    return false;
}

static bool snbt_is_bare(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
           c == '.' || c == '+';
}

static bool snbt_parse_quoted(snbt_reader & r, std::string & out) {
    char quote = r.text[r.at++];
    while (r.at < r.text.size() && r.text[r.at] != quote) {
        if (r.text[r.at] == '\\' && r.at + 1 < r.text.size()) {
            r.at++;
        }
        out += r.text[r.at++];
    }
    return snbt_eat(r, quote) || snbt_fail(r, "unterminated string");
}

static bool snbt_parse_key(snbt_reader & r, std::string & out) {
    if (r.at < r.text.size() && (r.text[r.at] == '"' || r.text[r.at] == '\'')) {
        return snbt_parse_quoted(r, out);
    }
    while (r.at < r.text.size() && snbt_is_bare(r.text[r.at])) {
        out += r.text[r.at++];
    }
    return !out.empty() || snbt_fail(r, "expected a key");
}

// a bare number with its type suffix
static bool snbt_parse_number(snbt_reader & r, snbt_value & out) {
    std::string word;
    while (r.at < r.text.size() && snbt_is_bare(r.text[r.at])) {
        word += r.text[r.at++];
    }
    if (word.empty()) {
        return snbt_fail(r, "expected a value");
    }
    char        last = word.back();
    std::string num  = word;
    out.type         = SNBT_INT;
    switch (last) {
        case 'b':
        case 'B':
            out.type = SNBT_BYTE;
            break;
        case 's':
        case 'S':
            out.type = SNBT_SHORT;
            break;
        case 'l':
        case 'L':
            out.type = SNBT_LONG;
            break;
        case 'f':
        case 'F':
            out.type = SNBT_FLOAT;
            break;
        case 'd':
        case 'D':
            out.type = SNBT_DOUBLE;
            break;
        default:
            break;
    }
    if (out.type != SNBT_INT) {
        num.pop_back();
    }
    char * end = nullptr;
    if (out.type == SNBT_FLOAT || out.type == SNBT_DOUBLE) {
        out.d = strtod(num.c_str(), &end);
    } else {
        out.i = strtoll(num.c_str(), &end, 10);
    }
    return (end != nullptr && *end == '\0' && !num.empty()) || snbt_fail(r, "bad number");
}

static bool snbt_parse_value(snbt_reader & r, snbt_value & out);

static bool snbt_parse_compound(snbt_reader & r, snbt_value & out) {
    out.type = SNBT_COMPOUND;
    r.at++;
    if (snbt_eat(r, '}')) {
        return true;
    }
    do {
        snbt_entry entry;
        if (!snbt_parse_key(r, entry.key) || !(snbt_eat(r, ':') || snbt_fail(r, "expected ':'")) ||
            !snbt_parse_value(r, entry.value)) {
            return false;
        }
        out.compound.push_back(std::move(entry));
    } while (snbt_eat(r, ','));
    return snbt_eat(r, '}') || snbt_fail(r, "expected '}'");
}

static bool snbt_parse_list(snbt_reader & r, snbt_value & out) {
    out.type = SNBT_LIST;
    r.at++;
    // typed arrays: [B;...], [I;...], [L;...]
    if (r.at + 1 < r.text.size() && r.text[r.at + 1] == ';') {
        r.at += 2;
    }
    if (snbt_eat(r, ']')) {
        return true;
    }
    do {
        out.list.emplace_back();
        if (!snbt_parse_value(r, out.list.back())) {
            return false;
        }
    } while (snbt_eat(r, ','));
    return snbt_eat(r, ']') || snbt_fail(r, "expected ']'");
}

static bool snbt_parse_value(snbt_reader & r, snbt_value & out) {
    if (r.at >= r.text.size()) {
        return snbt_fail(r, "expected a value");
    }
    switch (r.text[r.at]) {
        case '{':
            return snbt_parse_compound(r, out);
        case '[':
            return snbt_parse_list(r, out);
        case '"':
        case '\'':
            out.type = SNBT_STRING;
            return snbt_parse_quoted(r, out.s);
        default:
            return snbt_parse_number(r, out);
    }
}

bool snbt_parse(std::string_view text, snbt_value & out, std::string & err) {
    snbt_reader r;
    r.text = text;
    out    = snbt_value();
    if (!snbt_parse_value(r, out)) {
        err = r.err;
        return false;
    }
    if (r.at != text.size()) {
        snbt_fail(r, "trailing text");
        err = r.err;
        return false;
    }
    return true;
}
