# Asio (standalone)

- Version: 1.38.2 (tag `asio-1-38-2`, commit 8806a68)
- Source: https://github.com/chriskohlhoff/asio (`asio/include`)
- License: Boost Software License 1.0 (`LICENSE_1_0.txt`)
- Contents: unmodified headers only (`include/asio.hpp`, `include/asio/`)

Used privately by the Pothos library (`lib/Net/`) through the `pothos_asio`
CMake target; never installed and never included from `include/Pothos/`.
To update: replace `include/` with the headers of a new release and update
the version above.
