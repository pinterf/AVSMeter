<#
.SYNOPSIS
  Characterization tests for AVSMeter: runs the exe against fixture scripts and
  compares normalized stdout / exit code / log / csv / ini with golden files.

.EXAMPLE
  pwsh tests\run-characterization.ps1
  pwsh tests\run-characterization.ps1 -Exe msvc\bin\x64\Debug\AVSMeter64.exe
  pwsh tests\run-characterization.ps1 -UpdateGolden
#>
[CmdletBinding()]
param(
    [string]$Exe = (Join-Path $PSScriptRoot '..\msvc\bin\x64\Release\AVSMeter64.exe'),
    [string]$GoldenDir = (Join-Path $PSScriptRoot 'golden'),
    [string[]]$Case,
    [switch]$UpdateGolden,
    [switch]$KeepWork
)

$ErrorActionPreference = 'Stop'
$Exe = (Resolve-Path $Exe).Path
$Case = @($Case | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$ScriptDir = Join-Path $PSScriptRoot 'scripts'
$ExeName = [IO.Path]::GetFileName($Exe)
$IniName = [IO.Path]::ChangeExtension($ExeName, '.ini')
$Work = Join-Path ([IO.Path]::GetTempPath()) ("avsm-char-" + [Guid]::NewGuid().ToString('N').Substring(0, 8))
$Latin1 = [Text.Encoding]::Latin1

# Each case: Name, Args (script file names are resolved inside the work dir),
# expected exit code, artifacts to compare (log/csv), and normalization options.
$Cases = @(
    @{ Name = 'video_log_csv';      Args = @('video.avs', '-o', '-log', '-csv'); Exit = 0;  Artifacts = @('video.log', 'video.csv') }
    @{ Name = 'audio_video_log';    Args = @('audio_video.avs', '-o', '-log');   Exit = 0;  Artifacts = @('audio_video.log') }
    @{ Name = 'info';               Args = @('audio_video.avs', '-info');        Exit = 0;  Artifacts = @() }
    @{ Name = 'range_log';          Args = @('video.avs', '-o', '-log', '-range=10,59'); Exit = 0; Artifacts = @('video.log') }
    @{ Name = 'range_invalid';      Args = @('video.avs', '-o', '-range=50,10'); Exit = -1; Artifacts = @() }
    @{ Name = 'timelimit';          Args = @('video.avs', '-o', '-timelimit=1'); Exit = 0;  Artifacts = @(); VolatileFrames = $true }
    @{ Name = 'prescan_log';        Args = @('video.avs', '-log');               Exit = 0;  Artifacts = @('video.log'); VolatileRows = $true }
    @{ Name = 'short_log_csv';      Args = @('short.avs', '-o', '-log', '-csv'); Exit = 0;  Artifacts = @('short.log', 'short.csv') }
    @{ Name = 'syntax_error';       Args = @('syntax_error.avs', '-o');          Exit = -1; Artifacts = @() }
    @{ Name = 'runtime_error';      Args = @('runtime_error.avs', '-o');         Exit = -1; Artifacts = @() }
    @{ Name = 'missing_script';     Args = @('does_not_exist.avs');              Exit = -1; Artifacts = @() }
    @{ Name = 'usage';              Args = @();                                  Exit = -1;  Artifacts = @() }
    @{ Name = 'audio_av_log';       Args = @('audio_video.avs', '-o', '-log', '-av'); Exit = 0; Artifacts = @('audio_video.log') }
    @{ Name = 'audio_only';         Args = @('audio_only.avs', '-o', '-audio');  Exit = 0;  Artifacts = @() }
    @{ Name = 'audio_chunk1';       Args = @('audio_ntsc.avs', '-o', '-audio', '-chunk=1'); Exit = 0; Artifacts = @() }
    @{ Name = 'audio_only_as_video'; Args = @('audio_only.avs', '-o');           Exit = -1; Artifacts = @() }
    @{ Name = 'chunk_without_audio'; Args = @('video.avs', '-chunk=10');         Exit = -1; Artifacts = @() }
    @{ Name = 'audio_long_log_csv'; Args = @('audio_long.avs', '-audio', '-chunk=1', '-log', '-csv'); Exit = 0; Artifacts = @('audio_long.log', 'audio_long.csv'); VolatileRows = $true }
    @{ Name = 'audio_8ch_chunk';    Args = @('audio_8ch.avs', '-o', '-audio', '-chunk=333'); Exit = 0; Artifacts = @() }
    @{ Name = 'audio_prefetch_log'; Args = @('audio_prefetch.avs', '-o', '-av', '-log'); Exit = 0; Artifacts = @('audio_prefetch.log') }
    @{ Name = 'avsinfo';            Args = @('-avsinfo');                        Exit = 0;  Artifacts = @(); LabelsOnly = $true }
)

$TestIni = @"
DisplayFPS=1
DisplayTPF=0

CreateLog=0
CreateCSV=0
LogDirectory=
LogFileDateTimeSuffix=0
AVSDLL=

AllowOnlyOneInstance=0
ConUseSTDOUT=1
UseColor=0
PauseBeforeExit=0

DisableFFTWDLLWarning=1
InvokeDistributor=1
LogEstimatedTime=0
AutoCompleteExtension=1
LoadPluginInterval=40
"@

# --- normalization ---------------------------------------------------------

function ConvertTo-Lines([string]$Text) {
    # Emulate the console: '\r' returns to column 0, the last non-blank segment wins.
    foreach ($raw in ($Text -replace "`r`n", "`n") -split "`n") {
        $segs = $raw -split "`r" | Where-Object { $_.Trim() -ne '' }
        if ($segs) { (@($segs)[-1]).TrimEnd() } else { '' }
    }
}

function Compress-Blank([string[]]$Lines) {
    $out = [Collections.Generic.List[string]]::new()
    foreach ($l in $Lines) {
        if ($l -eq '' -and ($out.Count -eq 0 -or $out[$out.Count - 1] -eq '')) { continue }
        $out.Add($l)
    }
    while ($out.Count -gt 0 -and $out[$out.Count - 1] -eq '') { $out.RemoveAt($out.Count - 1) }
    $out
}

$VolatileValue = @(
    '^((FPS|CPS) \(min \| max \| average\):\s+).*$'
    '^((TPF|TPC) \(min \| max \| average\):\s+).*$'
    '^(Process memory usage \(max\):\s+).*$'
    '^(Thread count:\s+).*$'
    '^(CPU usage \(average\):\s+).*$'
    '^(Time \(elapsed\):\s+).*$'
    '^(Time \(elapsed \| estimated\):\s+).*$'
    '^(Audio speed \(realtime\):\s+).*$'
)

function Format-Common([string[]]$Lines, [hashtable]$C) {
    foreach ($l in $Lines) {
        $l = $l.Replace($Work, '<WORK>')
        $l = $l -replace '^(Log file created with:\s+)?AVSMeter \d+(\.\d+)+ \((x64|x86)\)', '$1AVSMeter <VERSION> (<ARCH>)'
        $l = $l -replace '^AviSynth.*\(\d+(\.\d+)+\)\s*$', '<AVISYNTH VERSION>'
        $l = $l -replace '^Pre-scanning script \(F\d+ / T[\d.]+\)$', 'Pre-scanning script (<V>)'
        foreach ($re in $VolatileValue) { $l = $l -replace $re, '$1<V>' }
        if ($C.VolatileFrames) { $l = $l -replace '^((Frames|Chunks|Audio samples) processed:\s+).*$', '$1<V>' }
        $l
    }
}

function Format-Stdout([string]$Text, [hashtable]$C) {
    $lines = @(ConvertTo-Lines $Text)
    if ($C.LabelsOnly) {
        # -avsinfo lists installed plugins; keep only stable key fields.
        $lines = @($lines | Where-Object { $_ -match '^AVSMeter ' } | Select-Object -First 1) + ($lines | Where-Object { $_ -match '^(VersionString|Interface Version|Avisynth\.dll location):' } |
            ForEach-Object { "$($Matches[1]):" })
    }
    # Drop the live progress block (first progress line .. last "Press 'Esc'" line).
    $first = -1; $last = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($first -lt 0 -and $lines[$i] -match '^(Frame|Chunk) \(current \| last\):') { $first = $i }
        if ($lines[$i] -like "Press 'Esc'*") { $last = $i }
    }
    if ($first -ge 0) {
        $end = [Math]::Max($first, $last)
        $lines = @($lines[0..($first - 1)]) + '<PROGRESS>' + @(if ($end + 1 -lt $lines.Count) { $lines[($end + 1)..($lines.Count - 1)] })
    }
    $lines = $lines | Where-Object { $_ -ne 'Query Avisynth info...' }
    Compress-Blank @(Format-Common $lines $C)
}

function Format-Log([string]$Text, [hashtable]$C) {
    $out = [Collections.Generic.List[string]]::new()
    $section = ''; $rows = 0
    $flush = { if ($rows -gt 0) { $out.Add($(if ($C.VolatileRows) { '<ROWS>' } else { "<ROWS: $rows>" })); Set-Variable rows 0 -Scope 1 } }
    foreach ($l in (ConvertTo-Lines $Text)) {
        if ($l -match '^\[.+\]$') { & $flush; $section = $l; $out.Add($l); continue }
        switch ($section) {
            { $_ -in '[OS/Hardware info]', '[Avisynth info]' } {
                # Machine specific; only the presence of the key fields is checked.
                if ($l -match '^(Operating system|CPU|VersionString|Interface Version|Avisynth\.dll location):') { $out.Add("$($Matches[1]):") }
            }
            '[Performance data]' {
                if ($l -match '^\s*\d+(\s+[\d.]+)+\s*$') {
                    if ($l.Trim() -split '\s+' | Where-Object { $_ -notmatch '^\d+(\.\d+)?$' }) { $out.Add("BAD ROW: $l") }
                    $rows++
                } else { & $flush; $out.Add($l) }
            }
            default { $out.Add($l) }
        }
    }
    & $flush
    Compress-Blank @(Format-Common $out $C)
}

function Format-Csv([string]$Text, [hashtable]$C) {
    $lines = @(ConvertTo-Lines $Text | Where-Object { $_ -ne '' })
    if ($lines.Count -eq 0) { return @('<EMPTY>') }
    $cols = ($lines[0] -split ',').Count
    $bad = @($lines | Select-Object -Skip 1 | Where-Object { ($_ -split ',').Count -ne $cols -or $_ -notmatch '^[\d.,]+$' })
    @($lines[0], $(if ($C.VolatileRows) { '<ROWS>' } else { "<ROWS: $($lines.Count - 1)>" })) + ($bad | ForEach-Object { "BAD ROW: $_" })
}

# --- runner ----------------------------------------------------------------

New-Item -ItemType Directory -Force $Work, $GoldenDir | Out-Null
$failed = 0; $passed = 0

function Compare-Golden([string]$Name, [string[]]$Actual) {
    $file = Join-Path $GoldenDir $Name
    $text = ($Actual -join "`n") + "`n"
    if ($UpdateGolden) {
        [IO.File]::WriteAllText($file, $text)
        return $true
    }
    if (-not (Test-Path $file)) { Write-Host "    missing golden: $Name" -ForegroundColor Yellow; return $false }
    $expected = ([IO.File]::ReadAllText($file)) -replace "`r`n", "`n"
    if ($expected -eq $text) { return $true }
    Write-Host "    mismatch: $Name" -ForegroundColor Red
    $diff = Compare-Object ($expected -split "`n") ($text -split "`n") -SyncWindow 5
    $diff | Select-Object -First 20 | ForEach-Object { Write-Host ("      {0} {1}" -f $(if ($_.SideIndicator -eq '<=') { '-' } else { '+' }), $_.InputObject) }
    return $false
}

try {
    foreach ($c in $Cases) {
        if ($Case -and $c.Name -notin $Case) { continue }
        $cw = Join-Path $Work $c.Name
        New-Item -ItemType Directory -Force $cw | Out-Null
        Copy-Item $Exe $cw
        Copy-Item (Join-Path $ScriptDir '*.avs') $cw
        [IO.File]::WriteAllText((Join-Path $cw $IniName), ($TestIni -replace "`r`n", "`n"))

        $psi = [Diagnostics.ProcessStartInfo]::new((Join-Path $cw $ExeName))
        foreach ($a in $c.Args) { $psi.ArgumentList.Add($a) }
        $psi.WorkingDirectory = $cw
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.RedirectStandardInput = $true
        $psi.StandardOutputEncoding = $Latin1
        $psi.StandardErrorEncoding = $Latin1
        $p = [Diagnostics.Process]::Start($psi)
        $p.StandardInput.Close()
        $errTask = $p.StandardError.ReadToEndAsync()
        $stdout = $p.StandardOutput.ReadToEnd()
        if (-not $p.WaitForExit(120000)) { $p.Kill(); throw "$($c.Name): timeout" }
        $stderr = $errTask.Result
        $exit = $p.ExitCode
        $p.Dispose()

        $ok = $true
        if ($exit -ne $c.Exit) { Write-Host "    exit code $exit, expected $($c.Exit)" -ForegroundColor Red; $ok = $false }
        $ok = (Compare-Golden "$($c.Name).stdout.txt" (Format-Stdout ($stdout + $stderr) $c)) -and $ok
        foreach ($art in $c.Artifacts) {
            $path = Join-Path $cw $art
            $text = if (Test-Path $path) { [IO.File]::ReadAllText($path, $Latin1) } else { $null }
            $norm = if ($null -eq $text) { @('<MISSING>') }
                    elseif ($art -like '*.csv') { Format-Csv $text $c }
                    else { Format-Log $text $c }
            $ok = (Compare-Golden "$($c.Name).$art.txt" $norm) -and $ok
        }
        $ok = (Compare-Golden "$($c.Name).ini.txt" (ConvertTo-Lines ([IO.File]::ReadAllText((Join-Path $cw $IniName), $Latin1)))) -and $ok

        if ($ok) { $passed++; Write-Host "PASS $($c.Name)" -ForegroundColor Green }
        else { $failed++; Write-Host "FAIL $($c.Name)  (work dir: $cw)" -ForegroundColor Red }
    }
}
finally {
    if (-not $KeepWork -and $failed -eq 0) { Remove-Item -Recurse -Force $Work -ErrorAction SilentlyContinue }
}

$verb = if ($UpdateGolden) { 'updated' } else { 'passed' }
Write-Host "`n$passed $verb, $failed failed"
exit [int]($failed -gt 0)
