# spux_bench results

`spux_bench` (bench/SpuxBench.cpp, Release only) times the spux math against libm and the DSP blocks.
R2 and S4: spux math replaces libm in the audio path only where it is at least as fast. Numbers are the
best of 20 runs over 65536 random inputs; they depend on the machine, so compare columns, not runs.

## 2026-10-20, migration step 4

Linux x86-64, GCC 13.3, glibc, CMake Release (`-O3 -DNDEBUG`), `-ffp-contract=off`, on a
cloud VM ("Intel Xeon Processor @ 2.10GHz", 4 cores). The Windows numbers (MSVC, UCRT) are in the
next section.

### Math (ns per call; speed-up = libm / spux)

| function                           |    spux |    libm |     x |
| --- | ---: | ---: | ---: |
| exp2F (std::exp2 float)            |    9.63 |    2.84 |  0.30 |
| fastTanhF (std::tanh float)        |   11.17 |   17.65 |  1.58 |
| tanhPade76 (std::tanh float)       |    4.85 |   17.52 |  3.61 |
| tanhPade32 (std::tanh float)       |    6.65 |   17.46 |  2.63 |
| sinTurns (std::sin float)          |   16.58 |   15.67 |  0.95 |
| tanRad (std::tan float)            |    5.85 |   15.67 |  2.68 |
| log2Approx (std::log2 float)       |    2.43 |    3.01 |  1.24 |
| atanApprox (std::atan float)       |    7.84 |   12.21 |  1.56 |
| logCosh (log(cosh) double)         |   13.91 |   17.58 |  1.26 |
| dbToGainF (std::pow float)         |   10.66 |    6.69 |  0.63 |
| expD (std::exp)                    |   38.55 |    5.36 |  0.14 |
| exp2D (std::exp2)                  |   36.46 |    3.10 |  0.09 |
| logD (std::log)                    |   50.82 |    4.85 |  0.10 |
| sinD (std::sin)                    |   18.90 |   21.07 |  1.11 |
| tanD (std::tan)                    |   36.33 |   16.75 |  0.46 |

### Blocks (ns per host-rate sample, float, 48 kHz)

| block                              |      ns |         |       |
| --- | ---: | ---: | ---: |
| OnePoleTpt lowpass                 |    3.79 |       - |     - |
| Svf tick                           |    8.28 |       - |     - |
| Biquad tick                        |    3.79 |       - |     - |
| DelayLine write + readHermite      |    7.12 |       - |     - |
| Oversampler x2 round trip          |   19.30 |       - |     - |
| Oversampler x4 round trip          |   53.92 |       - |     - |
| Oversampler x8 round trip          |  181.77 |       - |     - |
| Adaa1<TanhSaturator>               |   18.18 |       - |     - |
| TanhSaturator::value               |   12.19 |       - |     - |
| Oversampler x4 + Adaa1 tanh        |  133.34 |       - |     - |
| Blep<16>::residual                 |    6.88 |       - |     - |
| PinkNoise tick                     |    3.51 |       - |     - |
| Smoothed<OnePole> next             |    3.10 |       - |     - |


### Windows: MSVC (VS 2026, /O2 /fp:precise), UCRT, GitHub windows-latest runner, `spux_bench --quick`

| function | spux | libm | x |
| --- | ---: | ---: | ---: |
| exp2F (std::exp2 float) | 5.31 | 2.78 | 0.52 |
| fastTanhF (std::tanh float) | 7.43 | 13.29 | 1.79 |
| tanhPade76 (std::tanh float) | 5.66 | 13.25 | 2.34 |
| tanhPade32 (std::tanh float) | 7.13 | 13.24 | 1.86 |
| sinTurns (std::sin float) | 16.92 | 15.19 | 0.90 |
| tanRad (std::tan float) | 7.44 | 7.82 | 1.05 |
| log2Approx (std::log2 float) | 2.16 | 3.71 | 1.72 |
| atanApprox (std::atan float) | 8.35 | 11.26 | 1.35 |
| logCosh (log(cosh) double) | 13.79 | 25.91 | 1.88 |
| dbToGainF (std::pow float) | 5.38 | 12.86 | 2.39 |
| expD (std::exp) | 38.72 | 4.40 | 0.11 |
| exp2D (std::exp2) | 37.80 | 4.04 | 0.11 |
| logD (std::log) | 47.76 | 5.04 | 0.11 |
| sinD (std::sin) | 21.02 | 13.43 | 0.64 |
| tanD (std::tan) | 43.72 | 12.68 | 0.29 |

Blocks (ns per host-rate sample): OnePoleTpt 3.70, Svf 7.40, Biquad 3.70, DelayLine Hermite 7.25,
Oversampler x2 / x4 / x8 23.7 / 61.2 / 184.7, Adaa1 tanh 21.4, Oversampler x4 + Adaa1 tanh 159.0,
Blep<16> 7.22, PinkNoise 4.78, Smoothed<OnePole> 5.55. The MSVC build also passes the kernel test against
`tests/kernels/portable.txt`: the blocks give the same bits as GCC and Clang.

### Reading

- **Faster than glibc** in the audio path: `fastTanhF` (1.6x), the Pade tanh shapes (2.6 to 3.6x),
  `tanRad` (2.7x), `atanApprox` (1.6x), `log2Approx` (1.2x), `logCosh` (1.3x). Since 2026-10-20 (S4) they are
  opt-in: a plugin uses them where the gain matters (Acidus's ladder would gain 18 % of its engine with
  `fastTanhF`) and keeps libm elsewhere.
- **On par**: `sinTurns` (0.95 to 1.02 between runs) and `sinD` (1.1x).
- **Slower than glibc: `exp2F` (0.3x) and `dbToGainF` (0.6x against `std::pow`, it calls `exp2F`).** glibc's `exp2f` is
  a table plus a short double polynomial (about 3 ns); `exp2F` is Plumeflow's degree-5 Horner chain,
  bound by its latency. Speeding it up would change its results (a different polynomial or order of
  evaluation), so it is unchanged: a plugin keeps libm's `exp2f` (the S4 default). A port of glibc's
  `exp2f` (ARM optimized-routines, MIT; 32-entry double table and a cubic, 0.502 ulp) measured 2.6 to
  3.5 ns and 235 bytes of code plus a 256-byte table, bit-identical to glibc for every float (Acidus
  report, 2026-10-20), if spux ever wants a fast exp2 of its own.
  On Windows (the v1 platform) `exp2F` is also slower than the UCRT's `exp2f` (0.5x), but `dbToGainF`
  beats `std::pow` (2.4x).
- **The double set is slower than libm** (`expD` 7x, `exp2D` 12x, `logD` 10x, `tanD` 2x): series with
  fixed term counts from Lumeoid. They are meant for coefficients at control rate (once per tick or
  per parameter change), where a few tens of ns do not matter, and they make coefficients identical on
  every platform. Not for per-sample use.
- **Oversampler**: the 2x and 4x paths are unrolled (same operations in the same order as the general
  loop, so the output is unchanged): 19 ns and 54 ns per host sample and channel for the round trip;
  8x uses the general loop (182 ns). For comparison, Plumeflow's 4 + 4 design measured 14 ns (2x) and
  43 ns (4x) in the half-band study below.

## Half-band design study (2026-10-20)

Single-stage measurements of the float implementation (impulse response, 65536-point FFT), and the
round-trip cost with hand-unrolled stages:

| Design | Sections | Stopband (from 0.55 of the low rate) | Passband ripple (to 0.45) | Group delay at 0.05 / 0.2 / 0.4 of the low rate (high-rate samples) | 2x / 4x round trip |
| --- | --- | ---: | ---: | --- | --- |
| Plumeflow | 4 + 4 | -95.2 dB | < 1e-5 dB | 3.9 / 4.6 / 10.3 | 14 / 43 ns |
| Lumeoid (chosen) | 5 + 5 | -108.2 dB | < 1e-5 dB | 4.0 / 4.9 / 11.7 | 18 / 55 ns |

Same transition band, practically the same group delay; 5 + 5 costs about 25 to 30 % more and rejects
13 dB more. Chosen because -95 dB images of a full-scale signal are only 5 dB under the -90 dBFS null
threshold and above 16-bit resolution, while -108 dB clears both and comes within 2 dB of the strict
-110 dBFS gate. A few ns per sample and channel is small next to the nonlinear core that runs at the
oversampled rate (ADAA tanh alone is 18 ns per call).

Plumeflow's 4x path (`Oversampler<4>`, `BlockOversampler`) runs its second stage as two separate
filters on the even and odd samples of the 2x stream instead of one filter at the 2x rate. Measured
on a 1 kHz sine upsampled 4x (Blackman-Harris window): worst spurious component -36 dB, against
-86 dB (window-limited) for the cascade spux uses. spux does not copy that structure; Plumeflow's 4x
output changes when it migrates (a bug fix, step 8).
