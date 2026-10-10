#pragma once

#include "core/trace.h"
#include "io/case-file.h"

#include <string>

// Port of the oracle's TraceBatch for one case: place it, trace "run" ticks, apply inputs between ticks.
// Returns false with err set if the case hits something the sim does not handle yet.
bool case_run(const case_file & f, trace & tr, std::string & err);
