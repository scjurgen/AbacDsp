# TODO

## Looper with Sample Sequencer

The current idea to slice in any case is not a good concept.
We need to make the slicing on demand. For this purpose we need a 2 tiered engine:
first layer the plain looper in the traditional way.
On top of this a sequencer playing engine that will be fed with sample slices when demanded.
The sequencer will receive slices (reference to samples that it will copy lasy from the looper buffer) and the timing 
information relative to the beat of the looper buffer.

First step: clean up the current looper to accommodate the new architecture. Keep the UI as is for now (but remove the transient visualisation)
Second step: enhance the looper itself with following concepts:
  - we are recording based on a beat. If the threshold kicks in before the start of a new beat (i.e less than a 1/8 note early) we 
    copy the data to a temporary buffer, this part will be copied to the end of the final recorded loop.
    if we actually play with delay (i.e. max 1/8th note after the beat) we pad the looper buffer with empty samples. 
  - The same logic is applied when we press stop of the recording. Apply fade out, fade in for avoiding clicks, the fade 
   size should have a setter.
  - we record always first everything in a ringbuffer so we can copy data from it (pre and post recording) which should be half of the fade window (linear fade should do it). 
  - the looper time should always be a multiple of the beat, e.g. with 120BPM we start recording and stopping recording after 48050 smples the 
    time should be 48000 samples (48kHz sample rate)

Third step would be designing the slicing and sample sequencer which I will design after the first two steps.



## Looper

New looper mode:
I would like to add a sequencer specific recording mode. Detect onsets (simple threshold) and store them in the audio buffer. Every time
there is a new 

Next steps:

- visualise looper recording as Spectrogram, this should proof useful also for the next slizing step so we do some extraction
- slice material after recording (in parallel to recording in another process).
- play out the extracted samples when looping instead of the recording buffer.
- Have also a free recording instead BPM based and extract the actual BPM when recording stopped
- pitch shift slices (saves new sample, needs good memory handling, we are realtime)
- shuffle slices
- reverse play, the beginning lands on the beat (so the playout position is before the beat with the length of the sample)


## Code quality
### Sanitizier

- Tried wiring up ASan+UBSan via a CMake ENABLE_SANITIZERS option (2026-07-11):
  AddressSanitizer's dynamic runtime hangs at process startup on this Mac
  (Apple clang 17 / macOS 26.5.1) even for a trivial hello-world binary,
  stuck in AsanInitFromRtl's shadow-memory init. UBSan alone works fine.
  Sticking with Valgrind (docker-unit-tests) for now. Revisit ASan later,
  either once Apple/LLVM fixes this, or by running it in the Linux Docker
  container instead of natively.

## Cleanup

## Project Generator

- Make the UI better (again)
  - automatic position stuff
  - integer parameters honored correctly
  - sliders?
- Enhanced save presets (with names)
- Synth modules without AudioIn
- 5.1
- Background silkmask
