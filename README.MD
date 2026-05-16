# ACT (Anti Cheat Telemetry)

ACT is a mock anti-cheat designed to provide vulnerability researchers with a safe and legal environment to practice bypassing anti-cheat detection vectors. Its goal is to act as a stepping stone before working with full production anti-cheat systems, allowing researchers to learn and experiment without violating terms of service, risking account bans, or impacting the competitive integrity of games.

## Usage

If you downloaded the installer from the [Releases](https://github.com/mattrwx/ACT/releases) page, the loader will typically be located at: `C:\Program Files\AntiCheatTelemetry\`

If you would rather compile the project yourself, see the [Build with CMake](#build-with-cmake) section below. After building, the binaries will typically be located in: `build\bin\`

## Flags
*Note: There can be multiple detection routines that trigger the same console message*
- Failed to remove topmost flag from overlay window
- Failed to retrieve module path for overlay process
- Failed to open handle to overlay process
- Failed to create thread snapshot
- Failed to open first thread from snapshot
- Failed to open thread handle
- NtQueryInformationThread call failed
- Thread start address outside valid module range
- Exception handler chain has been tampered
- Hook detected on Wow64PrepareForExecution
- Failed to locate KiUserExceptionDispatcher
- Failed to locate ntdll
- RIP outside valid executable region during exception
- Executable page found outside known modules
- Untrusted module loaded into process
- PE section count mismatch detected
- PE section hash mismatch detected
- PE section memory protection modified
- Failed to open module file on disk
- VTable hook detected

## Build with CMake
```bash
git clone --recurse-submodules https://github.com/mattrwx/ACT
cd ACT
mkdir build
cmake -B build
cmake --build build
```
