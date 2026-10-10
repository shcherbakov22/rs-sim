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

enum check_result {
    CHECK_RESULT_PASS,
    CHECK_RESULT_UNSUPPORTED,  // stopped at something not simulated yet; the trace up to there matches
    CHECK_RESULT_WRONG,        // the trace differs from the oracle, or the case cannot be read
};

static check_result check_case(const std::string & path, bool verbose) {
    std::string golden_path = path.substr(0, path.size() - (path.ends_with(".case") ? 5 : 0)) + ".trace";
    std::string golden;
    case_file   f;
    if (!load_case(path, f)) {
        return CHECK_RESULT_WRONG;
    }
    if (!read_file(golden_path, golden)) {
        printf("WRONG %s: no golden trace %s\n", path.c_str(), golden_path.c_str());
        return CHECK_RESULT_WRONG;
    }
    trace                    tr;
    std::string              err;
    bool                     ok   = case_run(f, tr, err);
    std::vector<std::string> want = split_lines(golden);
    int64_t                  diff = trace_first_diff(want, tr.lines);
    // an unsupported run only produced a prefix, which must match
    if (!ok && (diff < 0 || (size_t) diff == tr.lines.size())) {
        printf("UNSUPPORTED %s: %s (%zu lines match)\n", path.c_str(), err.c_str(), tr.lines.size());
        return CHECK_RESULT_UNSUPPORTED;
    }
    if (diff >= 0) {
        printf("WRONG %s: trace differs%s%s\n", path.c_str(), ok ? "" : " before ", ok ? "" : err.c_str());
        if (verbose) {
            trace_print_diff(want, tr.lines, (size_t) diff);
        }
        return CHECK_RESULT_WRONG;
    }
    printf("PASS %s\n", path.c_str());
    return CHECK_RESULT_PASS;
}

// exit 0 if every case passes (with xfail: is unsupported with a matching prefix)
static int cmd_check(const std::vector<std::string> & paths, bool xfail) {
    int32_t n[3] = {};
    for (const std::string & path : paths) {
        n[check_case(path, paths.size() == 1)]++;
    }
    if (paths.size() > 1) {
        printf("%d pass, %d unsupported, %d wrong\n", n[CHECK_RESULT_PASS], n[CHECK_RESULT_UNSUPPORTED],
               n[CHECK_RESULT_WRONG]);
    }
    check_result expected = xfail ? CHECK_RESULT_UNSUPPORTED : CHECK_RESULT_PASS;
    return n[expected] == (int32_t) paths.size() ? 0 : 1;
}

int main(int argc, char ** argv) {
    if (argc >= 3 && strcmp(argv[1], "run") == 0) {
        return cmd_run(argv[2]);
    }
    if (argc >= 3 && strcmp(argv[1], "check") == 0) {
        bool                     xfail = strcmp(argv[2], "--xfail") == 0;
        std::vector<std::string> paths(argv + (xfail ? 3 : 2), argv + argc);
        return cmd_check(paths, xfail);
    }
    fprintf(stderr, "usage: tmcsim run <file.case>\n");
    fprintf(stderr, "       tmcsim check [--xfail] <file.case>...   (compares with <file.trace>)\n");
    return 2;
}
