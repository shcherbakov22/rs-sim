#include "engine/case-run.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

static bool read_file(const std::string & path, std::string & out) {
    FILE * f = fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    char   buf[65536];
    size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}

static std::vector<std::string> split_lines(const std::string & text) {
    std::vector<std::string> out;
    size_t                   start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        out.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

// index of the first line that differs outside annotations, or -1
static int64_t trace_first_diff(const std::vector<std::string> & want, const std::vector<std::string> & got) {
    size_t n = want.size() < got.size() ? want.size() : got.size();
    for (size_t i = 0; i < n; i++) {
        if (trace_strip(want[i]) != trace_strip(got[i])) return (int64_t) i;
    }
    return want.size() == got.size() ? -1 : (int64_t) n;
}

// like the oracle's traceDiff: the first differing line with 3 lines of context
static void trace_print_diff(const std::vector<std::string> & want, const std::vector<std::string> & got, size_t i) {
    printf("  differs at line %zu (%zu vs %zu lines):\n", i + 1, want.size(), got.size());
    for (size_t j = i < 3 ? 0 : i - 3; j < i; j++) printf("      %s\n", want[j].c_str());
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
    if (!load_case(path, f)) return 1;
    level       lv;
    trace       tr;
    std::string err;
    bool        ok = case_run(f, lv, tr, err);
    for (const std::string & line : tr.lines) printf("%s\n", line.c_str());
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
    if (!load_case(path, f)) return CHECK_RESULT_WRONG;
    if (!read_file(golden_path, golden)) {
        printf("WRONG %s: no golden trace %s\n", path.c_str(), golden_path.c_str());
        return CHECK_RESULT_WRONG;
    }
    level                    lv;
    trace                    tr;
    std::string              err;
    bool                     ok   = case_run(f, lv, tr, err);
    std::vector<std::string> want = split_lines(golden);
    int64_t                  diff = trace_first_diff(want, tr.lines);
    // an unsupported run only produced a prefix, which must match
    if (!ok && (diff < 0 || (size_t) diff == tr.lines.size())) {
        printf("UNSUPPORTED %s: %s (%zu lines match)\n", path.c_str(), err.c_str(), tr.lines.size());
        return CHECK_RESULT_UNSUPPORTED;
    }
    if (diff >= 0) {
        printf("WRONG %s: trace differs%s%s\n", path.c_str(), ok ? "" : " before ", ok ? "" : err.c_str());
        if (verbose) trace_print_diff(want, tr.lines, (size_t) diff);
        return CHECK_RESULT_WRONG;
    }
    printf("PASS %s\n", path.c_str());
    return CHECK_RESULT_PASS;
}

// exit 0 if every case passes (with xfail: is unsupported with a matching prefix)
static int cmd_check(const std::vector<std::string> & paths, bool xfail) {
    int32_t n[3] = {};
    for (const std::string & path : paths) n[check_case(path, paths.size() == 1)]++;
    if (paths.size() > 1) {
        printf("%d pass, %d unsupported, %d wrong\n", n[CHECK_RESULT_PASS], n[CHECK_RESULT_UNSUPPORTED],
               n[CHECK_RESULT_WRONG]);
    }
    check_result expected = xfail ? CHECK_RESULT_UNSUPPORTED : CHECK_RESULT_PASS;
    return n[expected] == (int32_t) paths.size() ? 0 : 1;
}

static void fingerprint_add(uint64_t & h, int64_t v) { h = (h ^ (uint64_t) v) * 0x100000001b3ull; }

// FNV-1a over what later ticks can read: time, RNG, blocks, block entities, ticks, events, tickers, toggles, entities
static uint64_t level_fingerprint(const level & lv) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (int64_t v : { lv.game_time, lv.sub_tick_count, lv.random.seed }) fingerprint_add(h, v);
    for (const level_section_ref & s : level_sections(lv)) {
        fingerprint_add(h, s.key);
        for (state_id st : s.section->states) fingerprint_add(h, st);
    }
    std::vector<int64_t> keys;
    for (const auto & [key, be] : lv.block_entities) keys.push_back(key);
    std::sort(keys.begin(), keys.end());
    for (int64_t key : keys) {
        const block_entity & be = *lv.block_entities.at(key);
        for (int64_t v : { (int64_t) be.type, (int64_t) be.state, (int64_t) be.moved_state, (int64_t) be.dir,
                           (int64_t) be.extending, (int64_t) be.is_source_piston, (int64_t) (be.progress * 1e6f),
                           (int64_t) (be.progress_o * 1e6f), be.last_ticked, (int64_t) be.output_signal,
                           (int64_t) be.cooldown_time, be.ticked_game_time, (int64_t) be.song_playing,
                           be.ticks_since_song_started, (int64_t) be.last_interacted_slot, (int64_t) be.disabled_slots,
                           (int64_t) be.crafting_ticks_remaining }) {
            fingerprint_add(h, v);
        }
        for (const item_stack & s : be.items) {
            for (int64_t v : { s.item, s.count, s.damage, s.contents }) fingerprint_add(h, v);
        }
    }
    for (const level_ticks * ticks : { &lv.block_ticks, &lv.fluid_ticks }) {
        for (const scheduled_tick & t : ticks->pending) {
            for (int64_t v : { (int64_t) t.type, (int64_t) t.pos.x, (int64_t) t.pos.y, (int64_t) t.pos.z,
                               t.trigger_tick, (int64_t) t.priority, t.sub_tick_order }) {
                fingerprint_add(h, v);
            }
        }
    }
    for (const block_event & e : lv.block_events) {
        for (int64_t v : { e.pos.x, e.pos.y, e.pos.z, e.block, e.param_a, e.param_b }) fingerprint_add(h, v);
    }
    for (const block_entity_ticker * ticker : lv.block_entity_tickers) {
        if (ticker->be == nullptr) continue;
        for (int64_t v : { ticker->be->pos.x, ticker->be->pos.y, ticker->be->pos.z }) fingerprint_add(h, v);
    }
    for (const redstone_torch_toggle & t : lv.redstone_torch_recent_toggles) {
        for (int64_t v : { (int64_t) t.pos.x, (int64_t) t.pos.y, (int64_t) t.pos.z, t.when }) fingerprint_add(h, v);
    }
    fingerprint_add(h, lv.entity_counter);
    for (const entity * e : lv.entity_tick_list) {
        for (double v : { e->pos.x, e->pos.y, e->pos.z, e->delta.x, e->delta.y, e->delta.z, e->fall_distance,
                          e->stuck_speed_multiplier.x, e->stuck_speed_multiplier.y, e->stuck_speed_multiplier.z,
                          e->water_level, e->last_yd }) {
            fingerprint_add(h, std::bit_cast<int64_t>(v));
        }
        for (int64_t v : { (int64_t) e->id,
                           (int64_t) e->type,
                           (int64_t) e->removed,
                           e->random.seed,
                           (int64_t) e->on_ground,
                           (int64_t) e->remaining_fire_ticks,
                           (int64_t) e->item.item,
                           (int64_t) e->item.count,
                           (int64_t) e->age,
                           (int64_t) e->pickup_delay,
                           (int64_t) e->health,
                           (int64_t) e->block_state,
                           (int64_t) e->time,
                           (int64_t) e->drop_item,
                           (int64_t) e->cancel_drop,
                           (int64_t) e->hurt_entities,
                           (int64_t) std::bit_cast<int32_t>(e->y_rot),
                           (int64_t) e->flipped,
                           (int64_t) e->hurt_time,
                           (int64_t) std::bit_cast<int32_t>(e->damage),
                           (int64_t) e->enabled,
                           (int64_t) e->fuse,
                           (int64_t) e->status,
                           (int64_t) e->old_status,
                           (int64_t) std::bit_cast<int32_t>(e->land_friction),
                           (int64_t) e->is_above_bubble_column,
                           (int64_t) e->bubble_column_direction_is_down,
                           (int64_t) e->bubble_time }) {
            fingerprint_add(h, v);
        }
        for (const item_stack & s : e->items) {
            for (int64_t v : { s.item, s.count, s.damage, s.contents }) fingerprint_add(h, v);
        }
    }
    return h;
}

// runs a case untraced (best of 5) and traced; both must end in the same state
static bool bench_case(const std::string & path, int64_t & ticks, double & seconds) {
    case_file f;
    if (!load_case(path, f)) return false;
    uint64_t fingerprint = 0;
    double   best        = 1e30;
    for (int32_t i = 0; i < 5; i++) {
        level       lv;
        trace       tr;
        std::string err;
        tr.record  = false;
        auto start = std::chrono::steady_clock::now();
        bool ok    = case_run(f, lv, tr, err);
        best       = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
        if (!ok) {
            fprintf(stderr, "%s: %s\n", path.c_str(), err.c_str());
            return false;
        }
        fingerprint = level_fingerprint(lv);
        ticks       = lv.game_time;
    }
    level       lv;
    trace       tr;
    std::string err;
    auto        start  = std::chrono::steady_clock::now();
    bool        ok     = case_run(f, lv, tr, err);
    double      traced = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (!ok || level_fingerprint(lv) != fingerprint) {
        printf("%s: the traced run ends in a different state\n", path.c_str());
        return false;
    }
    printf("%-40s %7lld ticks %9.0f ticks/s (traced %8.0f) fingerprint %016llx\n", path.c_str(), (long long) ticks,
           ticks / best, ticks / traced, (unsigned long long) fingerprint);
    seconds = best;
    return true;
}

static int cmd_bench(const std::vector<std::string> & paths) {
    int64_t total_ticks   = 0;
    double  total_seconds = 0;
    for (const std::string & path : paths) {
        int64_t ticks;
        double  seconds;
        if (!bench_case(path, ticks, seconds)) return 1;
        total_ticks += ticks;
        total_seconds += seconds;
    }
    printf("total %lld ticks in %.3f s: %.0f ticks/s\n", (long long) total_ticks, total_seconds,
           total_ticks / total_seconds);
    return 0;
}

int main(int argc, char ** argv) {
    if (argc >= 3 && strcmp(argv[1], "run") == 0) return cmd_run(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "check") == 0) {
        bool                     xfail = strcmp(argv[2], "--xfail") == 0;
        std::vector<std::string> paths(argv + (xfail ? 3 : 2), argv + argc);
        return cmd_check(paths, xfail);
    }
    if (argc >= 3 && strcmp(argv[1], "bench") == 0) return cmd_bench(std::vector<std::string>(argv + 2, argv + argc));
    fprintf(stderr, "usage: tmcsim run <file.case>\n");
    fprintf(stderr, "       tmcsim check [--xfail] <file.case>...   (compares with <file.trace>)\n");
    fprintf(stderr,
            "       tmcsim bench <file.case>...             (ticks/s untraced; the traced run must end the same)\n");
    return 2;
}
