# AVSMeter source history

This repository continues AVSMeter by Groucho2004. The historical commits are
reconstructed imports of release archives. The original project never used Git
or SVN, so there are no original commits.

## Import policy

- Original GitHub bootstrap commit: `9460dfcd5563ef66e3749389ca60df36ef868042`.
- Import setup commit: `6ec861e24c17e7235fe64a84593d636c65e38409`.
- 123 source-bearing releases, preserving original paths and bytes.
- No release `.exe`, prebuilt `.lib`, or linker `.exp` files are included.
- Headers, resources, original docs, license notices, and INI defaults are retained.
- Versions 2.7.6 and 2.7.7 have no source in their archives and have no invented source tags.
- Other missing old files/releases were not sought or reconstructed.
- Publication dates below are VideoHelp mirror metadata, not original development timestamps.
- Import commits and tag objects use actual import timestamps.
- Author: `Groucho2004 <groucho2004@users.invalid>`.
- The author email is a transparent, non-deliverable placeholder. `.invalid` is
  reserved for invalid names by [RFC 2606](https://www.rfc-editor.org/rfc/rfc2606.html#section-2).
  No email or GitHub account for Groucho2004 was verified.

## Sources and licensing

- [Author's announcement](https://forum.doom9.org/showthread.php?t=174797)
- [Author-linked download folder](https://www.mediafire.com/folder/x6f7yqjufdg7c/Groucho%27s_Avisynth_Stuff)
- [Mirror release list](https://www.videohelp.com/software/AVSMeter/old-versions)

The continuation preserves the project's GPL-3.0-or-later notices and the GPLv3
root LICENSE. Third-party files retain their own notices and exceptions.
A root license is not evidence that every old archive carried the same license file.

## Bytes and layout

Only redundant archive wrappers and approved binary exclusions were removed.
`.gitattributes` disables text/filter transformations for historical release paths.
No API updates, source layout modernization, or portability fixes were mixed into imports.
Each tagged release includes persistent root setup files plus the filtered release snapshot.

The current original manual is `doc\AVSMeter.html`, and the original changelog is
`doc\history.txt`. Tag annotations contain per-version notes from their original
packages. Version 1.7.4's notes come retrospectively from 1.7.5; no 1.21 notes were found.

## Build prerequisites: pending baseline work

No build is claimed or tested by this import. Compiled dependencies must be
obtained externally; they must not be committed to Git.

The latest NMAKE definitions expect a configured Windows C++ compiler/linker and
resource compiler, and compatible libcpuid headers/libraries. They refer to
`source\cpuid\libcpuid32.lib` or `source\cpuid\libcpuid64.lib`.
These files are intentionally absent. Historical 2.8.3-2.8.5 builds also used
AviSynth import libraries. Version-compatible download/source links, source-build
steps, architecture choices, and precise placement/configuration instructions
will be documented when establishing the build baseline; no dependency version
or modern-toolchain compatibility is assumed here.

## Release imports

Each release is tagged with the version name shown below; `Archive` is the local download the snapshot was imported from.

| Tag | Mirror date | Archive |
|---|---|---|
| `v1.21` | 2012-07-30 | `AVSMeter1.21.zip` |
| `v1.7.4` | 2014-01-09 | `AVSMeter174.zip` |
| `v1.7.5` | 2014-01-15 | `AVSMeter175.zip` |
| `v1.7.6` | 2014-02-02 | `AVSMeter176.zip` |
| `v1.7.7` | 2014-03-16 | `AVSMeter177.zip` |
| `v1.7.8` | 2014-05-03 | `AVSMeter178.zip` |
| `v1.7.9` | 2014-06-13 | `AVSMeter179.zip` |
| `v1.8.0` | 2014-09-09 | `AVSMeter180.zip` |
| `v1.8.1` | 2014-09-14 | `AVSMeter181.zip` |
| `v1.8.2` | 2014-09-17 | `AVSMeter182.zip` |
| `v1.8.3` | 2014-10-16 | `AVSMeter183.zip` |
| `v1.9.0` | 2014-11-22 | `AVSMeter190.zip` |
| `v1.9.1` | 2014-12-22 | `AVSMeter191.zip` |
| `v1.9.2` | 2015-01-04 | `AVSMeter192.zip` |
| `v1.9.3` | 2015-01-19 | `AVSMeter193.zip` |
| `v1.9.4` | 2015-01-26 | `AVSMeter194.7z` |
| `v1.9.5` | 2015-02-07 | `AVSMeter195.7z` |
| `v1.9.6` | 2015-03-13 | `AVSMeter196.7z` |
| `v1.9.7` | 2015-03-18 | `AVSMeter197.7z` |
| `v1.9.8` | 2015-03-22 | `AVSMeter198.7z` |
| `v2.0.0` | 2015-04-29 | `AVSMeter200.7z` |
| `v2.0.1` | 2015-05-05 | `AVSMeter201.7z` |
| `v2.0.2` | 2015-05-12 | `AVSMeter202.7z` |
| `v2.0.3` | 2015-05-16 | `AVSMeter203.7z` |
| `v2.0.4` | 2015-06-15 | `AVSMeter204.7z` |
| `v2.0.5` | 2015-06-24 | `AVSMeter205.7z` |
| `v2.0.6` | 2015-06-30 | `AVSMeter206.7z` |
| `v2.0.7` | 2015-07-02 | `AVSMeter207.7z` |
| `v2.0.8` | 2015-07-12 | `AVSMeter208.7z` |
| `v2.0.9` | 2015-07-14 | `AVSMeter209.7z` |
| `v2.1.0` | 2015-07-30 | `AVSMeter210.7z` |
| `v2.1.1` | 2015-08-09 | `AVSMeter211.7z` |
| `v2.1.2` | 2015-09-15 | `AVSMeter212.7z` |
| `v2.1.3` | 2015-10-03 | `AVSMeter213.7z` |
| `v2.1.4` | 2015-10-20 | `AVSMeter214.7z` |
| `v2.1.5` | 2015-10-29 | `AVSMeter215.7z` |
| `v2.1.6` | 2015-12-06 | `AVSMeter216.7z` |
| `v2.1.7` | 2016-04-08 | `AVSMeter217.7z` |
| `v2.1.8` | 2016-04-10 | `AVSMeter218.7z` |
| `v2.1.9` | 2016-04-10 | `AVSMeter219.7z` |
| `v2.2.0` | 2016-04-13 | `AVSMeter220.7z` |
| `v2.2.1` | 2016-04-20 | `AVSMeter221.7z` |
| `v2.2.2` | 2016-04-22 | `AVSMeter222.7z` |
| `v2.2.3` | 2016-04-28 | `AVSMeter223.7z` |
| `v2.2.4` | 2016-05-09 | `AVSMeter224.7z` |
| `v2.2.5` | 2016-05-11 | `AVSMeter225.7z` |
| `v2.2.6` | 2016-05-15 | `AVSMeter226.7z` |
| `v2.2.7` | 2016-05-26 | `AVSMeter227.7z` |
| `v2.2.8` | 2016-06-02 | `AVSMeter228.7z` |
| `v2.2.9` | 2016-07-02 | `AVSMeter229.7z` |
| `v2.3.0` | 2016-07-09 | `AVSMeter230.7z` |
| `v2.3.1` | 2016-07-23 | `AVSMeter231.7z` |
| `v2.3.2` | 2016-07-26 | `AVSMeter232.7z` |
| `v2.3.3` | 2016-07-26 | `AVSMeter233.7z` |
| `v2.3.4` | 2016-07-28 | `AVSMeter234.7z` |
| `v2.3.5` | 2016-08-07 | `AVSMeter235.7z` |
| `v2.3.6` | 2016-08-12 | `AVSMeter236.7z` |
| `v2.3.7` | 2016-08-15 | `AVSMeter237.7z` |
| `v2.3.8` | 2016-09-02 | `AVSMeter238.7z` |
| `v2.3.9` | 2016-09-11 | `AVSMeter239.7z` |
| `v2.4.0` | 2016-09-20 | `AVSMeter240.7z` |
| `v2.4.1` | 2016-09-27 | `AVSMeter241.7z` |
| `v2.4.2` | 2016-09-30 | `AVSMeter242.7z` |
| `v2.4.3` | 2016-10-22 | `AVSMeter243.7z` |
| `v2.4.4` | 2016-11-17 | `AVSMeter244.7z` |
| `v2.4.5` | 2016-12-09 | `AVSMeter245.7z` |
| `v2.4.6` | 2016-12-17 | `AVSMeter246.7z` |
| `v2.4.7` | 2017-01-20 | `AVSMeter247.7z` |
| `v2.4.8` | 2017-01-24 | `AVSMeter248.7z` |
| `v2.4.9` | 2017-02-09 | `AVSMeter249.7z` |
| `v2.5.0` | 2017-02-26 | `AVSMeter250.7z` |
| `v2.5.1` | 2017-03-16 | `AVSMeter251.7z` |
| `v2.5.2` | 2017-03-27 | `AVSMeter252.7z` |
| `v2.5.3` | 2017-04-05 | `AVSMeter253.7z` |
| `v2.5.4` | 2017-04-23 | `AVSMeter254.7z` |
| `v2.5.5` | 2017-05-08 | `AVSMeter255.7z` |
| `v2.6.6` | 2017-10-22 | `AVSMeter266.7z` |
| `v2.6.7` | 2017-11-16 | `AVSMeter267.7z` |
| `v2.6.8` | 2017-11-18 | `AVSMeter268.7z` |
| `v2.6.9` | 2017-12-01 | `AVSMeter269.7z` |
| `v2.7.0` | 2017-12-14 | `AVSMeter270.7z` |
| `v2.7.1` | 2018-01-16 | `AVSMeter271.7z` |
| `v2.7.2` | 2018-02-14 | `AVSMeter272.7z` |
| `v2.7.3` | 2018-02-15 | `AVSMeter273.7z` |
| `v2.7.4` | 2018-03-18 | `AVSMeter274.7z` |
| `v2.7.5` | 2018-03-18 | `AVSMeter275.7z` |
| `v2.7.8` | 2018-06-09 | `AVSMeter278.7z` |
| `v2.7.9` | 2018-07-19 | `AVSMeter279.7z` |
| `v2.8.0` | 2018-07-20 | `AVSMeter280.7z` |
| `v2.8.1` | 2018-07-22 | `AVSMeter281.7z` |
| `v2.8.2` | 2018-08-17 | `AVSMeter282.7z` |
| `v2.8.3` | 2018-09-02 | `AVSMeter283.7z` |
| `v2.8.4` | 2018-09-03 | `AVSMeter284.7z` |
| `v2.8.5` | 2018-09-08 | `AVSMeter285.7z` |
| `v2.8.6` | 2018-10-01 | `AVSMeter286.7z` |
| `v2.8.7` | 2018-11-06 | `AVSMeter287.7z` |
| `v2.8.8` | 2019-01-22 | `AVSMeter288.7z` |
| `v2.8.9` | 2019-01-23 | `AVSMeter289.7z` |
| `v2.9.0` | 2019-02-23 | `AVSMeter290.7z` |
| `v2.9.1` | 2019-05-08 | `AVSMeter291.7z` |
| `v2.9.2` | 2019-06-01 | `AVSMeter292.7z` |
| `v2.9.3` | 2019-06-20 | `AVSMeter293.7z` |
| `v2.9.4` | 2019-07-07 | `AVSMeter294.7z` |
| `v2.9.5` | 2019-07-22 | `AVSMeter295.7z` |
| `v2.9.6` | 2019-08-25 | `AVSMeter296.7z` |
| `v2.9.7` | 2019-11-06 | `AVSMeter297.7z` |
| `v2.9.8` | 2020-02-02 | `AVSMeter298.7z` |
| `v2.9.9` | 2020-04-15 | `AVSMeter299.7z` |
| `v2.9.9.1` | 2020-04-17 | `AVSMeter2991.7z` |
| `v2.9.9.2` | 2020-05-12 | `AVSMeter2992.7z` |
| `v2.9.9.3` | 2020-05-22 | `AVSMeter2993.7z` |
| `v3.0.0.0` | 2020-05-23 | `AVSMeter3000.7z` |
| `v3.0.0.3` | 2020-05-25 | `AVSMeter3003.7z` |
| `v3.0.0.4` | 2020-05-26 | `AVSMeter3004.7z` |
| `v3.0.1.0` | 2020-06-13 | `AVSMeter3010.7z` |
| `v3.0.2.0` | 2020-06-23 | `AVSMeter3020.7z` |
| `v3.0.3.0` | 2020-09-12 | `AVSMeter3030.7z` |
| `v3.0.4.0` | 2020-09-14 | `AVSMeter3040.7z` |
| `v3.0.5.0` | 2020-09-19 | `AVSMeter3050.7z` |
| `v3.0.6.0` | 2020-09-22 | `AVSMeter3060.7z` |
| `v3.0.7.0` | 2020-11-16 | `AVSMeter3070.7z` |
| `v3.0.8.0` | 2021-03-09 | `AVSMeter3080.7z` |
| `v3.0.9.0` | 2021-03-31 | `AVSMeter3090.7z` |
