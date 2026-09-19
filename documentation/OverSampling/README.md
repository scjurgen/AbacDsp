# OverSampling verification plots

Checks `UpDownSampler` (`src/includes/SamplerateConverter/`) wrapping `WobbleDelay`
(`src/includes/Delays/`): the delay with Wow and Flutter runs at `hostRate * ratio`, and the
ratio is a live parameter. Ratio above 1 is oversampling (quality), below 1 is undersampling
(cheaper, or deliberately lo-fi). The same composition gives a transport-speed effect, because the
wrapped delay time is fixed in internal samples and so changes in host time with the ratio.

```cpp
using Delay = AbacDsp::WobbleDelay<48000, 16>;
AbacDsp::UpDownSampler<Delay, 16> sampler(maxBlockSize, hostRate * ratio);
sampler.setRatio(ratio);
sampler.processor().setDelay(500.f * ratio, true);
sampler.processBlock(source, target);
```

The wrapped delay is constructed with the internal rate, and its delay values are internal
samples. With a ratio that changes while running it keeps the construction-time rate, so Wow and
Flutter run faster or slower in host time by the ratio, like a transport.

`./generate.sh` builds `OverSamplingExplore.cpp`, runs it, and renders every PNG here.
Intermediate data (spectrogram grids, plot series, `.wav` renders for listening) goes into the
gitignored `generated/`.

## What the plots show

![Swept tone through UpDownSampler<WobbleDelay> at five ratios](os_sweep.png)

Swept tone. A sine sweep from 50 Hz to 20 kHz at 48 kHz passes through the composition with wow
(1.5 Hz) and flutter (8 Hz) engaged, at ratios 0.99, 1.0, 0.5 (undersampling), 2.0 and 4.0
(oversampling). Every panel shows one ridge with modulation sidebands around it and no mirrored or
aliased ridge. At ratio 0.5 the internal rate is 24 kHz, and the ridge stops at the internal
Nyquist of 12 kHz instead of reflecting back down: the down-conversion filter removes what the
lower rate can not carry.

![Impulse arrival in host samples against ratio](os_delaytime.png)

Delay time. An impulse passes through the composition with the wrapped delay fixed at 800 internal
samples. The arrival in host samples is that delay divided by the ratio, plus the latency of the
converters and the re-blocker. That latency is measured as the difference:

| ratio | arrival | delay / ratio | converter latency |
|---:|---:|---:|---:|
| 0.125 | 6824 | 6400 | 424 |
| 0.25 | 3420 | 3200 | 220 |
| 0.5 | 1718 | 1600 | 118 |
| 1.0 | 868 | 800 | 68 |
| 2.0 | 459 | 400 | 59 |
| 4.0 | 255 | 200 | 55 |
| 8.0 | 153 | 100 | 53 |
| 16.0 | 102 | 50 | 52 |

The latency is smallest at high ratios and grows steeply below ratio 1, because the sinc kernel
is stretched in proportion to the rate reduction. It does not depend on the host block size, which
the unit test `LatencyDoesNotDependOnBlockSize` pins for blocks of 1 to 512 samples. It is
continuous across ratio 1.0, since the converters always run, also at exactly 1.0.

![Steady tone through an abrupt and a slow ratio change](os_ratiochange.png)

Ratio changes. A steady 1 kHz tone passes through with modulation off, so everything visible is
the ratio change. Top: the ratio jumps from 0.5 to 2.0 at 2 s. The ridge stays at 1 kHz, but a
broadband burst, no wider than one analysis window (85 ms), appears at the jump. The likely cause
is that the up-stage takes the new ratio at once while samples made at the old ratio are still in
flight through the wrapped delay and the re-blocker. Bottom: the ratio moves between 0.75 and 1.25 as a slow sine, and the ridge stays
clean. So a ratio that moves gradually is fine, and a step should be avoided until ratio changes
are smoothed inside `UpDownSampler::setRatio()`, which is marked as a TODO in the header.
