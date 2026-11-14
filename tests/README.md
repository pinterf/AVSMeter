# Characterization tests

Freeze the observable behaviour of AVSMeter (console output, exit code, log, CSV,
written INI) before refactoring/porting. Golden files are normalized: versions,
paths, timings, FPS/CPU/memory values and machine-specific info are masked;
row counts of performance data are kept.

```powershell
pwsh tests\run-characterization.ps1                                   # x64 Release
pwsh tests\run-characterization.ps1 -Exe msvc\bin\Win32\Release\AVSMeter.exe
pwsh tests\run-characterization.ps1 -Case video_log_csv,syntax_error -KeepWork
pwsh tests\run-characterization.ps1 -UpdateGolden                     # after an intended change
```

Requirements: AviSynth+ installed for the tested architecture. Each case runs in
its own temp directory with a fixed INI (no color, no pause, no single-instance).

`scripts\` holds the fixtures; `golden\` the expected normalized output.
Behaviour quirks are recorded as-is (e.g. no-argument run exits with -1,
a video-only clip reports "Audio channels: 1", no CSV for runs < 500 ms).
