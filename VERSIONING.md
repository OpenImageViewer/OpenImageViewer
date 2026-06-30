# OIViewer Versioning and Release Matrix

## Version components

OIViewer uses these version components:

| Symbol | Meaning | Source |
|---|---|---|
| `Major` | Major product version | `OIV_VERSION_MAJOR` in `Version.h` |
| `Minor` | Minor product version | `OIV_VERSION_MINOR` in `Version.h` |
| `Patch` | Manually maintained patch version | `OIV_VERSION_PATCH` in `Version.h` |
| `Revision` | Number of Git commits reachable from the source commit | `git rev-list HEAD --count` |
| `Hash` | At least eight-character abbreviated source commit | `git rev-parse --short=8 HEAD` |

When Git metadata is unavailable, configure with `-DOIV_VERSION_REVISION=<revision>`. Snapshot client builds
also require `-DOIV_GIT_SHORT_HASH=<hash>`; official releases do not consume the abbreviated hash.

The examples below use:

| Placeholder | Example value |
|---|---|
| `Major.Minor.Patch` | `0.18.0` |
| `Revision` | `806` |
| `Hash` | `abcdef12` |
| Platform suffix | `Win32x64LLVM` |

## Primary release matrix

| Property | Snapshot | Official release |
|---|---|---|
| How it is triggered | Automatically after successful release-branch CI, or manually with `official_release` disabled | Manually with `official_release` enabled |
| `OIV_OFFICIAL_BUILD` | `1` | `1` |
| `OIV_OFFICIAL_RELEASE` | `0` | `1` |
| Application version | `0.18.0.806-abcdef12` | `0.18.0` |
| Runtime archive | `OIV-0.18.0.806-abcdef12-Win32x64LLVM.7z` | `OIV-0.18.0-Win32x64LLVM.7z` |
| Symbols archive | `OIV-0.18.0.806-abcdef12-Win32x64LLVM-Symbols.7z` | `OIV-0.18.0-Win32x64LLVM-Symbols.7z` |
| Git tag | `snapshots/OIV-0.18.0.806` | `OIV-0.18.0` |
| GitHub release title | `OIV-0.18.0.806` | `OIV-0.18.0` |
| GitHub release type | Prerelease | Stable release |
| Release target | Exact source commit SHA | Exact source commit SHA |
| Hash in tag | No | No |
| Hash in archive | Yes | No |
| Revision in tag/archive | Yes | No |

## Workflow trigger matrix

| Workflow trigger | Triggering result | Branch or ref | `official_release` | Outcome |
|---|---|---|---:|---|
| Automatic `Build all` push | Success | Configured snapshot branch, default `master` | Not available | Publish snapshot |
| Automatic `Build all` push | Failure or cancellation | Any | Not available | Skip publishing |
| Pull-request `Build all` | Any | Any | Not available | Skip publishing |
| Manually dispatched `Build all` | Any | Any | Not available | Skip publishing |
| Automatic `Build all` push | Success | A branch other than the configured snapshot branch | Not available | Skip publishing |
| Manual release workflow | N/A | Any selected ref | Disabled | Publish snapshot |
| Manual release workflow | N/A | Any selected ref | Enabled | Publish official stable release |

Automatic publishing always uses snapshot mode. An official release must be explicitly requested through the manual workflow checkbox.

## Archive matrix

| Release mode | Archive type | Required filename form | Example |
|---|---|---|---|
| Snapshot | Runtime | `OIV-Major.Minor.Patch.Revision-Hash-Platform.7z` | `OIV-0.18.0.806-abcdef12-Win32x64LLVM.7z` |
| Snapshot | Symbols | `OIV-Major.Minor.Patch.Revision-Hash-Platform-Symbols.7z` | `OIV-0.18.0.806-abcdef12-Win32x64LLVM-Symbols.7z` |
| Official | Runtime | `OIV-Major.Minor.Patch-Platform.7z` | `OIV-0.18.0-Win32x64LLVM.7z` |
| Official | Symbols | `OIV-Major.Minor.Patch-Platform-Symbols.7z` | `OIV-0.18.0-Win32x64LLVM-Symbols.7z` |

The platform suffix is derived from the target system, architecture, and toolchain. It is not part of the product version.

## Application-title matrix

| `OIV_OFFICIAL_RELEASE` | `OIV_OFFICIAL_BUILD` | Application title |
|---:|---:|---|
| `0` | `1` | `OpenImageViewer 0.18.0.806-abcdef12 \| 64 bit \| <binary timestamp>` |
| `0` | `0` | `OpenImageViewer 0.18.0.806-abcdef12 \| 64 bit \| <binary timestamp> \| UNOFFICIAL` |
| `1` | `1` | `OpenImageViewer 0.18.0` |
| `1` | `0` | `OpenImageViewer 0.18.0 \| UNOFFICIAL` |

Additional title rules:

- A non-release build includes the full numeric version, hash, architecture, and binary timestamp.
- An official release includes only `Major.Minor.Patch`.
- `OIV_RELEASE_SUFFIX`, when configured, is appended only to an official-release title.
- `UNOFFICIAL` is controlled independently by `OIV_OFFICIAL_BUILD`.
- The publishing workflow always sets `OIV_OFFICIAL_BUILD` to `1`.

## Internal version-path matrix

Internal data paths always use `Major.Minor.Patch.Revision`, including official builds.

| Consumer | Path form | Example |
|---|---|---|
| Application log | `OIV/Major.Minor.Patch.Revision/oiv.log` | `OIV/0.18.0.806/oiv.log` |
| Renderer data | `OIV/Major.Minor.Patch.Revision/Renderer/D3D11/` | `OIV/0.18.0.806/Renderer/D3D11/` |

This prevents builds from different source revisions from sharing mutable runtime data.

## Git tag and release matrix

| Release mode | Tag form | Release title | Prerelease | Example tag |
|---|---|---|---:|---|
| Snapshot | `snapshots/OIV-Major.Minor.Patch.Revision` | `OIV-Major.Minor.Patch.Revision` | Yes | `snapshots/OIV-0.18.0.806` |
| Official | `OIV-Major.Minor.Patch` | `OIV-Major.Minor.Patch` | No | `OIV-0.18.0` |

The hash is recorded in snapshot archive names and release notes. It is deliberately excluded from both tag forms.

## Existing-tag behavior

| Existing state | Requested source SHA | Result |
|---|---|---|
| Tag and release do not exist | Any valid SHA | Create the tag and release, then upload both archives |
| Tag exists on the same SHA; release does not exist | Same SHA | Create the release for the existing tag, then upload both archives |
| Tag and release exist on the same SHA and have the expected stable/prerelease type | Same SHA | Upload replacement assets, update metadata, then remove obsolete assets |
| Tag exists on another SHA | Different SHA | Fail before modifying the release or its assets |
| Release stable/prerelease state does not match the selected mode | Same SHA | Fail before replacing assets |
| Release token is missing or invalid | Any | Fail publishing |

Official tags therefore remain immutable across source commits. Publishing another official build from a later commit requires incrementing Major, Minor, or Patch.

## Archive validation matrix

| Selected mode | Supplied archives | Result |
|---|---|---|
| Snapshot | One four-part, hash-qualified runtime archive and matching symbols archive | Accept |
| Snapshot | Official three-part archives | Reject |
| Official | One three-part runtime archive and matching symbols archive | Accept |
| Official | Four-part snapshot archives | Reject |
| Either | Runtime and symbols versions differ | Reject |
| Either | Runtime archive is missing or duplicated | Reject |
| Either | Symbols archive is missing or duplicated | Reject |
| Either | Filename does not match the selected release mode | Reject |

## Local `publish.ps1` matrix

`OIV_OFFICIAL_RELEASE` controls application presentation and archive naming. `OIV_OFFICIAL_BUILD` controls only official branding and the `UNOFFICIAL` title marker.

| `OfficialRelease` | `OfficialBuild` | Package version | Title version | `UNOFFICIAL` marker |
|---:|---:|---|---|---:|
| `false` | `true` | `Major.Minor.Patch.Revision-Hash` | `Major.Minor.Patch.Revision-Hash` | No |
| `false` | `false` | `Major.Minor.Patch.Revision-Hash` | `Major.Minor.Patch.Revision-Hash` | Yes |
| `true` | `true` | `Major.Minor.Patch` | `Major.Minor.Patch` | No |
| `true` | `false` | `Major.Minor.Patch` | `Major.Minor.Patch` | Yes |

## Legacy releases

Existing `snapshot` and `snapshot-<SHA>` tags and releases are not migrated or deleted. The new naming scheme applies only to future publications.
