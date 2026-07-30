# Looper

## CircularLoopDisplay enhancements (done)

Volume curve moved off the spectrogram band into its own ring just inside it (`volOuterR`/
`volInnerR`), drawn as a filled band polygon anchored at `volOuterR` and dipping inward with
loudness, instead of a stroked line overlapping the spectrogram. Spectrogram's jagged raster
edge is masked with a 3px background stroke at `ringInnerR`/`ringOuterR`. The fast (bar-phase)
hand no longer reaches the hub; the centre shows Bar.beat and status text instead. The special
12 o'clock emphasis on the outer bar spokes and inner beat ticks was removed so all markers
match. The inner bar disc was shrunk to make room for the volume band.