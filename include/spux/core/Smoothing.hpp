// Parameter smoothing as policies (guidelines section 5). The plugin picks a policy per
// parameter: Smoothed<OnePole<0.02>> cutoff; Smoothed<LinearRamp<>> gain; Smoothed<NoSmoothing> mode.
// A shipped policy never changes its curve; a different curve is a new policy.
//
// Every policy advances one sample per next(), so the result does not depend on the block size (R3).
// Coefficients come from times (R4); call prepare(sampleRate) before use.
//
//   NoSmoothing      the target, at once.
//   LinearRamp<N>    a straight line from the current value to the target over N samples (one control
//                    tick, default 64); evaluated from the position like TickRamp, so with targets set at
//                    tick boundaries it equals TickRamp exactly.
//   OnePole<Tau>     y += a (target - y), a = 1 - e^(-1 / (Tau fs)), Tau in seconds (63 % after Tau);
//                    snaps to the target once a step no longer changes the value.
//   Slew<Rate>       moves toward the target by at most Rate units per second.
#pragma once

#include <concepts>

#include "spux/core/Math.hpp"

namespace spux::inline v1 {

// One-pole smoothing coefficient for time constant tau (seconds) at rate fs: 1 - e^(-1 / (tau fs)).
// 1 (no smoothing) for tau <= 0 or fs <= 0.
template <std::floating_point T = double> [[nodiscard]] constexpr T onePoleCoeff(double tauSeconds, double sampleRate) {
    if (!(tauSeconds > 0.0) || !(sampleRate > 0.0))
        return T(1);
    return static_cast<T>(1.0 - math::expD(-1.0 / (tauSeconds * sampleRate)));
}

// What a policy's State<T> provides.
template <class S, class T>
concept SmootherState = std::default_initializable<S> && requires(S s, const S cs, T v, double fs) {
    s.prepare(fs);
    s.reset(v);
    s.setTarget(v);
    { s.next() } -> std::same_as<T>;
    { cs.current() } -> std::same_as<T>; // the value next() returned last (or the reset value)
    { cs.target() } -> std::same_as<T>;
    { cs.settled() } -> std::same_as<bool>;
};

template <class P, class T>
concept SmoothingPolicy = SmootherState<typename P::template State<T>, T>;

struct NoSmoothing {
    template <std::floating_point T> struct State {
        T value = T(0);
        constexpr void prepare(double) {}
        constexpr void reset(T v) { value = v; }
        constexpr void setTarget(T v) { value = v; }
        constexpr T next() { return value; }
        constexpr T current() const { return value; }
        constexpr T target() const { return value; }
        constexpr bool settled() const { return true; }
    };
};

template <int N = 64>
    requires(N > 0)
struct LinearRamp {
    template <std::floating_point T> struct State {
        T start = T(0), end = T(0), last = T(0);
        int pos = N;
        constexpr void prepare(double) {}
        constexpr void reset(T v) {
            start = end = last = v;
            pos = N;
        }
        // Starts a new ramp from the value the next sample would have had.
        constexpr void setTarget(T v) {
            start = valueAt(pos);
            end = v;
            pos = 0;
        }
        constexpr T next() {
            last = valueAt(pos);
            if (pos < N)
                ++pos;
            return last;
        }
        constexpr T current() const { return last; }
        constexpr T target() const { return end; }
        constexpr bool settled() const { return pos >= N; }

    private:
        constexpr T valueAt(int p) const {
            return p >= N ? end : start + (end - start) * (static_cast<T>(p) * (T(1) / T(N)));
        }
    };
};

template <double TauSeconds>
    requires(TauSeconds > 0.0)
struct OnePole {
    template <std::floating_point T> struct State {
        T y = T(0), goal = T(0), a = T(1);
        constexpr void prepare(double sampleRate) { a = onePoleCoeff<T>(TauSeconds, sampleRate); }
        constexpr void reset(T v) { y = goal = v; }
        constexpr void setTarget(T v) { goal = v; }
        constexpr T next() {
            const T n = y + a * (goal - y);
            y = n == y ? goal : n; // no progress left: land exactly (no denormal tail)
            return y;
        }
        constexpr T current() const { return y; }
        constexpr T target() const { return goal; }
        constexpr bool settled() const { return y == goal; }
    };
};

template <double UnitsPerSecond>
    requires(UnitsPerSecond > 0.0)
struct Slew {
    template <std::floating_point T> struct State {
        T y = T(0), goal = T(0), step = T(0);
        constexpr void prepare(double sampleRate) {
            step = sampleRate > 0.0 ? static_cast<T>(UnitsPerSecond / sampleRate) : T(0);
        }
        constexpr void reset(T v) { y = goal = v; }
        constexpr void setTarget(T v) { goal = v; }
        constexpr T next() {
            const T d = goal - y;
            if (d > step)
                y += step;
            else if (d < -step)
                y -= step;
            else
                y = goal;
            return y;
        }
        constexpr T current() const { return y; }
        constexpr T target() const { return goal; }
        constexpr bool settled() const { return y == goal; }
    };
};

template <class Policy, std::floating_point T = float>
    requires SmoothingPolicy<Policy, T>
class Smoothed {
public:
    using State = typename Policy::template State<T>;

    constexpr void prepare(double sampleRate) { s_.prepare(sampleRate); }
    constexpr void reset(T v) { s_.reset(v); }
    constexpr void setTarget(T v) { s_.setTarget(v); }
    constexpr T next() { return s_.next(); }
    [[nodiscard]] constexpr T current() const { return s_.current(); }
    [[nodiscard]] constexpr T target() const { return s_.target(); }
    [[nodiscard]] constexpr bool settled() const { return s_.settled(); }

    // n values into out.
    constexpr void process(T* out, int n) {
        for (int i = 0; i < n; ++i)
            out[i] = s_.next();
    }
    // Advances n samples; returns the last value.
    constexpr T skip(int n) {
        for (int i = 0; i < n; ++i)
            s_.next();
        return s_.current();
    }

private:
    State s_{};
};

} // namespace spux::inline v1
