# src/includes TODO

Findings noticed while documenting the headers. Not analysed, not fixed.

## Cross-cutting

### Non-ASCII characters in headers

CLAUDE.md requires ASCII-only source. The documentation pass rewrote the
comments carrying most of these and replaced the characters on the way through:
`Numbers/EasyingFunctions.h` (superscripts), `Modulation/Flutter.h` (pi and
superscripts), `Analysis/FftMisc.h` (non-breaking hyphens),
`Analysis/Spectrogram.h` and `Analysis/ZeroCrossings.h` (em dashes).

One file is left, deliberately:

`Numbers/Approximation.h` still carries pi glyphs and plus-minus signs
throughout its header block. It mirrors generator output from
`documentation/Numbers/generateApproximations.py`, so editing the header would
be undone by the next regeneration; the fix belongs in the generator. For the
same reason it is the one header assigned to its Doxygen group from
`docs/groups.dox` rather than by an `@ingroup` line in the file.

Worth adding as a sibling to `dev-scripts/dev-check-urls.sh`:
`LC_ALL=C grep -rn '[^\x00-\x7F]' src/includes`.

### SincFilter::getBufferSize does integer division before the float cast

`static_cast<float>(m_coeffs.size() / m_increment)` divides two `size_t` values
and only then converts, truncating the ratio before it is scaled by `maxRatio`.
clang-tidy flags it as `bugprone-integer-division`. The buffer comes out
undersized whenever the kernel length is not a whole multiple of `increment`.

## Delays/FracReadHead.h

### The quartic easing path undershoots its target by 20 percent

`setNewDelta()` sizes the ramp as `totalSteps = ceil(|1.5 * deltaDifference / c|)`
where `c = maxAdvance - 1`. That 1.5 is `1 / (2/3)`, the reciprocal of the mean
of the quadratic easing bump:

- `smoothStep2(x, c) = 1 - 4c*x^2 + 4c*x`, mean over [0, 1] is `1 + 2c/3`.
- `smoothStep4(x, c) = 1 + 16c*x^2*(x-1)^2`, mean over [0, 1] is `1 + 8c/15`.

So with `quartic = true` the head travels `(8/15) / (2/3) = 0.8` of the
requested distance and stops there. The correct constant for that curve is
`15/8 = 1.875`. Nothing closes the loop afterwards, so the error persists until
the next `setNewDelta()` call, which then ramps the remainder.

## Reverbs/HadamardWalsh*.h

### The Walsh files are not Walsh-ordered

`hadamardWalshN()` and `hadamardFeedN()` produce byte-identical output at orders
4, 8, 16 and 32. Verified by feeding basis vectors through both and comparing
the resulting matrices column by column (`explore/explore.cpp`).

The butterfly in these files is the natural-order fast Walsh-Hadamard transform,
which yields Sylvester row order, not sequency order. The names suggest a
sequency-ordered variant that would be a row permutation of the other; there is
no such difference.

So the real distinction is cost only: order 32 takes 160 adds through the
butterfly against 1024 written flat. That makes the flat `HadamardN.h` files
redundant except as a reference implementation, which is worth saying out loud
or acting on.

## Reverbs/ naming

### File name and class name disagree in two places

- `FdnTankBlockDelayWalshSIMD.h` declares `FdnTankBlockDelaySIMDWalsh`.
- `Filters/BiquadResoBPParallelSIMD.h` declares `BiquadResoBpParallelSIMD`.

## Delays/ParallelPlainDelay.h

### m_currentDelayWidth only initialises its first element

```cpp
alignas(16) std::array<int, CHANNELS> m_currentDelayWidth{MAXSIZE / 8};
```

Brace-initialising a `std::array` with one value sets element 0 and
value-initialises the rest, so channel 0 starts at `MAXSIZE / 8` and every other
channel starts at 0. Any channel whose `setSize()` is not called before the
first `processBlock()` runs at zero delay while channel 0 does not. If a uniform
default is wanted, the array needs `fill()` in the constructor.

## Delays/ModulationDelay.h

### Still modulates with a triangle wave

`step()` builds its depth from `m_modWidth * (std::abs(m_currentPhase) + 1) + 1`
with the phase sweeping -1 to 1, which is a triangle. A triangular delay
modulation produces a square-wave pitch deviation, the flip-flop artifact that
motivated replacing triangle modulation with `SineModulation` elsewhere in the
tree. Worth deciding whether this line should follow.

## Delays/FracReadHead.h (continued)

### Two dead members

`m_sampleRate` is stored by the constructor and never read. `m_targetDelta` is
assigned in `setNewDelta()` and never read. `m_reducingDelta` is only used on
the line after it is assigned and could be a local.

## Filters/OnePoleFilter.h

### OnePoleFilterStereo leaves outputs unwritten for HighPassLeaky

`stepStereo()` branches on AllPass, LowPass and HighPass, with no branch and no
`else` for `OnePoleFilterCharacteristic::HighPassLeaky`. The in-place
`processBlock` overload therefore passes audio through unchanged, but the
out-of-place overload never assigns `outLeft` / `outRight` and the caller keeps
whatever was already in its buffer.

`MultiChannelOnePoleFilter::step()` has the same missing branch. It is in-place
only, so it degrades to pass-through rather than to stale data.

Open question: should the two multichannel variants implement HighPassLeaky, or
should the combination be rejected with a `static_assert`?

### processBlock early-out assumes pole 0 means pass-through

Both `processBlock` overloads return early (or `copy_n`) when the coefficient is
0. That is only the identity for LowPass:

| Characteristic | Output at p = 0 | Early-out yields |
| --- | --- | --- |
| LowPass | `y = x` | `y = x`, correct |
| HighPassLeaky | `y = x - x = 0` | `y = x` |
| AllPass | one-sample delay | `y = x` |
| HighPass | `y = 0.5 * (x - x[n-1])` | `y = x` |

`setCutoff()` at or above Nyquist sets the coefficient to 0, so this state is
reachable through the normal API, not only through `setFeedback(0)`.

## Filters/Biquad.h

### dB-to-linear conversion is missing its /20 divisor

`biquadMagnitude()` and `BiquadCoefficients::magnitude()` both return
`pow(10, magnitudeInDb(cf))`. A dB value converts to an amplitude ratio as
`10^(dB/20)`, so these return `10^dB` and are off by an exponent of 20. Any
non-zero response turns into a wildly wrong number: +6 dB reads as 1e6 instead
of 2.0.

`biquadMagnitudeLinear()` computes the ratio correctly and independently, which
is presumably why the error has gone unnoticed.

### BiquadStereo::reset() is private

`Biquad::reset()` is public; the stereo twin declares the same method in the
private section, so a `BiquadStereo` cannot have its state cleared from outside.
Either it should be public or it should be removed.

### ChebyshevBiquad has dead members

- `m_isType1` is assigned in `computeType1()` and `computeType2()` and never
  read anywhere. `processBlock()` always runs `stepChebyType2()`.
- `stepChebyType1()` has no caller in `src/` or `test/`. It is public, so it may
  be intended as API, but nothing exercises it.
- `m_biquadSinglePole` is declared and never touched.
- `m_biquads` is only written by `assignToBiquads()` and read by
  `getMagnitudeInDb()`; the audio path uses the raw `coefficients` array
  instead, so the whole biquad array exists only for response plotting.

### computeType2 odd-order section looks inconsistent with computeType1

Two lines in the `order & 1` branch of `computeType2()` differ from the
equivalent code in `computeType1()` and from the even-order branch just above
them:

- `coefficients[m_elements - 1].a1 = -fZZeroReal;` takes the denominator
  coefficient from the *zero*. `computeType1()` uses `-fZPole.real()` there.
- the highpass pole transform divides by
  `std::complex<float>(1 - beta * fZPole.real(), -fZPole.imag())`, where every
  other instance of that transform uses `-beta * fZPole.imag()` for the
  imaginary part.

Both may be deliberate, but they read as transcription slips. Worth checking an
odd-order Type 2 highpass response against a reference.

## Filters/BiquadReference.h

### Throws std::invalid_argument without including stdexcept

`calculateCoefficients()` throws `std::invalid_argument` in its `default` case.
The file includes only `Filters/Biquad.h`, and neither that header nor anything
it pulls in includes `<stdexcept>`. It compiles today because libc++ reaches the
declaration transitively through `<iostream>`, which is not guaranteed by the
standard and can break on another toolchain.

## Filters/BiquadResoBP.h

### isActive() never counts the decay down

`isActive()` returns true unconditionally while `m_decayCount > 0`, but nothing
in the class ever decrements it. `triggered()` sets it to `m_decayMax` and it
stays there, so a triggered resonator reports active forever and the
silence-detection branch below becomes unreachable.

`SvfResoBP::isActive()` is the same function with `m_decayCount--` present
(SvfResoBP.h:217), so this reads as an omission rather than a design choice.

### setDecay() mixes milliseconds and seconds

`setByDecay()` takes `t` in seconds and uses it consistently for both
`m_decayMax` and the Q formula.

`setDecay()` treats `t` as milliseconds for `m_decayMax`
(`m_sampleRate * t * 0.001f`) but then feeds the same unscaled value into
`Q = pi * f * t * k`, which expects seconds. The resulting Q is 1000x too large.

### setDecay() silently reuses the previous frequency

`setDecay()` reads the `K` and `kSquare` members cached by the last
`computeCoefficients()` call rather than deriving them from `m_frequency`. The
new coefficients therefore belong to whatever frequency was set last, and the
dependency is invisible at the call site. Documented as-is for now.

### magnitude() has a sample rate default that can disagree with the object

`magnitude(index, hz, sampleRate = 48000.f)` evaluates against the argument
while the object carries its own `m_sampleRate`. A caller who omits the argument
at any other rate gets the wrong curve with no diagnostic.

The b1 = 0 and b2 = -b0 substitutions are also left unsimplified in the
expression: `std::pow((b0 + 0 + -b0), 2)` is identically zero.

### m_sampleRate is a public data member

Declared public, mid-class, with the `m_` prefix, next to a `setSampleRate()`
that does the same job.

## Filters/BiquadResoBandPassParallel.h

### Carries the same defects as BiquadResoBP, minus the unit bug

Verified present here as well:

- `magnitude()` returns dB despite the name, evaluates against a `sampleRate`
  argument defaulting to 48000 rather than the object's own, and leaves the
  `b1 = 0` / `b2 = -b0` substitutions unsimplified, including the identically
  zero `std::pow((b0 + 0 + -b0), 2)`.
- `m_sampleRate` is a public data member sitting next to its own setter.
- `setDecay()` reuses the `K` / `kSquare` cached by the last
  `computeCoefficients()` on that `mainIndex`.

Not present here: the millisecond/second mix-up, since this class has no
`m_decayMax` and treats `t` as seconds throughout.

### isActive() silence threshold disagrees with the sibling

This class compares against `1E-5f`; `BiquadResoBP::isActive()` uses `1E-6f`.
The 32-call hold-off is the same in both. No obvious reason for the difference.

## Filters/BiquadResoBPParallelSIMD.h

### isActive() reads state the SIMD paths never write

`process()` advances `m_z0` / `m_z1` (the lane-major state) in both the
`USE_SIMD_FRAMEWORK` and `USE_X86_INTRINSICS` paths. The per-element `m_z` array
is only written by `reset()` and by the scalar `#else` fallback.

`isActive()` and the `m_z` half of `reset()` therefore read stale data on every
platform that actually takes a SIMD path, which is all of them in practice. A
ringing element reports itself inactive after 32 calls because `m_z` is still
zero from construction.

Either `process()` should mirror the lane state back, or `isActive()` should read
`m_z0` / `m_z1` and index by group and lane the way `reset()` already does.

### damp() is global here, per-element in the sibling

`BiquadResoBpParallelSIMD::damp(bool)` switches a single `m_currentSet` for the
whole bank. `BiquadResoBandPassParallel::damp(size_t, bool)` switches one
element. The banks are otherwise presented as interchangeable.

### magnitude() and setDecay() carry the same issues as the other two banks

Same dB-despite-the-name return, same misleading `sampleRate = 48000.f`
default, same unsimplified `(b0 + 0 + -b0)`, same cached-`K` dependency in
`setDecay()`. `m_sampleRate` is correctly private here, unlike in the other two.

### Class name and file name disagree

File is `BiquadResoBPParallelSIMD.h`, class is `BiquadResoBpParallelSIMD`.

## Filters/SvfResoBP.h

### computeCoefficients() discards its Q argument

```cpp
const float k = 1.f / std::max(Q, 0.01f);
m_cf[index].k = k;                      // set from the Q argument
recomputeCoefficientsWithBend(index);   // immediately overwrites k from m_decayT
```

`recomputeCoefficientsWithBend()` recomputes `Q = pi * bendFrequency * m_decayT * decayConst`
and assigns `m_cf[index].k` again, so the caller's `Q` never reaches the filter.

This bites at construction: the constructor calls `computeCoefficients(0, 1000.f)`
and `computeCoefficients(1, 1000.f)` while `m_decayT` is still 0, giving `Q = 0`,
clamped to 0.01, so `k = 100`. A freshly constructed `SvfResoBP` is heavily
damped rather than sitting at the documented default of `Q = 1/sqrt(2)`.

### m_decayT changes unit depending on which setter was used

`setByDecay()` stores `m_decayT = t` with `t` in seconds
(`m_decayMax = m_sampleRate * t`).

`setDecay()` stores `m_decayT = t` with `t` in milliseconds
(`m_decayMax = m_sampleRate * t * 0.001f`) but still feeds it to the seconds-based
`Q = pi * f * t * k` formula, and leaves it in `m_decayT` for
`recomputeCoefficientsWithBend()` to reuse later. Same millisecond/second
confusion as `BiquadResoBP::setDecay()`.

### pitchBendCents() only updates the active coefficient set

`recomputeCoefficientsWithBend(m_currentSet)` leaves the other set at the old
pitch, so a later `damp()` switch jumps back to the unbent frequency.

### ResonanceCompensation interpolates linearly across a geometric axis

The column index comes from `log2(time) + 10`, but `col_frac` is
`(time - m_times[col]) / (m_times[col + 1] - m_times[col])`, a linear fraction
between two points that are a factor of two apart. The comment above it says
"Bilinear interpolation in log-space", which is true of the row axis and of the
stored values, but not of this fraction.

## Filters/PoleMixingFilter.h

### Dead reference URL in the source

The file header and `FourStageFilterTheoretical` both cited
`https://expeditionelectronics.com/Diy/Polemixing/math`, which now returns 404.
The parent page `https://expeditionelectronics.com/Diy/Polemixing` is live and
was substituted. The Wayback capture of the original is at
`https://web.archive.org/web/2023/https://expeditionelectronics.com/Diy/Polemixing/math`
if the derivation itself is wanted.

### warpCutoffForSampleRate only covers five sample rates

Cubic fits exist for 44100, 48000, 96000, 192000 and 384000. Every other rate,
88200 and 176400 included, falls through and returns the cutoff uncorrected, so
the requested and realised cutoffs silently diverge there.

### Duplicate aliases

`Notch12Smooth` and `Phaser12Smooth` are both
`FixedFourStageFilter<1, -2, 2, 0, 0>`, so they name the same type and cannot be
overloaded apart. `Bp24Smooth` is `<0, 0, 4, -8, 4>` where the `BP4` preset in
`poleMixingList` is `{0, 0, 2, -4, 2}`: the same shape at twice the gain, which
may or may not be deliberate.

### poleMixingList could be constexpr

It is an `inline const std::vector`, so it allocates during static
initialisation and its contents are not usable in constant expressions. The
project style prefers `std::to_array` for tables of this kind, which would make
it constexpr and allocation-free.

### Three near-duplicate resonator banks

`BiquadResoBP`, `BiquadResoBandPassParallel` and `BiquadResoBPParallelSIMD`
repeat the same coefficient design, the same two-set `damp()` switch and the
same `magnitude()` body, diverging in small ways that look accidental rather
than intended. Worth deciding whether one parameterised implementation can
replace them.
