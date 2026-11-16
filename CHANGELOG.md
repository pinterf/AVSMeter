# Changelog

Versions now have two parts (4.0, 4.1, ...). Releases up to 3.0.9.0 by
Groucho2004 are listed in the original [doc/history.txt](doc/history.txt).

## v4.0 (20261116)

First release of the continuation by Ferenc Pintér (pinterf), after 3.0.9.0
(2021).

### Compatibility changes

- Windows XP and Vista are no longer supported; Windows 7 is the minimum.
  libcpuid uses Windows 7 APIs since 0.6.0, and the last toolset that targets
  Windows 7 is MSVC 14.44. On XP use AVSMeter 3.0.9.0.
- INI, log, CSV and `avsinfo_<arch>.log` files are written in UTF-8 (no BOM).
  INI files in the ANSI code page are still read, but older AVSMeter versions
  misread non-ASCII paths in a file written by 4.0.

### New

- Linux (x86_64, aarch64) with AviSynth+ (`libavisynth.so`), Windows on ARM64
  (llvm-mingw build, untested on hardware).
- Audio benchmarking: `-audio`, `-av`, `-chunk`; speed relative to realtime,
  channel layout in the clip info.
- GPU sensors on Linux: NVIDIA through NVML; AMD (amdgpu) and Intel (i915, xe)
  experimental. `-gpu=sel` selects the GPU.
- ARM CPUs: features (NEON, SVE, SME, ...) and big.LITTLE core types.

### Improvements

- International text: command line, script paths, registry, plugin paths and
  messages are Unicode (UTF-8) throughout; non-ASCII script and plugin paths
  are passed to AviSynth+ with `utf8=true` (r2487+ / 3.7.6+).
- `-avsinfo` follows AviSynth+ plugin loading:
  - autoload directories in AviSynth+'s actual search order (`ListAutoloadDirs`);
  - plugins shadowed by an earlier one with the same name are marked;
  - a plugin that breaks autoloading no longer aborts the report;
  - the autoload runs once and is reported on its own; each plugin is then
    test-loaded alone (no autoload in the test environments), so a load error
    is reported against the plugin that caused it, and the test is faster;
  - C-Interface plugins classified by `avisynth_c_plugin_init` / `init2`;
  - plugins built for another CPU (e.g. ARM64 DLLs) are named, not loaded;
  - on ARM64, C++ plugins built by MSVC are flagged (ABI mismatch).
- Crash reports name the module that actually faulted, also when AviSynth
  rethrows a Prefetch worker's fault on the main thread; long module paths.
- OS detection: Windows 11 and Server 2016–2025 by build number, feature update
  and revision (e.g. "Windows 11 (x64) 26H2 (Build 26300.9974)"); Linux from
  `os-release`.
- The frame timer no longer pins the benchmark thread to CPU 0 (a workaround
  for unsynchronized TSCs on old hardware); timing uses a monotonic clock.
- libcpuid 0.8.2 (2026-09-13, was 0.5.1): current Intel and AMD CPUs.
- AviSynth+ headers updated to interface 14.1; older AviSynth versions still
  work. The interface version includes the bugfix level from V9 on (e.g. 14.1).
- `-hp` warns when the priority cannot be raised.
- An INI file that cannot be created is a warning; the defaults are used.

### Fixes

- GPU power consumption average was 0.5 W too high.
- Timing broke after 49.7 days of uptime (GetTickCount wrap).
- GPU-Z shared memory was not unmapped.
- Colorspace "n/a" was misaligned for clips without video.

### Build

- C++17, Visual Studio solution (v143) and CMake presets (MSVC, gcc, clang,
  llvm-mingw, aarch64 cross); characterization tests.
- Source history of all public releases since 1.21 imported to Git; see
  [doc/SOURCE-HISTORY.md](doc/SOURCE-HISTORY.md).

This version was developed with heavy AI assistance (Anthropic Claude Opus 5.5,
OpenAI GPT models, GitHub Copilot), directed, reviewed and tested by Ferenc
Pintér (pinterf).
