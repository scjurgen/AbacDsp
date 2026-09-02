# Third-party licenses

AbacDsp's own code is MIT licensed (see `LICENSE`), copyright Jürgen
Schwietering. That MIT grant covers the header-only core library in
`src/includes/` outright. It does **not** by itself cover a built plugin from
`examples/`, because those additionally link JUCE - see below.

## Core library (`src/includes/`)

Depends on two permissive, non-copyleft submodules:

| Submodule | License | Full text |
|---|---|---|
| `AudioFile` | MIT | `3rdparty/AudioFile/LICENSE` |
| `pffft` | BSD-3-Clause-style (UCAR) | `3rdparty/pffft/LICENSE.txt` |

Both only require keeping their copyright notice and license text around in
anything you redistribute (source or binary); neither imposes any copyleft
obligation on code that uses them. Using `src/includes/` on its own is
therefore plain MIT in practice, plus that attribution requirement.

## Example plugins (`examples/`)

Additionally link JUCE and, for the six Lua-scripted examples (`dronesequencer`,
`resonik`, `pingsynth`, `spectraltap`, `tapelooper`, `morphexsynth`), Lua/sol2 and
(Authoring Mode's embedded HTTP server) cpp-httplib:

| Dependency | License | Full text |
|---|---|---|
| `JUCE` (submodule) | AGPLv3 or JUCE commercial licence (dual) | `3rdparty/JUCE/LICENSE.md` |
| `lua` (submodule) | MIT | license text embedded in `3rdparty/lua/lua.h` |
| `sol2` (submodule) | MIT | `3rdparty/sol2/LICENSE.txt` |
| `cpp-httplib` (CMake FetchContent) | MIT | https://github.com/yhirose/cpp-httplib/blob/master/LICENSE |

`googletest` (BSD-3-Clause, `3rdparty/googletest/LICENSE`) is test-only and
never linked into a distributed plugin.

**JUCE is the important one.** This repo does not configure a JUCE
commercial license (no license key, no `JUCE_DISPLAY_SPLASH_SCREEN`
override), so every example plugin under `examples/` is built under JUCE's
free tier, which requires the AGPLv3. That means:

- A built example plugin is an AGPLv3-licensed combined work, not MIT,
  regardless of the root `LICENSE` file's blanket wording.
- AGPLv3's source-availability requirement is satisfied by this being a
  public repository with the complete source of every example - but anyone
  redistributing a *built binary* of an example must also make the complete
  corresponding source available under AGPLv3-compatible terms, and the
  combined work is bound by AGPLv3 as a whole (its copyleft is not confined
  to the JUCE portion).
- Only using `src/includes/` avoids all of this - the AGPLv3 obligation
  exists solely because `examples/` links JUCE.

To distribute an example plugin under different terms, a commercial JUCE
license would need to be obtained and configured in the corresponding
`examples/<name>/CMakeLists.txt`.
