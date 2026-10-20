# spux changelog

Sound-relevant changes: every change of a kernel reference (tests/kernels/) gets a line here, in the same
commit as the regenerated files (guidelines section 3). Newest first.

## Unreleased

- 2026-10-20, migration step 5: `spux_params` brings parameter laws and host mapping (count as sound, S1):
  Plumeflow's Linear, Log, LogZero and Db, bit-identical to its `plugin/Params` over its whole table. No kernel
  reference changed.
- 2026-10-20, migration step 4: `spux_core` (Math, Pcg32, ControlTick, Smoothing, DenormalGuard) and
  `spux_dsp` (OnePoleTpt, Svf, Biquad, DcBlock, DelayLine, HalfbandStage and Oversampler, Blep 2/4/16/32,
  pt2 MinBlep, saturators with Adaa1 and the Newton and fixed-point solvers, white and pink noise, OU
  drift). First kernel references recorded in tests/kernels/ (portable.txt; GCC 13 and Clang 18 on x64
  agree). Every merged function is bit-identical to its Lumeoid, Plumeflow or Paulascape original
  except the OU setup (expD instead of std::exp, 1e-11 cents); see DECISIONS history 2026-10-20 for the
  differences a plugin meets when it switches.
