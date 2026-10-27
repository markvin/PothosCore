# Blocks toolkit

General purpose processing blocks, built with the Pothos core
(option `POTHOS_BUILD_BLOCKS`, ON). Formerly the separate project
https://github.com/pothosware/PothosBlocks; its history is kept here.

Modules (each a directory, registered with `pothos_blocks_module()` in
`CMakeLists.txt`): event, file, network, packet, serialize, stream, testers.
The blocks use the core's vendored dependencies (nlohmann/json, xsimd,
Asio through `lib/Net`) and its internal header-only helpers (`lib/`);
`common/` holds the helpers shared by the modules.

The topology of the core inserts `/blocks/copier` (domain crossing) and
`/blocks/network_sink` / `/blocks/network_source` (flows across processes).

## Tests

Every module lists its self-tests (`TESTS`); CTest runs them one by one in
`pothos.blocks.selftests` and with all others in `pothos.selftests`.

## Licensing information

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt). The SIMD kernels in `stream/SIMD`
are BSD-3-Clause, as marked in their headers.
