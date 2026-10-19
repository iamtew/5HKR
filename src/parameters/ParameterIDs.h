#pragma once

#include "../dsp/Sequencer.h"

namespace ParamID
{
constexpr const char* volume     = "volume";
constexpr const char* seqTempo   = "seqTempo";
constexpr const char* seqBank    = "seqBank";
constexpr const char* seqPattern = "seqPattern";

constexpr const char* scOn     = "scOn";
constexpr const char* scThresh = "scThresh";
constexpr const char* scAmount = "scAmount";
constexpr const char* scAttack = "scAttack";
constexpr const char* scRelease = "scRelease";
constexpr const char* scMix    = "scMix";

inline constexpr const char* laneOn[hkr::numLanes] = {
    "laneOn0", "laneOn1", "laneOn2", "laneOn3",
    "laneOn4", "laneOn5", "laneOn6", "laneOn7"
};
inline constexpr const char* laneSwing[hkr::numLanes] = {
    "laneSwing0", "laneSwing1", "laneSwing2", "laneSwing3",
    "laneSwing4", "laneSwing5", "laneSwing6", "laneSwing7"
};
inline constexpr const char* laneTune[hkr::numLanes] = {
    "laneTune0", "laneTune1", "laneTune2", "laneTune3",
    "laneTune4", "laneTune5", "laneTune6", "laneTune7"
};
}
