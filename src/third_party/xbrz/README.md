# xBRZ 1.8

Copyright Zenju, distributed under GPL version 3 ([License.txt](License.txt)) with
the linking exception stated in the source files. Source files retain their original notices.

Vendored unchanged from the xbrzscale mirror, commit
`ef36908831c058df7a202c8175d575a1fdfaf156`:
https://github.com/atheros/xbrzscale/tree/ef36908831c058df7a202c8175d575a1fdfaf156/xbrz

Original project: https://sourceforge.net/projects/xbrz/

`xbrz_c.h` and `xbrz_c.cpp` are DM2Mac's adapter, not upstream files. They expose
2x, 3x and 4x opaque RGB scaling to C, replicate screen borders, restore opaque
alpha after blending, and contain C++ exceptions. The first use of the upstream
RGB distance table allocates about 64 MiB; it is reused for subsequent frames.
