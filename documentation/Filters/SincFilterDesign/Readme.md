# Generating Filter Coefficients

The scripts below are used in [GNU Octave][GNU Octave] which should be available
on most platforms including Linux, Mac and Windows.

On Debian: `sudo apt install octave octave-signal`.

On Mac OSX use brew: `brew install octave`.

Run octave and install the packages with:
```
# octave
pkg install -forge control signal
```

Filter coefficients can be generated as follows by running the `octave` command
in the same directory as the scripts and this readme file. It will generate automatically 
an include file containing the coefficients.

```
octave:1> pkg load signal
octave:2> f = make_filter (8, 128, 100.3);

These have been tried and are ok:

f = make_filter(2, 128, 27.2); # FIR multiplies: 7.92188
f = make_filter(3, 128, 44.3); # FIR multiplies: 12.8906
f = make_filter(4, 128, 55.0); # FIR multiplies: 17.5469
f = make_filter(5, 128, 77.6); # FIR multiplies: 22.9844
f = make_filter(6, 128, 87.2); # FIR multiplies: 27.6094
f = make_filter(7, 128, 90.0); # FIR multiplies: 31.7969
f = make_filter(8, 128, 100.0); # FIR multiplies: 36.4375
f = make_filter(11, 128, 160.0); # FIR multiplies: 52.25
f = make_filter(13, 256, 180.0); # FIR multiplies: 61.6094
f = make_filter(21, 512, 180.0); # FIR multiplies: 93.9414
f = make_filter(33, 512, 170.0); # FIR multiplies: 141.543
f = make_filter(69, 768, 170.0); # FIR multiplies: 286.305
```


[GNU Octave]: https://www.gnu.org/software/octave/