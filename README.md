# AbacDsp
Dsp code for abacad projects

## Goals

Have handy classes for various DSP tasks.
No dependencies for the dsp code it self (examples and unit-test have submodules based code)

### Submodules used
- googletest
- Audiofile
- juce v8

### Class design

- float based unless we really need precision for iterative algorithms
- mostly templates based for adaptive code and better optimisations.
- classes should ctor with the samplerate
- blockoperations of BlockSize=8 Samples for better compiler optimisations
- operations on simple buffer design with interleaved or mono array
- raw float operations allowed but with BlockSize only
- testability

## What
- Analysis
- Generic Audio
- Filters
- Generators (from Naive to more sophisticated band limited version)
- Parameter Smoothing
- Conversions



### Usage

For usage check always the unit-tests or examples, these contain implementations that should cover and 
which should be self-explanatory.
