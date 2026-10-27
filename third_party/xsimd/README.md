# xsimd

- Version: 14.3.0 (tag `14.3.0`, commit e88a728, released 2026-07-15)
- Source: https://github.com/xtensor-stack/xsimd (`include/`)
- License: BSD-3-Clause (`LICENSE`)
- Contents: unmodified headers only (`include/xsimd/`)

Used privately by the Pothos library (CPU feature detection, SIMD buffer
conversion) through the `pothos_xsimd` CMake target; never installed and
never included from `include/Pothos/`. Toolkits find their own xsimd.
To update: replace `include/` with the headers of a new release and update
the version above.
