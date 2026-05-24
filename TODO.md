The Todo which is likely never be done:

## DSP Library

- add FDN
- add Resampler
- add Modulation Delays (with pitched size changes)
- 


## Project Generator

- Generate Standalone Juce project and examples Juce project
- Make the UI better (again)
  - automatic position stuff
  - integer parameters honored correctly
  - sliders?
- Save presets
- Midi only 
- 5.1


It contains a metronome with a settable speed, e.g. 40-250 BPM
a volume control for the Metronome(-60 to 0dB), 
a volume control for the input signal.
A start stop switch.
We keep like the plaingain and guisandbox the CPU Level gauges, spectrogram and wave display.
Try to figure out how to arrange the UI items, you have "layout" "type" and "composition".
The metronome tick itself could be a simple Soundgenerator with a damped sine wave.
This would be the first iteration.

you should call :
"./generate-juce-standalone.py metronome" 

inside it's folder and it will generate in examples.
Modify the CMakeLists.txt by adding the new directory:
add_subdirectory(examples/metronome)

