# Generating Filter Coefficients

The scripts below are used in [GNU Octave][GNU Octave] which should be available
on most platforms including Linux, Mac and Windows.

On Debian: `sudo apt install octave octave-signal`.

On mac: `brew install octave`.
Run octave and install the packages with:
```
pkg install -forge control signal
```

Filter coefficients can be generated as follows by running the `octave` command
in the same directory as the scripts and this readme file. It will generate automatically 
an include file containing the coefficients.

```
octave:1> pkg load signal
octave:2> f = make_filter (8, 128, 100.3);

f = make_filter(8, 128, 100);
f = make_filter(7, 128, 92);
f = make_filter(6, 128, 92);
f = make_filter(5, 128, 77);
f = make_filter(4, 128, 60);
f = make_filter(3, 128, 50);
f = make_filter(2, 128, 28);
```


[GNU Octave]: https://www.gnu.org/software/octave/