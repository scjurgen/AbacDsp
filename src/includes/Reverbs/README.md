# Reverbs

This section contains various Hadamard based FDN reverbs.

There are many modules found here, some as reference, intermediate versions for block based processing and optimized for SIMD.


## What to consider

The hadamard walsh algorithm works only with 2^N matrices. We have here also implementations for 12, 20, 24, 28. 
However, these are much slower than the walsh algorithm. The FdnReverb.h use this but is very CPU hungry.

## What is missing

- Velvet noise, velvet scattering.
- Nested FDN (e.g. FDN 4x4 inside an FDN 8x8?)

