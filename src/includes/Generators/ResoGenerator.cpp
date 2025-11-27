#include "ResoGenerator.h"

static constexpr int minMidiNote{21};
static constexpr int maxMidiNote{132};
static constexpr int range{maxMidiNote - minMidiNote};
static constexpr int stepsPerSemitone{66};

static constexpr size_t NumElements{range * (stepsPerSemitone) + 1};

template class ResoGenerator<16, NumElements>;
