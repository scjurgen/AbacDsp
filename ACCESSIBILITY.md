# Plan: accessibility migration for the JUCE example plugins' UI

Status: analysis/planning only, per user request. No code has been changed.
Scope note: Windows/Narrator is explicitly out of scope for now (no Windows build
currently maintained - AU/VST3/AUv3/Standalone are built and CI'd on macOS only). Windows
support is called out as future work wherever it would otherwise appear, not silently
dropped.

## 1. Executive summary

All 18 example plugins (`examples/*`) share essentially 100% of their UI code. Each
blueprint (`JuceStandaloneGenerator/blueprints/*.json`) only supplies parameter lists, a
visual layout grid, and DSP glue; the actual `Editor`, `Processor`, `LookAndFeel`, and every
custom-painted widget class come from `JuceStandaloneGenerator/templates/sourcefiles/`,
assembled by `codegen_processor.py`/`codegen_widgets.py` and regenerated per blueprint.
Confirmed by grep: no example under `examples/*/src` has a single hand-written
`mouseDown`/`mouseDrag`/`hitTest`/`keyPressed` override outside this shared template set -
every custom interaction pattern in the project lives in about a dozen files under
`JuceStandaloneGenerator/templates/sourcefiles/inc/` and two Python modules. This is good
news for the migration: fixing accessibility in the generator's shared components and
regenerating propagates the fix to all 18 plugins uniformly, instead of 18 separate efforts.

Also confirmed by grep: zero calls to `setTitle()`, `setAccessible()`, `setDescription()`,
or `createAccessibilityHandler()` exist anywhere in the Python codegen today. A prior,
already-landed (but unverified-for-accessibility) piece of infrastructure is directly
relevant: `.claude/tooltips-for-components.md` rolled out `.setTooltip()` calls to every
generated widget kind. Tooltip text is not automatically the same thing as an accessible
name/description in JUCE - this needs an empirical check (Phase 1 below) before assuming
it already closes part of the gap.

The core, load-bearing findings are:
- Every "dial" (the majority of controls in every plugin) exposes no accessible name to
  assistive tech - its visual label is a disconnected sibling component.
- The MIDI-Learn / CC-range / dial-enable context menu is reachable only by right-click;
  there is no keyboard or accessibility-action equivalent, and no visible affordance
  advertises it exists at all.
- Save/load/delete/rename/script-apply/MIDI-learn outcomes are announced only via a custom
  `StatusBar` that repaints text with no live-region notification - screen reader users get
  no feedback that an action succeeded, failed, or produced an error.
- Level/CPU meters convey safety-relevant state (clipping risk, CPU headroom) purely through
  custom-painted bar fills, with no accessible value exposed at all.
- Keyboard Tab order follows each widget's declaration order in the blueprint JSON, which is
  verified (via `minireverb.json`) to diverge substantially from the visual layout grid -
  Tab/Shift+Tab traversal will not match what's on screen, project-wide.

None of this requires DSP, parameter-model, or serialized-state changes. The fixes are
concentrated in ~10 shared component headers and 2 codegen modules, then a mechanical
regenerate-and-rebaseline pass across all blueprints (the same workflow already used and
documented for the tooltip rollout).

## 2. Current architecture and accessibility inventory

### 2.1 Build/format surface

- Plugin formats (from `JuceStandaloneGenerator/templates/sourcefiles/CMakeListsExamples.txt`):
  `AU`, `VST3`, `AUv3`, `Standalone`. No AAX, no CLAP.
  - AU/AUv3 route through JUCE's Cocoa `NSAccessibility` bridge on macOS.
  - VST3's accessibility exposure depends on the host's own editor-hosting window (JUCE's
    editor is a normal `NSView`/window either way; VoiceOver generally reaches it, but this
    varies per host and needs per-host verification, not assumption).
  - Standalone is the most reliable target for the actual accessibility testing pass, since
    it is a plain native top-level window with no host-imposed view wrapping.
- No Windows build is currently maintained (per user); Narrator/Accessibility Insights work
  is deferred to when Windows support exists, not attempted now.
- `dev-scripts/dev-test-full.sh <target>` builds/tests `BUILD_FULL_PROJECT` targets
  (JUCE examples) in `cmake-build-debug/`; this is the vehicle for building a canary
  blueprint's Standalone/AU/VST3 targets during this work.

### 2.2 Parameter model

- Every blueprint's DSP-facing parameters go through a single
  `juce::AudioProcessorValueTreeState` (`m_parameters`, constructed in
  `{Module}Processor.h`), built by `createParameterLayout()` from the blueprint's
  `ports-control` list. Three parameter-backed control types exist:
  - `dial` -> `juce::AudioParameterFloat`, always constructed with
    `AudioParameterFloatAttributes{}.withLabel(unit).withStringFromValueFunction(...)` - so
    `getText()`/value-to-text quality is already good project-wide (formatted value + unit,
    e.g. "12.3 dB"). This is a verified-OK baseline, not a finding.
  - `switch` -> `juce::AudioParameterBool`.
  - `drop` -> `juce::AudioParameterChoice`.
  - `gauge`/`label` types are display-only, not parameter-backed.
- Bindings are all standard APVTS attachments: `SliderAttachment` (dial),
  `ButtonAttachment` (switch), `ComboBoxAttachment` (drop) - all built and torn down by the
  generator in `create_init_widgets()` (`codegen_widgets.py`). This is a sound,
  host-automation-correct architecture; no alternate binding scheme is in play.
- A separate raw-MIDI-CC layer (`impl/CcMapping.h`, `impl/CcSettings.h`,
  `AudioPluginAudioProcessor::handleMidiCc`) maps incoming CC 0-127 directly onto a
  parameter's `[valueLow, valueHigh]` sub-range, independent of host automation. This is
  the "MIDI Learn" feature whose only UI entry point is the dial's right-click menu.
- Patch/loop/script persistence goes through `FileIo` (`impl/FileIo.h`), independent of the
  host's own program/preset mechanism (`getStateInformation`/`setStateInformation` still
  round-trips full APVTS state for host-side save, so host presets and this app's own
  "Patches" menu are two coexisting, correctly separated concerns - verified-OK, not a
  finding).
- No `juce::UndoManager` exists anywhere in the codebase (confirmed by grep) - there is no
  undo/redo feature to make accessible; flagged in Open Questions rather than assumed absent
  by oversight.

### 2.3 Editor/component hierarchy

`{Module}Editor.h` (`AudioPluginAudioProcessorEditor`) is a `juce::AudioProcessorEditor` +
`juce::Timer` + `juce::MenuBarModel` + `juce::ComponentListener`. Fixed chrome:
`juce::MenuBarComponent` (top) with "Theme" / optional "Patches" / optional "Loops" /
optional "Scripts" / "About" top-level menus, and a custom `StatusBar` (bottom). The
blueprint-specific controls fill the middle via `/*INIT_WIDGETS*/` /
`/*RESIZED_AREA*/` codegen splices, laid out per the blueprint's `"layout"` /
`"performance-page"` composition grid (parsed by `parseboxstructure.py`). An optional
Performance/Settings page-switch (`/*START_PERFORMANCEPAGE*/`) toggles widget visibility
between two composition grids.

### 2.4 Interactive component inventory

| Component/class | Source file(s) | Purpose | Current interaction | Connected parameter/state | Stock or custom | Accessibility status/risk |
|---|---|---|---|---|---|---|
| `CustomRotaryDial` (outer wrapper) | `inc/CustomRotaryDial.h` | Continuous parameter knob, the majority control type in every plugin | Composes a slider + a sibling label; no interaction of its own | `dial` params (`AudioParameterFloat`) via `SliderAttachment` | Custom composite around a stock `Slider` | **Critical** - see Finding 1 |
| `ModRotaryDial : juce::Slider` | same | The actual focusable/draggable/typeable control | Drag, scroll, Shift-drag (JUCE default), text-box numeric entry, right-click context menu | same | Custom `Slider` subclass | **Critical** - see Findings 1, 2, 3 |
| `MomentaryToggleButton` / plain `juce::ToggleButton` | `inc/MomentaryToggleButton.h`, codegen | Discrete on/off (`switch` params) | Click, Space/Enter (stock) | `AudioParameterBool` via `ButtonAttachment` | Stock base, thin custom subclass | Low risk - good baseline, verify only |
| `juce::ComboBox` | generated in `{Module}Editor.h` | Discrete choice (`drop` params) | Click, arrow keys + type-ahead (stock) | `AudioParameterChoice` via `ComboBoxAttachment` | Stock | Medium - no explicit accessible name beyond default, see Finding 7 |
| `juce::Label` (decorative/`label` ports) | generated | Static text/section headers | None expected | n/a | Stock | Low risk |
| `Gauge`/`GaugeValue`/`GaugeIndicators` (VU meter, `gaugetype: levels`) | `inc/GenericMeter.h` | Live input/output level display | Read-only, mouse-transparent subcomponents | `processorRef.getInputDbLoad()/getOutputDbLoad()` | Custom | **High** - see Finding 4 |
| `CpuGauge`/`CpuValue` | `inc/CpuMeter.h` | CPU load % | Read-only | `processorRef.getCpuLoad()` | Custom | **High** - see Finding 4 |
| `WaveformGauge`/`WaveformShow` | `inc/WaveformMeter.h` | Waveform amplitude display | Read-only, mouse-transparent | `processorRef.getWaveDataToShow()` | Custom | Medium - see Finding 4 |
| `SpectrogramDisplay` (+ `SpectrogramBackground`/`Value`/`Overlay`) | `inc/SpectrogramDisplay.h` | Spectrogram visualization | Read-only | `processorRef.getSpectrogram()` | Custom | Medium - candidate for "intentionally decorative" (Strategy E), see Finding 4 |
| `CircularBarDisplay` | `inc/CircularBarDisplay.h` | Per-bar rhythm/beat visualization | Read-only | pushed via `update()`/`setBarPhase()` | Custom | Medium - candidate for Strategy E |
| `SliceWaveDisplay` | `inc/SliceWaveDisplay.h` | Sequencer slice thumbnail overview | Read-only | pushed via setters | Custom | Medium - candidate for Strategy E |
| `StatusBar` | `inc/StatusBar.h` | Transient save/load/delete/rename/script/MIDI-learn feedback | None; auto-fades after 60s or is replaced | `m_statusBar.showMessage(...)` calls throughout `{Module}Editor.h` | Custom | **Critical** - see Finding 5 |
| `juce::MenuBarComponent` (Theme/Patches/Loops/Scripts/About) | `{Module}Editor.h` | Top-level app menu | Stock keyboard nav (arrows, Enter, mnemonics) | theme/patch/loop/script actions | Stock | Low risk - verify only |
| `juce::AlertWindow`-based Save/Rename dialogs | `{Module}Editor.h` (`promptSaveAs`, `promptRename`, loop/script equivalents) | Modal name/folder text entry | Stock Tab/Enter/Escape, `focusNameEditor()` already grabs focus on open (good existing practice) | patch/loop/script name | Stock `AlertWindow` | Low/Medium - verify only |
| `CcRangeRow` (label + `TextEditor` pair) | `inc/CustomRotaryDial.h` | "Value at CC 0/127" numeric entry inside the CC Range `AlertWindow` | Manual side-by-side layout, no explicit label/editor association | CC range low/high | Custom composite of stock parts | Medium - see Finding 6 |
| `juce::NativeMessageBox` confirmations | `{Module}Editor.h` | Delete/tempo-mismatch Yes/No confirmations | Fully native OS dialog | patch/loop/script deletion, BPM conflict | Stock (native) | Low risk - native dialogs are platform-accessible by definition |
| `ScriptEditorWindow` (`CodeEditorComponent` + combo + 3 buttons) | `inc/ScriptEditorWindow.h` | Lua script text editing | Stock code-editor keyboard input | `processorRef.applyScriptText()`/script pool | Mostly stock | Medium - `CodeEditorComponent` screen-reader behavior needs empirical verification (Open Question) |
| `AboutWindow` | `inc/AboutWindow.h` | Read-only license/attribution text | Stock read-only `TextEditor` + `TextButton` | static text | Stock | Low risk |
| `LuaControlArea::ParamWidget` | `inc/LuaControlArea.h` | Script-declared dynamic knob/drop/switch pool | Stock `Slider`/`ComboBox`/`ToggleButton`, **already calls `setDescription()`/`setTooltip()`** | pool params via `juce::ParameterAttachment` | Stock | Low risk - **this is the best-practice pattern already in the codebase; replicate its approach onto `CustomRotaryDial` et al. rather than reinventing one** |
| `GuiLookAndFeel` | `inc/LookAndFeel.h` | Custom paint for rotary slider, toggle button, buttons, combo box, popup menu, menu bar | Painting only, no interaction/accessibility overrides | n/a | Custom `LookAndFeel_V4` subclass | Not itself a component, but affects focus-ring visibility - see Finding 8 |

## 3. Prioritized findings

### Critical

**Finding 1 - Dial controls have no accessible name.**
File/class/method: `JuceStandaloneGenerator/templates/sourcefiles/inc/CustomRotaryDial.h`,
`CustomRotaryDial::CustomRotaryDial()` / `CustomRotaryDial::setLabelText()`.
Problem: `setLabelText()` sets the text of a plain sibling `juce::Label` positioned above the
knob; it is never associated with `m_slider` via `Label::attachToComponent()` or an explicit
`m_slider.setTitle(text)`. `m_slider` (the actual focusable `ModRotaryDial`) never receives a
title/name from anywhere in the generator (`create_init_widgets()` in `codegen_widgets.py`
only calls `.reset()`, `.setLabelText()`, `.setTooltip()`).
Affected users/failure mode: VoiceOver/Narrator users tabbing to any dial hear an unnamed
slider (or, at best, a generic role announcement) with no indication of which parameter it
controls - this affects the majority of controls in every one of the 18 plugins.
Required behavior: the accessible name for a dial must be its display label text.
Recommended mechanism: forward `setLabelText()`'s text to `m_slider.setTitle(text)` (JUCE's
`AccessibilityHandler` default name resolution uses `Component::getTitle()`); done once in
`CustomRotaryDial::setLabelText()`, it is picked up automatically without touching
`create_init_widgets()`.
Scope/dependencies: one method in one shared header; propagates to all 18 blueprints on
regeneration. Regression risk: none to DSP/behavior; purely additive.
Test: VoiceOver focus-cycle on a canary blueprint's Standalone build; each dial must
announce its display name.

**Finding 2 - MIDI-Learn / CC-range / dial-enable menu is mouse-only and undiscoverable.**
File/class/method: `inc/CustomRotaryDial.h`, `ModRotaryDial::mouseDown()`.
Problem: the entire CC-Learn / "Set CC Range..." / "Clear CC Assignment" popup menu, and the
separate "click to disable this modifiable dial" toggle, are gated behind
`modifiers.isPopupMenu()` (right-click/ctrl-click) inside a `mouseDown()` override. No
keyboard path, no `AccessibilityActionType`-based trigger, and no visible on-screen
affordance (icon, button) exists advertising that a CC-mappable dial supports this at all.
Affected users/failure mode: keyboard-only users and screen reader users cannot start MIDI
Learn, inspect an existing CC mapping, edit its range, or clear it - one of the explicitly
named required workflows ("perform MIDI learn, cancel it, inspect an existing mapping") is
completely unreachable without a mouse. Sighted mouse users who don't already know the
right-click gesture exists also cannot discover the feature.
Required behavior: the same actions must be reachable via keyboard (e.g. the conventional
context-menu key / Shift+F10, per the keyboard contract in Section 6) and exposed as
accessibility actions, with some persistent visual indicator that a dial is CC-mappable.
Recommended mechanism: refactor the popup-menu-building code already in `mouseDown()` into
a private `showCcMenu()` method; call it from both `mouseDown()` (existing) and a new
`keyPressed()` override handling the menu key / Shift+F10; register it additionally as
`AccessibilityActionType::showMenu` in a small `createAccessibilityHandler()` override on
`ModRotaryDial` (Strategy A: keep the component, add/override
`createAccessibilityHandler()`) so VoiceOver's rotor/Narrator's context-menu gesture reaches
it too.
Scope/dependencies: `ModRotaryDial` only; no change to `CcMenuCallbacks` or the
processor-side CC-learn/CC-range API. Regression risk: low - purely additive keyboard/action
path alongside the existing mouse path.
Test: keyboard-only walkthrough - open CC menu via keyboard, start Learn, send a CC message,
confirm the assignment; open "Set CC Range...", edit both fields via keyboard, confirm
Cancel and OK both work as expected.

**Finding 3 - Dial modifier (enable/disable) toggle is mouse-only.**
File/class/method: `inc/CustomRotaryDial.h`, `ModRotaryDial::mouseDown()` (the
`m_isModifiable && modifiers.isPopupMenu()` branch).
Problem: same trigger gate as Finding 2, but a functionally distinct feature (some dials can
be right-click-toggled enabled/disabled, independent of CC mapping).
Affected users: same as Finding 2 - keyboard/AT users cannot toggle it at all.
Required behavior: reachable via the same keyboard/accessibility-action path as Finding 2,
disambiguated by menu content when both features are present on one dial.
Recommended mechanism: fold into Finding 2's `showCcMenu()`/`keyPressed()` refactor - both
branches already share the same `mouseDown()` entry point, so the fix is one change, not two.
Scope/dependencies/test: same as Finding 2.

**Finding 4 - Level/CPU/waveform meters expose no accessible value.**
Files/classes: `inc/GenericMeter.h` (`Gauge`, `GaugeValue`), `inc/CpuMeter.h` (`CpuGauge`,
`CpuValue`), `inc/WaveformMeter.h` (`WaveformGauge`, `WaveformShow`). Also relevant, lower
severity: `inc/SpectrogramDisplay.h`, `inc/CircularBarDisplay.h`, `inc/SliceWaveDisplay.h`.
Problem: all of these are plain `juce::Component`s that convey their entire state through
custom `paint()` pixel drawing (bar-fill height mapped from a dB/percentage value); none
implement an `AccessibilityValueInterface` or expose formatted current-value text; several
inner subcomponents explicitly call `setInterceptsMouseClicks(false, false)`, reinforcing
that no accessibility surface currently exists for them.
Affected users/failure mode: a screen reader user has no way to learn input/output level
(clipping risk), CPU headroom, or waveform amplitude - this directly undermines "safe
output-level behaviour" (goal 7), since visual-only feedback is the sole channel for
clipping/overload state today.
Required behavior: VU/CPU/Waveform gauges (the three that carry safety- or
performance-relevant information not duplicated elsewhere) must expose a live, formatted
accessible value (e.g. "Input -6.2 dB, Output -4.8 dB", "CPU 12%") that updates when the
displayed value changes. Spectrogram/CircularBar/SliceWave are recommended as **Strategy E**
(intentionally decorative, `setAccessible(false)`) since their information is either
supplementary detail already covered by the VU meter (spectrogram) or purely visual timing
aids duplicating host transport / on-screen playhead state (circular bar, slice wave) -
confirm no unique information is lost per-blueprint before finalizing E vs. C per gauge type
(Open Question).
Recommended mechanism: **Strategy C** (split visual rendering from an accessible semantic
layer) for VU/CPU/Waveform - add a small custom `AccessibilityHandler` (role
`AccessibilityRole::label` or similar, with a value interface) to the outer `Gauge`/
`CpuGauge`/`WaveformGauge` component, and call
`handler->notifyAccessibilityEvent(AccessibilityEvent::valueChanged)` from each `update()`
call. This runs on the message thread only (the timer-poll -> `update()` call chain already
does), so it introduces no audio-thread work.
Scope/dependencies: 3 shared header files + the timer-driven `update()` call sites already
generated by `create_gauge_callbacks()` (no codegen change needed if the accessible-value
formatting lives inside the component itself). Regression risk: low, purely additive.
Test: VoiceOver/Accessibility Inspector value-changed announcement while audio is playing on
a canary blueprint with a VU meter (e.g. `minireverb`) and one with a CPU gauge.

**Finding 5 - Status messages (save/load/error/MIDI-learn feedback) are never announced.**
File/class: `inc/StatusBar.h` (`StatusBar::showMessage()`, `StatusBar::paint()`).
Problem: pure custom-painted `Component` with no `AccessibilityHandler` at all; every call
site across `{Module}Editor.h` (patch save/load/delete/rename, loop save/load/delete/rename,
script apply/save/delete/rename, LLM-Assist status, tempo-mismatch resolution) routes its
only user-facing confirmation through this component's `repaint()`-only text change.
Affected users/failure mode: a screen reader user who triggers "Save" has no way to learn
whether it succeeded, failed, or - for scripts - what error occurred, since nothing is
announced unless they happen to move focus to the status bar and re-read it, which nothing
in the UI prompts them to do. This is a core-task-completion blocker (the plan's own end-to-
end scenario "load a preset, edit a parameter, reset it, save a variation" cannot be
confirmed as having worked, by a screen reader user, without this fix).
Required behavior: every `showMessage()` call must be announced as a live region without
requiring focus to move.
Recommended mechanism: **Strategy C** - give `StatusBar` a minimal custom
`AccessibilityHandler` (role `AccessibilityRole::staticText`) and call
`handler->notifyAccessibilityEvent(AccessibilityEvent::titleChanged)` (or `valueChanged`,
whichever JUCE version-appropriate event maps to a live-region announcement without focus
change - confirm exact enum during implementation) inside `showMessage()`, right after
`m_message = message;`.
Scope/dependencies: one shared header; every existing `m_statusBar.showMessage(...)` call
site in `{Module}Editor.h` benefits automatically, no call-site changes needed.
Regression risk: low - additive only; verify the announcement doesn't double-fire or spam
during the 60-second auto-fade-to-empty (`timerCallback()` clearing `m_message` should
probably NOT re-announce an empty string - explicit check needed during implementation).
Test: VoiceOver announcement check across at least Save, a failed Delete, and a script-apply
error on a canary blueprint that has patches, loops, and scripts (e.g. `dronesequencer`).

### High

**Finding 6 - `CcRangeRow`'s label/editor pairing is not accessibility-associated.**
File/class: `inc/CustomRotaryDial.h`, `CcRangeRow` (used inside `showSetRangeDialog()`'s
`AlertWindow`).
Problem: `CcRangeRow` manually lays out a `juce::Label` and a `juce::TextEditor` side by
side (chosen specifically to avoid `AlertWindow::addTextEditor()`'s label-truncation
behavior, per the existing code comment) but never associates them for accessibility (no
`Label::attachToComponent()`, no `m_editor.setTitle(labelText)`).
Affected users/failure mode: a screen reader focusing the "Value at CC 0" editor likely
hears only "edit text" with no indication of which field it is, in a dialog with two
visually-identical numeric fields ("Value at CC 0" / "Value at CC 127") - easy to mix up.
Required behavior: each editor announces its own row's label as its accessible name.
Recommended mechanism: add `m_editor.setTitle(labelText)` in `CcRangeRow`'s constructor
(one line, no layout change, preserves the existing side-by-side visual fix).
Scope/dependencies: one class in one shared header. Regression risk: none.
Test: VoiceOver focus on both editors in the CC Range dialog; each must announce its own
label.

**Finding 7 - `drop` (ComboBox) and `switch` widgets have no explicit accessible name beyond
default component-name fallback.**
File/method: `codegen_widgets.py`, `create_init_widgets()` (the `"drop"` and `"switch"`
cases) - only `.setTooltip(...)` is emitted; `juce::ToggleButton`'s constructor is passed
`item['display']` as its button text (which JUCE does use as the accessible name fallback
for `Button`-derived components, so `switch` is likely fine already - verify empirically),
but `juce::ComboBox` receives no equivalent name source at all.
Affected users/failure mode: a `drop` control announces only its currently-selected item
text, with no indication of what the control as a whole represents, if no title is set.
Required behavior: every `drop`/`switch` control's accessible name is its blueprint
`"display"` text.
Recommended mechanism: in `create_init_widgets()`, add
`{varname}.setTitle(juce::String::fromUTF8("{item['display']}"));` alongside the existing
`.setTooltip(...)` line for the `"drop"` case (and, once Finding-2's empirical check
confirms whether `ToggleButton`'s button-text fallback is sufficient, the `"switch"` case
too if not).
Scope/dependencies: one codegen function; propagates to all blueprints on regeneration.
Regression risk: none - additive.
Test: VoiceOver name-announcement check on one `drop` and one `switch` control per canary
blueprint.

**Finding 8 - Keyboard Tab order does not match the visual layout, project-wide.**
Files: `codegen_widgets.py` `create_init_widgets()` (adds children in `ports-control` JSON
array order), `JuceStandaloneGenerator/parseboxstructure.py` (parses the actual visual
`"layout"`/`"performance-page"` composition grid, currently only consumed for
`setVisible()` page-switching in `create_page_switch_methods()`, not for focus order).
Problem: verified concretely against `minireverb.json` - `ports-control` declares
`order, dry, wet, stereoWidth, baseSize, sizeFactor, bulge, uniqueDelay, decay,
allPassUp, ...`, while the visual composition groups controls as `[level, cpu]`, `[decay,
dry, wet, width, fft]`, `[size, sizeFactor, bulge, uniqueDelay]`, etc. - a materially
different order. Since no `setExplicitFocusOrder()` or custom `KeyboardFocusTraverser` is
set anywhere (confirmed absent by grep), JUCE's default traversal follows z-order
(== add order == JSON declaration order), which does not match the on-screen grid for any
blueprint that declares parameters in a different order than they're visually grouped
(the common case - `ports-control` order tends to follow DSP-signal-chain logic, not visual
column layout).
Affected users/failure mode: keyboard-only and screen reader users cannot build a reliable
mental map of "next control" while tabbing - Tab lands on visually distant controls in an
order unrelated to what's on screen, for every one of the 18 plugins.
Required behavior: Tab/Shift+Tab order should follow the visual composition grid's
reading order (left-to-right, top-to-bottom within each area, area order as declared).
Recommended mechanism: add a small helper in `parseboxstructure.py` (or a new function in
`codegen_widgets.py` that reuses its existing `parse_box_structure()`/`composition_shorts()`
building blocks) that assigns a numeric rank to every `short` symbol in visual reading
order; `create_init_widgets()` then calls `{varname}.setExplicitFocusOrder(rank)` for every
generated widget. This does not change z-order/paint order/`addAndMakeVisible` call order
(avoiding any visual regression risk) - it only overrides focus traversal.
Scope/dependencies: touches `parseboxstructure.py` (new/reused helper) and
`create_init_widgets()`; propagates to all blueprints on regeneration. Higher regression
risk than most findings here because it changes behavior on every single blueprint's full
control set at once - needs careful per-blueprint spot-checking (Phase 4 test strategy).
Test: keyboard-only Tab walkthrough on at least 3 structurally different blueprints
(a simple one, one with a Performance/Settings page switch, one with the most controls),
confirming order matches the visual grid.

### Medium

**Finding 9 - `CustomRotaryDial`'s custom knob painting may visually obscure the default
focus ring.**
File/method: `inc/LookAndFeel.h`, `GuiLookAndFeel::drawRotarySlider()`.
Problem: this override fully repaints the knob face (background arc, status-ring arc, disk
gradient, shadow, rim highlight, inner ring) with no explicit focus-state handling; it's
unverified whether `LookAndFeel_V4`'s default focus-ring painting (drawn by the base
`Slider`/`Component` focus machinery, typically layered around the component's bounds)
remains visible against this fully custom-painted disk, especially since the knob is drawn
inset from the component's bounds by several constants (`extraMargin`,
`statusOutlineThickness`, `bedOutline`, `bedThickness`).
Affected users/failure mode: sighted keyboard-only users may be unable to tell which dial
currently has focus.
Required behavior: a clearly visible, non-color-only focus indicator on every focused
control, including dials.
Recommended mechanism: needs an empirical screenshot/visual check first (Open Question);
if the default ring is indeed hidden, add an explicit focus outline draw at the end of
`drawRotarySlider()` gated on `slider.hasKeyboardFocus(false)`.
Scope/dependencies: one method in the shared `LookAndFeel`; propagates everywhere on
rebuild (no regeneration needed, just recompile, since `LookAndFeel.h` isn't
blueprint-specific content).
Test: keyboard Tab to a dial in the Standalone app, screenshot/visually confirm a focus
indicator is present at both 100% and 200% OS display scaling.

**Finding 10 - Disabled/CC-mappable-toggle state on dials relies on alpha/color contrast
only, at the sighted-non-screen-reader level.**
File/method: `inc/LookAndFeel.h`, `GuiLookAndFeel::drawRotarySlider()` (the
`slider.isEnabled()` branch swapping `statusOutline`/`backgroundDark` for their
`*Disabled` alpha-reduced variants).
Problem: the enabled/disabled visual distinction for a dial is alpha/opacity-based, not
shape- or pattern-based (contrast this with the toggle switch, which already moves the
indicator dot left/right - a positional cue, not just color - verified-OK, not a finding).
Affected users/failure mode: low-vision or color-vision-deficient sighted users may have
difficulty distinguishing enabled vs. disabled dials at a glance; screen reader users are
unaffected since `isEnabled()` state is announced automatically by JUCE's built-in
accessibility regardless of paint code.
Required behavior: an additional non-alpha visual cue (e.g. a distinct "muted" fill pattern
or icon) for disabled dials, consistent with the toggle switch's existing positional-cue
precedent.
Recommended mechanism: extend `drawRotarySlider()`'s disabled branch; cross-reference the
existing Orbit theme accessibility/contrast work (per project memory) before choosing new
colors, to stay consistent with whatever contrast validation already exists there.
Scope/dependencies: one method, shared `LookAndFeel`. Regression risk: low, purely visual.
Test: side-by-side screenshot of an enabled vs. disabled dial in grayscale (simulating
achromatopsia) - the two states must remain distinguishable.

**Finding 11 - `ScriptEditorWindow`'s `CodeEditorComponent` accessibility behavior is
unverified.**
File: `inc/ScriptEditorWindow.h`. JUCE's `CodeEditorComponent` has historically had
inconsistent screen-reader line-by-line reading behavior across platforms/JUCE versions.
This is flagged as **needing manual verification**, not asserted as broken - explicitly
called out per the requirement to not guess where source inspection is insufficient.
Test: open the Script Editor (any blueprint with `use-lua: true`, e.g. `dronesequencer`),
use VoiceOver to navigate and read existing script text, confirm line-by-line reading and
edit-position announcement work acceptably; if not, evaluate whether a JUCE-version bump or
a custom `AccessibilityHandler` wrapper is warranted (open scope question, not pre-decided
here).

**Finding 12 - Minimum editor size is fixed to each blueprint's initial window size; no
lower floor for constrained host racks.**
File: `{Module}Editor.h` constructor - `setResizeLimits(GuiConstants::instance().init.WindowWidth,
GuiConstants::instance().init.WindowHeight, 4000, 3000)` sets the *minimum* resizable size to
the blueprint's own default (e.g. 1200x800 for `minireverb`, from `GuiConstants::init`),
meaning the plugin can never be shrunk smaller than that.
Affected users/failure mode: this is good for legibility (no accidental illegible-tiny
state) but may not fit constrained host mixer racks or small-resolution/laptop displays; not
a screen-reader issue, a magnification/small-viewport issue.
Required behavior: verify at the smallest currently-permitted size (the blueprint default)
that magnification/200% OS scaling still leaves all controls reachable and legible; this is
primarily a **test item**, not a presumed code change - resizing smaller may be intentionally
disallowed by design and should not be loosened without confirming that's wanted (Open
Question).

### Low

**Finding 13 - No explicit reduced-motion accommodation, though nothing currently animates
in a high-risk way.**
Files: `inc/MomentaryToggleButton.h` (forced 300ms visual "flash" hold on trigger),
meter/gauge classes redrawing at `GuiConstants::instance().init.TimerHertz` (60 Hz).
Assessment: 60 Hz data-refresh repaints and a single 300ms non-repeating flash are not
strobing/parallax effects and are unlikely to trigger vestibular or photosensitive
reactions; no OS "reduce motion" query (`Desktop::isReducedMotion... `-style, or
platform-specific equivalent, if JUCE exposes one) is consulted anywhere. Recommended as
low-priority polish: confirm during Phase 7 whether JUCE's current API exposes a
reduced-motion signal at all before committing to any change here (Open Question).

**Finding 14 - Whether tooltip text already surfaces as the accessible description is
unverified.**
Context: `.claude/tooltips-for-components.md` (already-landed but not fully
gold-master-rebaselined per its own text) added `.setTooltip(...)` to every generated
widget kind, including dials and gauges. JUCE's `SettableTooltipClient` tooltip text and a
component's `AccessibilityHandler` description are not necessarily the same pipe in every
JUCE version/component type. This must be checked empirically (Phase 1) before deciding how
much of Findings 1/4/7 the tooltip work already mitigates versus how much still needs
`setTitle()`/a real `AccessibilityHandler`.

## 4. Proposed target accessibility architecture

No new cross-cutting abstraction is needed - JUCE's own `AccessibilityHandler`,
`AccessibilityActions`, `AccessibilityValueInterface`, and `SettableTooltipClient` cover
every case identified above. The architecture is: **augment existing components in place**
(Strategy A for nearly everything), with two **Strategy C** exceptions (`StatusBar`,
VU/CPU/Waveform gauges) where the component genuinely has no natural accessible
representation to hang a title/value off of, and up to three **Strategy E** exceptions
(Spectrogram/CircularBar/SliceWave, pending the Open Question about redundant information)
where marking the component decorative is honest rather than inventing a synthetic summary.
No component needs a full rewrite (Strategy B) or an interaction-model replacement
(Strategy D) - every existing interaction pattern (drag, text entry, click, right-click
menu) is soundly designed; it's specifically the *accessible surface* around already-correct
mouse/keyboard behavior that's missing.

| Component category | Strategy | Role | Key mechanism |
|---|---|---|---|
| `dial` (`CustomRotaryDial`/`ModRotaryDial`) | A | `AccessibilityRole::slider` (JUCE `Slider` default) | `setTitle()` forwarding (Finding 1); `createAccessibilityHandler()` override adding a `showMenu` action for CC-Learn (Finding 2/3) |
| `switch` (`ToggleButton`/`MomentaryToggleButton`) | A | `AccessibilityRole::toggleButton` (JUCE default) | Verify button-text-as-name is sufficient; no other change anticipated |
| `drop` (`ComboBox`) | A | `AccessibilityRole::comboBox` (JUCE default) | `setTitle()` addition in codegen (Finding 7) |
| VU/CPU/Waveform gauges | C | `AccessibilityRole::label` + value interface | New minimal `AccessibilityHandler` per class, `notifyAccessibilityEvent(valueChanged)` on `update()` (Finding 4) |
| Spectrogram/CircularBar/SliceWave | E (pending confirmation) | n/a (`setAccessible(false)`) | Confirm no unique info lost per blueprint first |
| `StatusBar` | C | `AccessibilityRole::staticText`, live region | New `AccessibilityHandler`, `notifyAccessibilityEvent` on `showMessage()` (Finding 5) |
| `CcRangeRow` editors | A | `AccessibilityRole::editableText` (JUCE `TextEditor` default) | `setTitle()` per row (Finding 6) |
| `MenuBarComponent`, `AlertWindow`, `NativeMessageBox`, `ScriptEditorWindow`, `AboutWindow`, `LuaControlArea` | A (verify-only) | JUCE defaults | No code change anticipated unless Phase 1 empirical testing finds a real gap |

Continuous-parameter (dial) accessible contract, once Findings 1-3 land:
- Label/unit/min/max/default/step: already correct via the existing
  `AudioParameterFloatAttributes.withLabel/.withStringFromValueFunction` -
  `AccessibilityValueInterface` on a stock `Slider` derives its value text from the same
  `getTextFromValue()` path already wired for the visual text box, so no duplicate text
  source to maintain.
- Pointer drag: unchanged (JUCE `Slider` default rotary-drag).
- Fine/coarse adjustment: JUCE `Slider` already maps arrow keys to
  `getInterval()`-sized steps by default; verify this matches the parameter's configured
  `intervalValue` per blueprint (Open Question - not independently re-verified here).
- Keyboard mapping: arrows (fine step, JUCE default), Page Up/Down (coarse step - JUCE
  `Slider` default multiplies the interval; verify factor is sensible for each parameter's
  range), Home/End (JUCE `Slider` default: min/max), direct numeric entry (already works via
  the visible text box), reset (not currently bound to any key - recommend a documented
  key, e.g. a double-click-equivalent or a dedicated shortcut, decided in Section 6).
- Context menu/reset/MIDI-learn: Finding 2/3's keyboard-triggered menu.
- Base vs. effective/modulated value: this codebase has no separate internal-modulation-
  source layer distinct from the parameter's own value (confirmed by inspection - no
  modulation-depth/routing UI beyond the value arc itself) - flagged as an Open Question
  rather than assumed absent by oversight, since the goal document explicitly asks about it.

Discrete-control (switch/drop) accessible contract:
- Role: JUCE defaults (`toggleButton`/`comboBox`) - correct already.
- Toggled/selected/disabled state: JUCE fires these automatically via `setToggleState()`/
  `setEnabled()` - verified-OK.
- Keyboard activation: Space/Enter (`ToggleButton`), arrow+type-ahead+Enter (`ComboBox`) -
  JUCE defaults, verified-OK.
- Non-colour visual state cue: toggle switch already uses dot position, not just color -
  verified-OK (see Finding 10's contrast with the dial's alpha-only cue).
- Accessible label/description: Finding 7 (name), existing `.setTooltip()` rollout
  (description, pending Finding 14's verification).

## 5. Detailed phased implementation plan

### Phase 1 - Foundation and audit fixes (empirical baseline)

- Files touched: none (verification-only), except possibly a build config check.
- New abstractions: none.
- Work: build a canary blueprint's Standalone target (recommend `minireverb` - has dials,
  switches, a VU gauge, and a CPU gauge in one place - plus `dronesequencer` for
  patches/loops/scripts/Lua-pool coverage) via `dev-scripts/dev-test-full.sh`. Run
  VoiceOver + macOS Accessibility Inspector against both. For each Critical/High finding
  above, confirm or refute it against the real running app, and specifically resolve
  Finding 14 (does tooltip text already appear as the accessible description?).
- Acceptance criteria: a written baseline note per finding, confirmed or refuted against a
  real screen reader, not source-review alone.
- Test strategy: manual only, this phase produces no code.
- Risks/rollback: none - read-only investigation.
- Complexity: **S**.

**Baseline results (canary: `maxdiffuser`, `Maxdiffuser_Standalone`, built clean via
`dev-test-full.sh`, VoiceOver on macOS):**

- **Finding 14 - RESOLVED, tooltip text does NOT reach the accessible name/description.**
  Tabbing to a dial (e.g. "Dry", "Wet"), VoiceOver announces the current value, then only
  "slider" - no parameter name. Confirms the existing `.setTooltip()` rollout does not
  substitute for `setTitle()`; Finding 1's fix is fully needed as planned, not partially
  mitigated.
- **Finding 1 - CONFIRMED.** Same observation as above, directly - dials have no
  accessible name.
- **Findings 2/3 - CONFIRMED.** With a CC-mappable dial focused (not right-clicked), a
  keyboard-only trigger (Shift+F10 / Menu key) does not open the CC-Learn/Set-Range menu;
  mouse right-click still works. Keyboard path is entirely absent, as predicted from source.
- New Phase-1 observation (not in the original source-review findings): `maxdiffuser`
  additionally has two blueprint-specific `customtype` gauges
  (`examples/maxdiffuser/src/impl/ShowProcessingBinsBands.h`,
  `.../ShowProcessingBins.h`, bound as "Bands"/"Sizes") - plain custom-painted
  `juce::Component`s with no accessibility surface, same category as Finding 4 but outside
  the shared template set (hand-written per-blueprint `impl/` files, out of the generator's
  ownership per the existing tooltip-rollout plan's own precedent for `customtype` gauges).
  Not yet walked with VoiceOver for focus/announcement behavior - carry forward as a
  Finding-4-adjacent item to check in Phase 2/4's gauge work, decide there whether it's
  in-scope (same fix pattern, applied by hand since codegen doesn't own it) or accepted gap.
- Remaining checklist items (5, 6, 7, 8, 9 from the original list) were not walked
  individually in this pass ("everything seems working well" was the general report) - not
  re-blocking Phase 2, since 1/2/3/14 (the scope-determining items) are resolved, but still
  open for a future targeted pass if time allows.

### Phase 2 - Shared accessible component infrastructure

- Files touched: `inc/CustomRotaryDial.h` (Findings 1, 2, 3), `inc/StatusBar.h`
  (Finding 5), `inc/GenericMeter.h`, `inc/CpuMeter.h`, `inc/WaveformMeter.h` (Finding 4),
  `codegen_widgets.py`'s `create_init_widgets()` (Finding 7).
- New abstractions: none new - reuses JUCE's own `AccessibilityHandler`/
  `notifyAccessibilityEvent` API directly in each affected class; if duplication across the
  3 gauge classes turns out to be significant, a small shared free function/mixin in
  `GenericMeter.h` (the header all three already draw shared pieces from) may be introduced,
  decided during implementation rather than pre-declared here.
- Acceptance criteria: on the two Phase-1 canary blueprints, VoiceOver announces a correct
  name for every dial/switch/drop, correct live values for VU/CPU gauges, and status-bar
  messages are announced without moving focus.
- Test strategy: `dev-scripts/dev-build.sh`/`dev-scripts/dev-test-full.sh` (build-only) for
  the canary Standalone targets, then a manual VoiceOver pass repeating Phase 1's checklist.
- Risks/rollback: `CustomRotaryDial` is instantiated 10-20+ times per plugin - a mistake
  here is maximally visible; keep every change additive (no removed/renamed public methods)
  so the existing mouse-driven CC-learn/modifier-toggle paths are never at risk.
- Complexity: **M**.

### Phase 3 - Primary sound-shaping controls (project-wide rollout)

- Files touched: every `examples/*/src/*` generated output (never hand-edited, regenerated
  via `generate-juce-standalone.py`), all 18 `JuceStandaloneGenerator/gold_master/*`
  baselines except the 2 already excluded (`dronesequencer`, `tanpura`, per existing
  convention documented in `tooltips-for-components.md`).
- New abstractions: none - purely propagating Phase 2's changes.
- Work: for each blueprint, check for uncommitted hand-edits to its generated output before
  overwriting (same caution `tooltips-for-components.md` Phase 4 already documents -
  flag anything found rather than silently discarding), regenerate, then
  `check_gold_master.py --check` diff-review to confirm only the intended
  accessibility-related lines changed, then `--record` to rebaseline.
- Acceptance criteria: `check_gold_master.py --check` clean after rebaselining;
  `dev-scripts/dev-warnings.sh` clean project-wide.
- Test strategy: build-only check across all 18 blueprints
  (`dev-test-full.sh <target> -- "^$"` per example, or as many as practical in one pass).
- Risks/rollback: mechanical but wide blast radius - a codegen mistake surfaces on every
  plugin at once; the `--check`-before-`--record` workflow is the safety net, same as the
  tooltip rollout's own documented approach.
- Complexity: **M**.

### Phase 4 - Keyboard focus/navigation and visible focus

- Files touched: `JuceStandaloneGenerator/parseboxstructure.py` (new/reused visual-rank
  helper), `codegen_widgets.py`'s `create_init_widgets()` (Finding 8 -
  `setExplicitFocusOrder()` calls), `inc/LookAndFeel.h` (Finding 9 - explicit focus-ring
  paint if the default is found to be obscured).
- New abstractions: a rank-computation helper reusing `parse_box_structure()`'s existing
  output (no new file).
- Acceptance criteria: Tab/Shift+Tab order on both canary blueprints visually matches the
  composition grid's left-to-right, top-to-bottom reading order, including across a
  Performance/Settings page switch (test on a blueprint that has one, e.g.
  `dronesequencer` if it uses `performance-page` - confirm during implementation); every
  focused control shows a clearly visible, non-color-only focus indicator at 100% and 200%
  display scaling.
- Test strategy: manual keyboard-only walkthrough per canary blueprint;
  `dev-scripts/dev-warnings.sh` clean.
- Risks/rollback: highest regression-visibility item in this plan - changes tab order on
  every control in every blueprint simultaneously; needs per-blueprint spot-checking, not
  just the 2 canaries, before Phase 3-style project-wide rollout is repeated for this
  change (fold into the same rollout pass as Phase 3 if Phase 3 hasn't shipped yet, or as a
  second rollout pass if it has - decide based on how Phase 3 actually lands).
- Complexity: **L**.

### Phase 5 - Presets, menus, overlays, and text entry

- Files touched: `{Module}Editor.h` (verify patch/loop/script `AlertWindow` dialogs'
  built-in `addTextEditor()` label association is adequate - likely no change needed, JUCE's
  own `addTextEditor(name, text, onScreenLabel)` already associates a label), `inc/
  CustomRotaryDial.h`'s `CcRangeRow` (Finding 6), re-verification pass of Finding 5's
  status-bar announcements across every menu action end-to-end (Save/Load/Delete/Rename/
  Apply/MIDI-learn/tempo-mismatch).
- New abstractions: none.
- Acceptance criteria: every text-entry field in every dialog announces its own label when
  focused; every status-bar-driven confirmation is announced without the user manually
  moving focus, across the full set of menu actions.
- Test strategy: manual VoiceOver walkthrough of Save/Rename/Delete for patches, loops, and
  scripts on `dronesequencer` (the only canary with all three).
- Risks/rollback: low - small, additive, well-isolated changes.
- Complexity: **M**.

### Phase 6 - Modulation, MIDI learn, automation, and complex editors

- Files touched: none beyond what Phase 2 already delivered for Findings 2/3 - this phase
  is primarily **verification** that the keyboard-triggered CC-Learn path (built in Phase 2,
  rolled out in Phase 3) works end-to-end for automation/MIDI workflows, plus resolving the
  "base vs. modulated value" Open Question from Section 4.
- Acceptance criteria: MIDI Learn can be started, an existing assignment inspected via its
  CC Range dialog, and cleared, entirely from the keyboard, on a CC-mappable dial in a
  canary blueprint.
- Test strategy: keyboard-only MIDI Learn walkthrough with an actual MIDI controller (or a
  virtual MIDI CC sender) connected to the Standalone app.
- Risks/rollback: none - verification-only phase; any gap found here routes back to
  reopening Phase 2's `ModRotaryDial` changes, not new scope.
- Complexity: **S**.

### Phase 7 - Visual accessibility, scaling, reduced motion, and polish

- Files touched: `inc/LookAndFeel.h` (Finding 10 - non-color disabled-state cue for dials),
  possibly `inc/GuiConstants.h`/theme files if Finding 10's fix needs a new named color
  token (cross-reference the existing Orbit theme contrast work per project memory rather
  than inventing new colors independently).
- Acceptance criteria: enabled vs. disabled dial states remain distinguishable in a
  grayscale/simulated-colorblind screenshot comparison; Finding 12's minimum-size behavior
  is confirmed acceptable (or flagged back to the user if not) at 200% OS display scaling.
- Test strategy: screenshot comparison (grayscale filter) across a representative set of
  theme/hue combinations; manual check at 200% scaling.
- Risks/rollback: low - visual-only, additive.
- Complexity: **S/M**.

### Phase 8 - Manual regression testing and release readiness

- No code changes anticipated (unless Phase 1-7 testing surfaces a gap requiring a
  follow-up fix, handled as its own small phase rather than folded in silently).
- Full execution of the Section 7 test matrix across Standalone, AU, VST3, and AUv3 on
  macOS, and representative DAW hosts.
- Acceptance criteria: every scenario in Section 7 passes on every in-scope format/host
  combination, or has a documented, user-agreed exception.
- Complexity: **L** (time-heavy, not code-heavy).

## 6. Keyboard/focus specification

### 6.1 Intended traversal order

Menu bar (Theme / Patches / Loops / Scripts / About, left to right) -> [Performance /
Settings page-switch buttons, if the blueprint uses `performance-page`] -> composition-grid
controls in visual reading order (each area's declared controls, left to right, top to
bottom, area order as declared in the blueprint's `"layout"`/`"performance-page"` string,
per Finding 8's fix) -> (status bar is not a focus stop - it is an announcement-only live
region, per Finding 5).

- Components that should accept focus: every `dial`, `switch`, `drop` control; menu bar
  items; Performance/Settings page buttons; all dialog/`AlertWindow` text editors and
  buttons.
- Components that should NOT accept focus: `label`-type static text; `StatusBar`; all
  read-only gauge/meter/spectrogram/circular-bar/slice-wave displays (Finding 4's
  Strategy-C gauges are accessible via the value-changed announcement mechanism, not via
  tab-stop focus - they carry no interactive affordance to focus to).
- Focus containers: the top-level `AudioPluginAudioProcessorEditor` is the natural focus
  container; no nested focus-trap container exists today beyond modal `AlertWindow`/
  `DialogWindow` instances, which already correctly trap focus via JUCE's modal-state
  machinery (verified-OK, not a finding).
- No custom `KeyboardFocusTraverser` is needed - `setExplicitFocusOrder()` (Finding 8) is
  sufficient and lower-risk than replacing JUCE's default traversal wholesale.
- Existing key-event handling that may conflict with host shortcuts: none found - no
  component in the reviewed set currently overrides `keyPressed()` at all (Finding 2/3's
  new `keyPressed()` override on `ModRotaryDial` is additive; it should call
  `Slider::keyPressed()`/return `false` for unhandled keys so the host's own shortcuts still
  receive them, per the keyboard contract below).
- Behavior after menus/dialogs/popovers/overlays close: `AlertWindow`/`DialogWindow`
  instances already correctly restore focus to a sensible place on close via JUCE's modal
  stack (verified-OK); the non-modal `ScriptEditorDialogWindow` explicitly deletes itself
  and is a separate top-level window, so main-window focus is unaffected by it closing -
  verified-OK, not a finding.

### 6.2 Keyboard interaction contract

| Action | Behavior |
|---|---|
| Tab / Shift+Tab | Move focus forward/backward through the traversal order in 6.1 |
| Arrow keys (on a focused dial) | Fine-adjust by the parameter's configured interval (JUCE `Slider` default) |
| Page Up / Page Down (on a focused dial) | Coarse-adjust (JUCE `Slider` default multiplier of the interval - verify per-parameter sanity in Phase 1) |
| Home / End (on a focused dial) | Jump to minimum/maximum (JUCE `Slider` default) |
| Enter / Space | Activate a button/switch; open a focused `drop`'s list |
| Escape | Close the currently open transient UI (popup menu, dialog, dropdown) - already correctly wired for `AlertWindow` dialogs (`escapeKey` bound to Cancel) and native `ComboBox` (JUCE default) |
| Direct numeric entry | Already works via each dial's visible text box (Tab to it, type, Enter) - no change needed |
| Reset to default | **Not currently bound to any key** - recommend deciding and documenting a shortcut (e.g. a dedicated key while a dial has focus) as part of Phase 2, rather than leaving it exclusively mouse-driven (double-click-to-reset, if that exists today, is unverified - Open Question) |
| Context menu access | New keyboard path from Finding 2/3 (menu key / Shift+F10) on CC-mappable/modifiable dials |
| Undo/redo, copy/paste | **No undo/redo system exists in this codebase** (confirmed by grep) - out of scope unless the user wants it added as new functionality, which is a product decision beyond this accessibility migration |

## 7. Test matrix and acceptance criteria

Windows/Narrator rows are intentionally omitted - no Windows build is currently maintained;
revisit this plan's test matrix if/when Windows support is added.

| Area | Method | Scope |
|---|---|---|
| macOS VoiceOver | Full keyboard+VoiceOver walkthrough | Standalone build of both canary blueprints (Phase 1-6), all 18 after Phase 3/4 rollout (spot-check subset) |
| macOS keyboard-only (VoiceOver off) | Tab order, arrow/PgUp/PgDn/Home/End, Enter/Space/Escape | Same scope as above |
| macOS Accessibility Inspector | Name/role/value/description audit per control | Both canary blueprints, all Critical/High-finding controls |
| High-DPI / display scaling | Visual check at 100% and 200% OS scaling | Both canary blueprints |
| Minimum editor size | Confirm legibility/reachability at each blueprint's own resize floor (Finding 12) | Representative sample (smallest and largest control-count blueprints) |
| Reduced motion | Confirm no strobing/rapid flashing (Finding 13) | `MomentaryToggleButton`-using blueprint |
| Grayscale / CVD simulation | Screenshot comparison, enabled vs. disabled, on vs. off | Both canary blueprints, all hue/theme combinations touched by Finding 10 |
| Standalone app | All scenarios below | Every canary + representative subset of all 18 |
| AU / VST3 / AUv3 | Editor loads, VoiceOver reaches controls, no host-specific accessibility regression | At least one canary per format |
| Representative DAWs | Manual smoke test | At minimum whatever host(s) are already used for existing manual QA (Open Question - ask user which hosts to prioritize) |
| Loading existing presets/automation | Confirm no parameter ID/state regression from any accessibility change | Both canaries, using an existing saved patch |
| Audio-thread/UI performance | Confirm no audio-thread allocation/locking introduced (all changes in this plan are message-thread-only, per Section 4's design) | Code review + existing `dev-warnings.sh`/sanitizer coverage, no new runtime profiling expected to be needed |

### End-to-end acceptance scenarios

1. Load a preset, edit a primary (dial) parameter via keyboard, reset it, save a variation -
   entirely via keyboard, with every step's outcome announced via VoiceOver.
2. Set an exact parameter value without dragging (via the dial's text box).
3. Toggle bypass/an on-off switch, compare A/B via keyboard (if an A/B feature exists in a
   given blueprint - confirm scope per blueprint, not assumed present in all).
4. Find, inspect, change, and remove a MIDI CC assignment entirely via keyboard
   (Findings 2/3/6).
5. Perform MIDI Learn, cancel it, inspect an existing mapping (Finding 2).
6. Complete a primary sound-design workflow (adjust the blueprint's 3-4 most central
   parameters) using keyboard only.
7. Complete the same workflow using VoiceOver, with all status/value feedback confirmed
   audible at each step.

## 8. Open questions and assumptions

1. **Does JUCE's `SettableTooltipClient` tooltip text already surface as the
   `AccessibilityHandler` description in this project's JUCE version?** (Finding 14) - this
   materially affects how much of Findings 1/4/7 remain after Phase 1's empirical check;
   not resolved by source inspection alone.
2. **Is there any internal modulation-source system (LFO/envelope routing distinct from
   host automation and MIDI CC) anywhere in the DSP layer that the UI is expected to
   surface?** Inspection of the Editor/Processor templates and `LookAndFeel` found none
   beyond the value arc itself - flagging rather than assuming, since the original request
   explicitly asks about base-vs-modulated-value semantics.
3. **Does a double-click-to-reset-to-default gesture already exist on dials via JUCE
   `Slider`'s own default behavior, and if so, does the project want an additional explicit
   keyboard-bound reset, or is relying on JUCE's default sufficient?**
4. **For Spectrogram/CircularBar/SliceWave (Finding 4), does every blueprint that uses one
   also expose the same information some other accessible way** (e.g. a VU gauge covering
   what the spectrogram would add), or are there blueprints where one of these is the *only*
   source of some safety/functional information? This needs a per-blueprint check before
   finalizing Strategy E for all three.
5. **Which specific DAW hosts should the manual test matrix (Section 7) prioritize?** No
   existing "representative hosts" list was found in the repo's docs/CI.
6. **Windows/Narrator support**: explicitly out of scope for this plan per the user's
   direction (no Windows build currently maintained); this plan's keyboard-contract and
   architecture sections are written to be Windows-portable in principle (nothing proposed
   is macOS-specific beyond the AU/AUv3-vs-VST3 accessibility-bridge note in Section 2.1),
   but no Windows-specific testing or `Narrator`/Accessibility Insights verification is
   planned until that build exists.
7. **A/B comparison feature**: not found as a distinct, named UI feature in the reviewed
   shared templates (only per-blueprint `switch`/bypass-style controls, which vary by
   blueprint) - Section 7's scenario 3 is scoped as "if present," not assumed universal.
8. **Undo/redo**: confirmed absent from the codebase (Section 6.2) - flagged as a scope
   boundary, not silently ignored.
