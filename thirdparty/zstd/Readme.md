# zstd (decompressor only)

The Zstandard decompressor, amalgamated into a single C file for easy embedding. It is used by
the read-only RVZ disc image reader (`src/rvz.cpp`); the compressor is not included.

Source: https://github.com/facebook/zstd (tag `v1.5.7`), files
`build/single_file_libs/zstddeclib.c`, `lib/zstd.h` and `lib/zstd_errors.h`.

To re-generate `zstddeclib.c` from the upstream source tree:

```
cd zstd/build/single_file_libs
./create_single_file_decoder.sh      # writes zstddeclib.c
```

License: BSD-3-Clause OR GPL-2.0 (see `LICENSE`); this project uses it under the BSD-3-Clause
option.
