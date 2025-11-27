# BlockProcessors

Goal is to have simple processors that can be set as callbacks for the FDN based reverb tank.

The processors can do post delay line manipulations that then are fed back to the FDN.

All processors in this section work inplace.

The various purposes are:

- Change frequency response
- Add dispersion e.g. Slow vibrato modulation
- Add shimmer (using the grain pitch shifter)

## What is missing

- Shelving filters (for frequency response)
- Allpass filters (dispersion)
