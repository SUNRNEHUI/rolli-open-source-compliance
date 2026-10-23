# RAW component source snapshot

This folder contains source materials for FujiLUTViewer's RAW components. It is
assembled from a fixed RawSpeed upstream commit, the LibRaw 0.22.0 component
currently vendored by the app, its RawSpeed C-API wrapper and generated camera
metadata, the seven RawSpeed patches, and pugixml. The app repository, app
sources, images, account data, build archives and Git history are excluded.

This repository contains component source, not the App's proprietary source.
The release index identifies which App version uses this snapshot and links to
its separate relink kit. That kit supplies proprietary App object code and
linking instructions so recipients can substitute a modified RawSpeed library
without receiving the App source. A source build alone is not a substitute for
the release-specific relink kit.

## Components and versions

- RawSpeed: [darktable-org/rawspeed](https://github.com/darktable-org/rawspeed),
  commit `de70ef5fbc62cde91009c8cff7a206272abe631e`. Its source headers
  permit LGPL version 2 or later; the repository supplies the LGPL-2.1 text
  and this distribution uses that version.
- LibRaw: 0.22.0-Release; upstream tag 0.22.0 resolves to
  `0b56545a4f828743f28a4345cdfdd4c49f9f9a2a`. The `LibRaw/`
  files reflect the app's vendored source, including changes from that tag.
  This distribution selects its CDDL-1.0 option. The DCB demosaicing portion
  also carries a BSD-3-Clause notice.
- RawSpeed C-API: the LibRaw-distributed wrapper with local modifications.
  `RawSpeed-CAPI/cameras.cpp` is generated from RawSpeed
  `data/cameras.xml`; its embedded XML declares CC-BY-SA-3.0. The exact XML,
  generator and full license text are included.
- pugixml: vendored v1.14 source under MIT.

The C-API directory contains differently licensed material. The
`rawspeed3_capi.cpp`, `rawspeed3_capi.h`, `rawspeed3_capi_test.cpp`, and
`rsxml2c.sh` are LibRaw-derived wrapper source, distributed here under
LibRaw's LGPL-2.1 option with their original notices and dated modification
notices. `RawSpeed-API.h` retains its own LGPL version 2-or-later header.
`cameras.cpp` contains generated CC-BY-SA-3.0 camera metadata from
`RawSpeed/data/cameras.xml`. The directory name alone is not a license
statement for every file.

The `LibRaw/COPYRIGHT` and original `LICENSE.CDDL` and `LICENSE.LGPL`
files are preserved. The X3F BSD attribution and terms occur in the retained
`LibRaw/internal/x3f_tools.h` and X3F source headers; a separate copy is in
`licenses/BSD-3-Clause-LibRaw-X3F.txt`. LibRaw's COPYRIGHT attributes
portions of Adobe DNG SDK 1.4 to Adobe Systems Incorporated under MIT.
`licenses/MIT-Adobe-DNG-LibRaw.txt` retains that reported attribution and
standard MIT terms. It does not identify individual Adobe-derived source
lines or represent the whole Adobe SDK.

Three modified LibRaw files and three modified C-API files carry source-file
modification notices using the repository's recorded author names and dates.
Their exact App source SHA-256 values and the published source SHA-256 values
are both in `MANIFEST.json`. The packaging notices add comments only.

RawSpeed files changed by the seven functional patches receive a separate
comment-only patch 08 with modification notices. Patches 01–05 are attributed
to LibRaw integration; 2026-10-23 is this snapshot application/notice date,
not an assertion of their original authorship date. Local patches 06–07
retain the recorded 2026-07-12 attribution to renhui sun. The manifest records
the unchanged seven input patch hashes and the separate notice patch hash.

The complete file-level SHA-256 list and the local differences from LibRaw
0.22.0 are in `MANIFEST.json`. The build procedure below reproduces the
source build for the two libraries; it is not a byte-for-byte identity claim
for a previously shipped archive.

## Build RawSpeed source libraries

On macOS with Xcode and CMake 3.31.6, run the supplied
`build_rawspeed_ios.sh` with `RAWSPEED_BUILD_ROOT` set to a new directory
whose parent exists; set `CMAKE_BIN` to the CMake executable if it is not on
`PATH`. The script
copies RawSpeed source to that scratch directory, applies patch 01 in reverse
and patches 02–07 forward, followed by packaging-only patch 08, then builds arm64 iPhoneOS and iPhone Simulator
`librawspeed.a` and `librawspeed3_capi.a`. It leaves the source package
unchanged and reports the output directory.

The App links these static archives and compiles the LibRaw source directly.
The `PerfLogging.hpp` in this package is a standalone-only shim that disables
two diagnostic timing logs while permitting the modified LibRaw files to
compile outside the App. It is not the App's original header and does not
represent the released binary. The proprietary App object code needed for
relinking is supplied in a separate release-specific relink kit; its source
remains private. The public source build entry builds the two RawSpeed
archives. It does not build or relink the App.

The component source build was checked on macOS with Xcode 26.6 and CMake
3.31.6 for arm64 iPhoneOS and iPhone Simulator. The two modified LibRaw units
that include the standalone diagnostic shim also passed syntax checks.

## Packaging support license

The newly written `build_rawspeed_ios.sh`, `PerfLogging.hpp`, and
`RawSpeed-patches/08.source-modification-notices.patch` are provided under
LGPL-2.1-or-later, including permission to use, modify and redistribute them
under that license. See `licenses/LGPL-2.1-RAWSPEED.txt`. This grant applies
only to these packaging support files and does not license proprietary App
source or change any original third-party license.
