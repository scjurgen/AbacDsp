# Documentation style for src/includes

What the briefs in the core library are for, and how to keep new ones
consistent with the existing 114 files.

## The one rule that matters

A class comment documents **the class's own contract and the reasoning behind
it**. Not who calls it, not what it is used for, not where its data comes from.
Callers change; the class does not.

Use cases belong in separate documents, with example code where that helps.
They are deliberately absent from the headers.

## What earns a place in a brief

Ask whether a competent reader would already know it from the declaration. If
yes, leave it out.

Worth writing:

- **What it actually is, named precisely.** "One-pole (first-order IIR)
  lowpass", not "a filter". A reader who knows the term can stop reading; one
  who does not now has something to look up.
- **The numeric contract.** Order, slope, state size, latency, allocation
  behaviour, thread constraints, valid ranges.
- **Why a specific choice was made,** where the alternative is not obviously
  worse. Why the `exp()` coefficient mapping over the cheaper Taylor term. Why
  an integer accumulator rather than a float. Why sine modulation rather than
  triangle.
- **Invariants and traps.** Padding the caller must provide, an ordering
  dependency between two methods, a state the object can reach where a method
  stops meaning what its name says.
- **A theory reference,** where a canonical one exists.

Not worth writing:

- Restating the class name as a sentence.
- Describing what the code does step by step.
- Naming a caller, a plugin, or a feature the class happens to serve.
- Project history: what this replaced, what it was extracted from, what is
  planned. The status quo is what matters.

## Shape

```cpp
/**
 * @ingroup filters
 * @brief One line: what this is, named precisely.
 *
 * Why it exists, what it trades off, what is non-obvious about it.
 * Numeric contract and invariants.
 * @see https://ccrma.stanford.edu/~jos/filters/One_Pole.html
 */
```

Short is good but is not the goal; completeness is. A thin wrapper gets one
line and delegates its theory to whatever it wraps:

```cpp
/**
 * @ingroup blockprocessors
 * @brief Block-wise one-pole lowpass. Filter theory: see Filters/OnePoleFilter.h.
 */
```

A class with genuinely subtle geometry gets as much room as it needs;
`Delays/PitchFadeWindowDelay.h` runs to ninety lines and every one of them
earns its place.

Enumerators take `///<` trailing briefs where the distinction between values is
the useful information:

```cpp
enum class OnePoleFilterCharacteristic
{
    LowPass,
    HighPass,   ///< a0 = (1 + p)/2 normalises the response to exactly unity gain at Nyquist.
    AllPass,    ///< Unity magnitude at every frequency; only the phase is shaped.
    HighPassLeaky ///< y[n] = x[n] - lowpass(x[n]). One state less, but Nyquist gain 2p/(1 + p) stays under unity.
};
```

Free functions and methods take `///` briefs, generally two or three lines.
Use `@file` on headers that hold free functions or tables rather than a class.

## Groups

Every documented entity carries `@ingroup <id>`, one id per directory under
`src/includes/`. The ids and their overview pages are in `docs/groups.dox`;
each page states the design decision that domain turns on, so a reader gets the
shared context once instead of once per class.

## References

Check a URL resolves **before** citing it:

```bash
./dev-scripts/dev-check-urls.sh https://example.org/candidate
```

Prefer sources that have already outlived several redesigns: Julius Smith's
CCRMA books, the W3C-hosted Audio EQ Cookbook, the DAFx paper archive,
Wikipedia for standard algorithms. For paywalled AES papers, put author, title,
journal and year in the text so the citation survives a dead link.

Add each new reference to `WEB-REFERENCES.md` under its topic.

## Mechanics

- ASCII only. `pi`, `sqrt(2)`, `+/-`, `-3 dB`. No em dashes anywhere.
- Escape `#` in prose as `` `#error` ``, or Doxygen reads it as a link request.
- Do not hand-edit `Numbers/Approximation.h`; it is a copy of generator output
  from `documentation/Numbers/generateApproximations.py`.
- The doc build treats warnings as errors. Run `./dev-scripts/dev-docs.sh`
  before committing documentation changes.
