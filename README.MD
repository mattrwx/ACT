# ACT (Anti Cheat Telemetry)

ACT is a mock anti-cheat designed to provide vulnerability researchers with a safe and legal environment to practice bypassing anti-cheat detection vectors. Its goal is to act as a stepping stone before working with full production anti-cheat systems, allowing researchers to learn and experiment without violating terms of service, risking account bans, or impacting the competitive integrity of games.

# Usage

If you downloaded the installer from the [Releases](https://github.com/mattrwx/ACT/releases) page, the loader will typically be located at:

`C:\Program Files\AntiCheatTelemetry\`

If you would rather compile the project yourself, see the [Build with CMake](#build-with-cmake) section below. After building, the binaries will typically be located in:

`build\bin\`

# Build with CMake

```bash
git clone --recurse-submodules https://github.com/mattrwx/ACT
cd ACT
mkdir build
cmake -B build
cmake --build build
```
