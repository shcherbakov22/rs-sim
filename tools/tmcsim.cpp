#include "engine/case-run.h"

#include <cstring>
#include <string>
#include <vector>

static bool read_file(const std::string & path, std::string & out) {
    FILE * f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    char   buf[65536];
    size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    fclose(f);
    return true;
}

static std::vector<std::string> split_lines(const std::string & text) {
    std::vector<std::string> out;
    size_t                   start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        out.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

// index of the first line that differs outside annotations, or -1
static int64_t trace_first_diff(const std::vector<std::string> & want, const std::vector<std::string> & got) {
    size_t n = want.size() < got.size() ? want.size() : got.size();
    for (size_t i = 0; i < n; i++) {
        if (trace_strip(want[i]) != trace_strip(got[i])) {
            return (int64_t) i;
        }
    }
    return want.size() == got.size() ? -1 : (int64_t) n;
}

// like the oracle's traceDiff: the first differing line with 3 lines of context
static void trace_print_diff(const std::vector<std::string> & want, const std::vector<std::string> & got, size_t i) {
    printf("  differs at line %zu (%zu vs %zu lines):\n", i + 1, want.size(), got.size());
    for (size_t j = i < 3 ? 0 : i - 3; j < i; j++) {
        printf("      %s\n", want[j].c_str());
    }
    printf("    - %s\n", i < want.size() ? want[i].c_str() : "<end>");
    printf("    + %s\n", i < got.size() ? got[i].c_str() : "<end>");
}

static bool load_case(const std::string & path, case_file & f) {
    std::string text;
    std::string err;
    if (!read_file(path, text)) {
        fprintf(stderr, "%s: cannot read\n", path.c_str());
        return false;
    }
    if (!case_parse(text, f, err)) {
        fprintf(stderr, "%s: %s\n", path.c_str(), err.c_str());
        return false;
    }
    return true;
}

static int cmd_run(const std::string & path) {
    case_file f;
    if (!load_case(path, f)) {
        return 1;
    }
    trace       tr;
    std::string err;
    bool        ok = case_run(f, tr, err);
    for (const std::string & line : tr.lines) {
        printf("%s\n", line.c_str());
    }
    if (!ok) {
        fprintf(stderr, "%s: %s\n", path.c_str(), err.c_str());
        return 1;
    }
    return 0;
}

static int cmd_check(const std::vector<std::string> & paths) {
    int32_t n_pass = 0;
    for (const std::string & path : paths) {
        std::string golden_path = path.substr(0, path.size() - (path.ends_with(".case") ? 5 : 0)) + ".trace";
        std::string golden;
        case_file   f;
        if (!load_case(path, f)) {
            continue;
        }
        if (!read_file(golden_path, golden)) {
            printf("FAIL %s: no golden trace %s\n", path.c_str(), golden_path.c_str());
            continue;
        }
        trace       tr;
        std::string err;
        if (!case_run(f, tr, err)) {
            printf("FAIL %s: %s\n", path.c_str(), err.c_str());
            continue;
        }
        std::vector<std::string> want = split_lines(golden);
        int64_t                  diff = trace_first_diff(want, tr.lines);
        if (diff >= 0) {
            printf("FAIL %s: trace differs\n", path.c_str());
            if (paths.size() == 1) {
                trace_print_diff(want, tr.lines, (size_t) diff);
            }
            continue;
        }
        printf("PASS %s\n", path.c_str());
        n_pass++;
    }
    if (paths.size() > 1) {
        printf("%d/%zu pass\n", n_pass, paths.size());
    }
    return n_pass == (int32_t) paths.size() ? 0 : 1;
}

int main(int argc, char ** argv) {
    if (argc >= 3 && strcmp(argv[1], "run") == 0) {
        return cmd_run(argv[2]);
    }
    if (argc >= 3 && strcmp(argv[1], "check") == 0) {
        std::vector<std::string> paths(argv + 2, argv + argc);
        return cmd_check(paths);
    }
    fprintf(stderr, "usage: tmcsim run <file.case>\n");
    fprintf(stderr, "       tmcsim check <file.case>...   (compares with <file.trace>)\n");
    return 2;
}
