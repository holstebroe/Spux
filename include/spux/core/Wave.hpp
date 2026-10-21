// The common periodic shapes, named once for everything that has them: LFOs (spux/dsp/Lfo.hpp), the waveform
// icons of the UI (spux/ui/Icons.hpp), and the oscillators of a plugin that chooses among them.
//
//   Sine, Triangle  as usual (a triangle starts at 0 and rises).
//   Saw, SawDown    a ramp up (down) over the period, jumping back at its end.
//   Square          50 % duty; Pulse is 25 %.
//   Noise           a random line: a new value each period, joined smoothly (a smooth random LFO, the white
//                   scribble of the icon).
//   SampleHold      a new random value each period, held.
#pragma once

namespace spux::inline v1 {

enum class Wave { Sine, Triangle, Saw, SawDown, Square, Pulse, Noise, SampleHold };
inline constexpr int kWaveCount = 8;

[[nodiscard]] constexpr const char* waveName(Wave w) {
    switch (w) {
    case Wave::Sine:
        return "Sine";
    case Wave::Triangle:
        return "Triangle";
    case Wave::Saw:
        return "Saw";
    case Wave::SawDown:
        return "Saw down";
    case Wave::Square:
        return "Square";
    case Wave::Pulse:
        return "Pulse";
    case Wave::Noise:
        return "Noise";
    case Wave::SampleHold:
        return "Sample and hold";
    }
    return "";
}

} // namespace spux::inline v1
