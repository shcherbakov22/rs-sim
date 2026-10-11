#pragma once

#include "core/trace.h"
#include "engine/level.h"
#include "io/case-file.h"

#include <string>

// Port of the oracle's TraceBatch for one case: place it in a fresh level, trace "run" ticks, apply inputs between ticks.
// Returns false with err set if the case hits something the sim does not handle yet; tr then holds the trace up to that point.
bool case_run(const case_file & f, level & lv, trace & tr, std::string & err);
