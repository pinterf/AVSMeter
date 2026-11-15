# AVSMeter

AVSMeter is a Windows command-line tool for benchmarking AviSynth scripts and
diagnosing AviSynth installations and plugins. Originally developed by
**Groucho2004**, it measures how quickly a script delivers video frames without
encoding them, helping identify slow filters, bottlenecks, and crashes.

It reports clip information, minimum/maximum/average processing speed, CPU and
memory usage, and optional GPU/VPU sensor data. It can inspect the installed
AviSynth environment and plugin dependencies, and export logs and CSV data.

## History and resurrection

The preserved releases span **2012 to 2021**, from version 1.21 to
**3.0.9.0**. The author's Doom9 announcement for the latest release was last
updated on **2021-03-31**. This release history does not establish the project's
exact start or final development date.

The original project had **no Git or SVN repository**. Groucho2004 packaged the
executables and sources into release archives, uploaded them to his MediaFire
folder, and announced releases on Doom9.

This continuation by [pinterf](https://github.com/pinterf) began on
**2026-11-13**. This repository's historical sequence was **rebuilt from the
public releases' sources**, replaying the packaged snapshots in release order.
It is a reconstruction of release history, not the author's development history.

Of 125 collected archives, 123 contain source. Each source-bearing release has
an import commit and an annotated release tag containing release notes
and provenance. Versions 2.7.6 and 2.7.7 contain no source and have no invented
source tags. Tagged snapshots preserve original paths, file bytes, bundled
headers, and notices; distributed `.exe`, `.lib`, and `.exp` files are deliberately
excluded.

Historical commits credit Groucho2004 as Author and the actual importer as
Committer. The author's email is unknown; `groucho2004@users.invalid` is an
explicit, reserved-domain placeholder, not a verified contact or GitHub account.
See [SOURCE-HISTORY.md](SOURCE-HISTORY.md) for the release manifest, hashes,
attribution policy, and known gaps.

**Current status:** historical imports are complete. The working tree now has a
Visual Studio solution (`.slnx`, VS2022 17.13+ or VS2026) using **v143**, with matching libcpuid 0.8.2 headers
and prebuilt static libraries. Debug and Release builds for x86/x64 run on the
development machine. XP build support has been dropped: libcpuid requires
Windows 7 or later since 0.6.0 (September 2022), which introduced the processor
group APIs (`GetActiveProcessorCount`, `GetActiveProcessorGroupCount`,
`SetThreadGroupAffinity`); the bundled version is 0.8.2. libcpuid 0.5.1 did not
use them. In addition, the prebuilt libcpuid 0.8.2 libraries could not be linked
with the `v141_xp` linker (IL mismatch), and the maintainer (pinterf) did not
want to build libcpuid from source.
Audio benchmarking remains planned. A Linux (x86_64) CMake build compiles and
links, but it is work in progress: several platform modules are still stubs, so
it cannot benchmark yet. Audio metadata reporting
is already present; processing audio samples for benchmarking is not.

## Links and documentation

- [Original Doom9 announcement and discussion](https://forum.doom9.org/showthread.php?t=174797)
- [Author's MediaFire download folder](https://www.mediafire.com/folder/x6f7yqjufdg7c/Groucho%27s_Avisynth_Stuff)
- [Original manual](doc/AVSMeter.html) and [original changelog](doc/history.txt)
- [AviSynth+](https://github.com/AviSynth/AviSynthPlus)
- [GPU-Z](https://www.techpowerup.com/download/gpu-z/)
- [libcpuid](https://github.com/anrieff/libcpuid)

The original HTML manual can be opened locally in a browser. Its changelog is
preserved as supplied, but later releases pruned old entries; the latest
file alone is not the complete historical record.

## Running AVSMeter

Historical Windows builds were distributed through the author's MediaFire
folder linked above. This continuation does not yet publish binary releases;
local builds are described below.

```text
AVSMeter.exe script.avs -log
AVSMeter.exe -avsinfo -log
AVSMeter64.exe script.avs -csv
AVSMeter64.exe script.avs -gpu
```

Use the executable matching the **AviSynth DLL and plugins' architecture**:
`AVSMeter.exe` is x86; `AVSMeter64.exe` is x64. A 64-bit Windows installation
can run the x86 executable, but that process still needs x86 AviSynth/plugins.

The original author specifies AviSynth 2.6.x or AviSynth+. By default AVSMeter
loads `avisynth.dll` through Windows DLL search rules. The `AVSDLL` entry in
`AVSMeter.ini` / `AVSMeter64.ini` can select a fully qualified DLL path. AviSynth
and individual script plugins may require additional runtime DLLs of their own.

GPU monitoring is optional: start GPU-Z before using `-gpu`. AVSMeter reads
GPU-Z's `GPUZShMem` shared-memory interface. **There is no GPU library to copy
or link**, and AVSMeter itself does not require a CUDA/OpenCL SDK. Filters used
by a script can have separate GPU requirements.

The author's Doom9 announcement recommends administrative rights for full diagnostic
functionality. Be aware that script/plugin checks load third-party DLLs; use
trusted scripts and plugins, and elevate only when necessary.

### Operating systems

The imported implementation is **Windows-only, x86 and x64**. POSIX support in
some bundled AviSynth headers does not make AVSMeter itself portable.

Static inspection of the original 3.0.9.0 executables found these PE targets:

| Executable | Architecture | PE subsystem version | Historical target |
|---|---|---|---|
| `AVSMeter.exe` | x86 | 5.1 | Windows XP generation |
| `AVSMeter64.exe` | x64 | 5.2 | Windows XP x64 / Server 2003 x64 generation |

The makefiles also define `WINVER` / `_WIN32_WINNT` as `0x0501`. These establish
historical targeting intent, **not verified minimum service packs or a tested
compatibility matrix**. For current builds, see [Windows targeting](#windows-targeting).

### Executable dependencies

Both original 3.0.9.0 executables directly import only Windows system DLLs:
`KERNEL32.dll`, `USER32.dll`, `ADVAPI32.dll`, `PSAPI.dll`, `imagehlp.dll`, and
`VERSION.dll`. No delay-import dependencies were found.

There are no direct MSVC runtime, libcpuid, or AviSynth DLL imports. Together with
the makefiles' `/MT` setting, this is consistent with a statically linked C/C++
runtime and libcpuid: the original executables do not directly require a separate
Visual C++ Redistributable. **This does not mean they have no dependencies:**
AviSynth is loaded dynamically with `LoadLibraryEx` / `GetProcAddress`, GPU-Z is
needed for `-gpu`, and plugins can require their own redistributables or DLLs.
New builds may have different dependencies.

## Building with Visual Studio 2022 / 2026

Open **`msvc\AVSMeter.slnx`**, select `Release` and `Win32` or `x64`, then
**Build Solution**. All four `Debug` / `Release` configurations use **v143**
and the downloaded static libcpuid libraries. There is no separate dependency
project, external source checkout, or XP-toolset configuration.

All four configurations were built and each processed a 64-frame `BlankClip`
script with the installed AviSynth+ 3.7.6. This is a build/smoke-test baseline, not a performance result
or legacy Windows compatibility certification.

### Prerequisites

Install Visual Studio 2022 or its Build Tools with **Desktop development with
C++**, the **MSVC v143 x86/x64 tools**, and a **Windows 10/11 SDK**, including
`rc.exe` and `winres.h`. No XP support components or Windows 7.1A SDK are needed.
Compiler downloads are available from
[Microsoft](https://visualstudio.microsoft.com/downloads/).

Download the official libcpuid 0.8.2
[x86](https://github.com/anrieff/libcpuid/releases/download/v0.8.2/libcpuid-0.8.2-windows-ix86.zip)
and [x64](https://github.com/anrieff/libcpuid/releases/download/v0.8.2/libcpuid-0.8.2-windows-x86_64.zip)
packages and extract their static libraries to `source\cpuid`:

| Package | Library inside ZIP | Destination filename |
|---|---|---|
| x86 | `lib\Release\libcpuid.lib` | `libcpuid32.lib` |
| x64 | `lib\Release\libcpuid.lib` | `libcpuid64.lib` |
| x86 | `lib\Debug\libcpuid.lib` | `libcpuid32d.lib` |
| x64 | `lib\Debug\libcpuid.lib` | `libcpuid64d.lib` |

Release libraries use `/MT`; Debug libraries use `/MTd`. Do not select the
`ReleaseDLL` / `DebugDLL` import libraries. Matching public headers are already
under `source\cpuid`; their package locations are described below. All four
libraries are already present locally and ignored by Git.

### Outputs and command-line builds

Release outputs:

```text
msvc\bin\Win32\Release\AVSMeter.exe
msvc\bin\x64\Release\AVSMeter64.exe
```

Debug outputs use the corresponding `Debug` directory; intermediate files are
under `msvc\obj`. Release uses `/MT`, `/EHa`, and `/O2`; Debug uses `/MTd`,
`/EHa`, and `/Od`. Both use **Whole Program Optimization = No (`/GL-`)** and
**Link Time Code Generation = Default**. The linker automatically enables LTCG
for the Release library's own `/GL` objects, but AVSMeter is not compiled to
LTCG intermediate code.

From a VS2022/VS2026 Developer Command Prompt, at the repository root:

```bat
msbuild msvc\AVSMeter.slnx /m /p:Configuration=Release /p:Platform=Win32
msbuild msvc\AVSMeter.slnx /m /p:Configuration=Release /p:Platform=x64
msbuild msvc\AVSMeter.slnx /m /p:Configuration=Debug /p:Platform=x64
```

Use `Configuration=Debug` for development builds. The linker can warn about
missing libcpuid debugging information (LNK4099/LNK4204): the official binary
ZIPs contain no PDB files. This limits
libcpuid source-level debugging, not executable linking or operation.
The supplied x86 Debug library also requests Edit and Continue; LNK4075 notes
that this is ignored because incremental linking is disabled.

### Windows targeting

Both architectures target Windows 7 (`WINVER` / `_WIN32_WINNT = 0x0601`,
PE subsystem 6.01). XP is not supported. **Not yet tested on Windows 7.**

Build with the **v143 toolset, MSVC 14.44** (VS2022 17.14, also selectable in
VS2026); any 14.44 build number will do. It is the last toolset that targets
Windows 7. From **14.50** (VS2026, v145) the minimum target is Windows 10 /
Server 2016, and the STL calls Windows 8+ APIs such as
`GetSystemTimePreciseAsFileTime` and `WaitOnAddress` unconditionally
([STL #5432](https://github.com/microsoft/STL/pull/5432)). Static CRT linking
does not help: the OS API imports remain.

#### Runtime for AviSynth and plugins

AVSMeter links the CRT statically, but AviSynth and most plugins need the
Visual C++ Redistributable. On Windows 7 SP1 / Server 2008 R2 install:

- [KB2999226](https://support.microsoft.com/en-us/servicing/os/windows/2020/04/update-for-universal-c-runtime-in-windows)
  (Universal C Runtime);
- the **14.44** Redistributable (e.g. 14.44.35211.0) from the VS2022 17.14
  permalinks `https://aka.ms/vs/17/release/vc_redist.x86.exe` / `.x64.exe`,
  or the [abbodi1406 AIO v0.92.0](https://github.com/abbodi1406/vcredist/releases/tag/v0.92.0)
  package. The latest Redistributable (14.50+) does not run on Windows 7.

**Mutex crash:** since 14.40 (VS2022 17.10) `std::mutex` has a `constexpr`
constructor. A DLL built with 14.40 or newer crashes with an access violation
at its first mutex lock if the loaded `msvcp140.dll` is older than 14.40
([STL #4730](https://github.com/microsoft/STL/issues/4730)). An older DLL on a
newer runtime is fine. Either update the runtime (on Windows 7: the 14.44 one),
or rebuild the affected DLL (AviSynth or the plugin, not AVSMeter) with
`/D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR`, which makes it work with older
runtimes too.

To see which runtime is loaded, check the `msvcp140.dll` file version in
`System32` (x64 runtime on x64 Windows), `SysWOW64` (x86 runtime) and next to
the application: an app-local copy is not updated by the Redistributable
installer.

### libcpuid versions, headers, and binary packages

The latest original release's tagged snapshot contains **0.5.1** headers.
The current working tree's `source\cpuid` headers and notice have been updated
to **0.8.2**. All configurations use the downloaded 0.8.2 static libraries,
not Groucho2004's historical libraries.

Matching public headers are available in the
[versioned source directory](https://github.com/anrieff/libcpuid/tree/v0.8.2/libcpuid)
and inside the official [x86](https://github.com/anrieff/libcpuid/releases/download/v0.8.2/libcpuid-0.8.2-windows-ix86.zip)
and [x64](https://github.com/anrieff/libcpuid/releases/download/v0.8.2/libcpuid-0.8.2-windows-x86_64.zip)
binary ZIPs, under `windows-ix86\include` / `windows-x86_64\include`:
`libcpuid.h`, `libcpuid_constants.h`, and `libcpuid_types.h`. All three must be
updated together. Their contents were identical between architectures.

The binary packages' `lib\Release\libcpuid.lib` is a `/MT` static library;
`lib\ReleaseDLL\libcpuid.lib` is a DLL import library and is not interchangeable.
The downloaded static libraries are ignored local inputs at
`source\cpuid\libcpuid32.lib` / `libcpuid64.lib` for Release, and
`libcpuid32d.lib` / `libcpuid64d.lib` for Debug. The Release libraries link
successfully with
**v143 14.44 when AVSMeter uses `/GL-` and the linker setting is Default**.
The earlier `/GL` application builds failed with C1047/LNK1257 because their
LTCG intermediate code differed from the library's. Disabling `/GL` for the
application fixes that tested combination; it does not turn off LTCG inside
the prebuilt library or guarantee compatibility with arbitrary toolchains.

Never combine 0.8.2 libraries with 0.5.1 headers: `CPU_FLAGS_MAX` grew from 128 to
384, and raw-data structures grew too, risking memory corruption. For historical
reproduction instead, start with [libcpuid v0.5.1](https://github.com/anrieff/libcpuid/tree/v0.5.1)
and compare against the original bundled headers; the exact original library
revision was not established. Compiled libraries must not be committed.

### Historical compiler evidence

The supplied NMAKE files use Microsoft's `cl.exe`, `link.exe`, and `rc.exe`.
Static analysis of both original 3.0.9.0 executables found linker version
**10.0** and Rich-header tool records with build **40219**, pointing to
**Visual C++ 2010 SP1 / v100**. This is binary evidence, not a recovered build
log or proof of every linked object's origin. Version 1.21 reports linker 7.10,
consistent with Visual C++ .NET 2003.

VC6 references in old libcpuid filenames/header compatibility code do not
establish AVSMeter 3.0.9.0's compiler. VC++ 6.0 is not the indicated compiler
for that release.

MinGW is not a drop-in replacement. In addition to Windows and MSVC-specific
exception handling, AVSMeter calls the **AviSynth C++ API**; dynamically loading
the DLL does not remove calling-convention, object-layout, or C++ ABI requirements.
Alternative compilers need an explicitly validated combination of AVSMeter,
AviSynth, and dependency builds.

### AviSynth and GPU build inputs

Keep the supplied SDK under `source\avs_headers` for the initial baseline.
**The latest release does not need `avisynth.lib`:** it resolves AviSynth's
entry point dynamically. Some older historical releases did use import
libraries; that is not the latest build's requirement.

GPU-Z is a runtime companion only. Its shared-memory structures are already
declared in `source\GPUInfo.h`; no additional GPU headers or libraries need
to be placed in the tree.

### Historical NMAKE entry points

The original makefiles are retained unchanged and were not validated by the new
solution builds. They link libraries from `source\cpuid` rather than rebuilding
the dependency. With separately compatible inputs, their entry points run from
the **repository root**, not `source`:

```bat
rem In an x86 MSVC command prompt:
nmake /nologo /f source\AVSMeter32.mak

rem Separately, in an x64 MSVC command prompt:
nmake /nologo /f source\AVSMeter64.mak
```

Outputs are intended to be root `AVSMeter.exe` / `AVSMeter64.exe`, with objects
under `Intermediate32` / `Intermediate64`. The original x64 makefile names its
target `AVSMeter.exe` while the linker produces `AVSMeter64.exe`; this known
inconsistency must be corrected before relying on incremental builds. These
commands describe the supplied entry points, not successful builds.

## Building with CMake (Windows, Linux)

CMake 3.22 or later, C++17. Presets are in `CMakePresets.json`:

```sh
# Linux (GCC or Clang, Ninja): build/linux-gcc/avsmeter
cmake --preset linux-gcc && cmake --build --preset linux-gcc
cmake --preset linux-clang && cmake --build --preset linux-clang
```

```bat
rem Windows (Visual Studio 2026): build\vs64\Release\AVSMeter64.exe, build\vs32\Release\AVSMeter.exe
cmake --preset vs64 && cmake --build --preset vs64
cmake --preset vs32 && cmake --build --preset vs32
```

MSVC links the prebuilt libcpuid libraries from `source\cpuid`. Other compilers
download libcpuid 0.8.2 at configure time (Git, network access) and build it as a
static library; `-DAVSMETER_SYSTEM_LIBCPUID=ON` uses the system's libcpuid via
pkg-config instead. Linux needs AviSynth+ (`libavisynth.so`) at run time.

## License

AVSMeter's project sources are licensed under **GPL-3.0-or-later**. See
[LICENSE](LICENSE) and the original source notices. Bundled third-party headers
and libcpuid notices retain their own licensing and interface exceptions.
