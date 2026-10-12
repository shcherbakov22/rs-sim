#pragma once

#include "core/trace.h"
#include "engine/level.h"
#include "io/case-file.h"

#include <string>

// Snapshot.restore into a fresh level, with the trace box and the loaded chunks of the oracle's batch.
void case_load(const case_file & f, level & lv, trace & tr);

// Port of the oracle's TraceBatch for one case: place it in a fresh level, trace "run" ticks, apply inputs between ticks.
// Returns false with err set if the case hits something the sim does not handle yet; tr then holds the trace up to that point.
bool case_run(const case_file & f, level & lv, trace & tr, std::string & err);
