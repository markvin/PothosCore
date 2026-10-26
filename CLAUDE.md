# CLAUDE.md

Project: PothosCore — local copy of the fork `git@github.com:markvin/PothosCore.git`
(upstream: https://github.com/pothosware/PothosCore).

## Goal

Restore / update the project so it builds and works on a modern toolchain.

## Repository layout

- `lib/`, `include/` — core framework
- Git submodules: spuce, muparserx, poco, plotters, widgets, flow, soapy,
  blocks, python, audio, comms, libsimdpp

## Build

<!-- TODO -->

## Conventions

<!-- TODO -->

## Notes

- Remotes: `origin` = fork (markvin), `upstream` = pothosware/PothosCore.
- spuce: the URL in `.gitmodules` (audiofilter/spuce) is dead — the account was
  renamed and the repo is no longer public. The recorded commit `ad08df7` is
  available in the fork https://github.com/hayguen/spuce.git (one-off clone:
  `git -c submodule.spuce.url=https://github.com/hayguen/spuce.git submodule update --init spuce`).
- Nested submodules poco/gradle and poco/openssl are not initialized (not needed
  for a Linux build); plotters/qwt6 is initialized.
