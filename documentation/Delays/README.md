# Delays verification plots

Visualizes and verifies the delay classes in `src/includes/Delays/` directly, with no JUCE
involved. One subfolder per class, each with its own explore program, README and checked-in PNGs:

| Folder | Covers |
|---|---|
| `VariSpeedTapeDelay/` | wow and flutter baked into the recorded pitch, octave-based transport-speed glide, write-side aliasing across ratios with a measured SNR table |
| `WobbleDelay/` | single-rate delay with Wow and Flutter on the read head: pitch wobble, crossing clamp, retune glide, swept-tone spectrograms |
| `MultiTapDelay/` | whole-sample-only tap spacing |

Running `WobbleDelay` at another sample rate (oversampling, undersampling, transport speed) is
`UpDownSampler`'s job and is documented in `../OverSampling/`.

## Quick start

`./generate.sh` runs the whole pipeline in one step: configures and builds the three explore
programs (behind `EXPLORE_STUFF`, into a gitignored `build/` at the repo root), runs each, sets up
a local `.venv` from `requirements.txt`, and renders every PNG into its class's subfolder.
Intermediate data (`.txt` plot series, spectrogram grids, `.wav` renders for listening) goes into
a gitignored `generated/` folder.

## Shared files

- `generate.sh`, `CMakeLists.txt`: build and run all three programs, render all plots.
- `DelaysExploreCommon.h`: sample rate and tile size, the spectrogram-grid writer, the pitch
  tracking window, and the shared-ratio aliasing sweep.
- `spectrogram_plot.py`, `requirements.txt`: renders spectrogram grids as heatmaps. Time series
  plots use `../Plot/PyConPlot.py`.
