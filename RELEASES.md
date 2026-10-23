# Build and materials index

## 1.9.31 (62), distributed build — reconstructed relinking materials

- [Download the kit and checksum](https://github.com/SUNRNEHUI/rolli-open-source-compliance/releases/tag/app-1.9.31-62-reconstructed)
- File: `rolli-1.9.31-62-iphoneos-arm64-relink.tar.gz`
- SHA-256: `fa56ced9e5a5fcb677a9f280c9355a9009374a5f6924b1037fe512ddb3b8318f`
- Size: 17,553,825 bytes; 278 arm64 iPhoneOS application link objects.
- [Corresponding component source and build instructions](components/raw-2026-10-23/README.md)
- Component manifest SHA-256: `b7d8d580db7005d7e9b4e249f31b40e4ae00664dc81f1f503857c382eab5825b`.

The application objects were reconstructed from retained release inputs; original build objects were not retained. The kit does not claim byte-for-byte reproduction of the distributed executable. It distinguishes historical production libraries from the newly source-built libraries supplied in the kit. Historical exact-library requests remain available through [Issues](https://github.com/SUNRNEHUI/rolli-open-source-compliance/issues/new).

Baseline relinking with the historical libraries matches the original release's imports, exports and all 56 dynamic-library dependencies after its executable stripping step. Relinking with the supplied source-built pair and a modified-library probe also passed. The source-built pair changes three libc++ imports in place of two historical imports; this measured difference is documented in the kit. Device execution and runtime equivalence have not been verified.

## Current development snapshot — 2026-10-23, unreleased

- Snapshot: `current-unreleased-link-snapshot-827aad35ff2a368f`.
- [Download the snapshot kit and checksum](https://github.com/SUNRNEHUI/rolli-open-source-compliance/releases/tag/current-unreleased-20261023)
- File: `rolli-current-unreleased-20261023-iphoneos-arm64-relink.tar.gz`
- SHA-256: `6541f7e1dd22809ef9b69c065d8c133f7ee751df178184fa8753725e86ff404d`
- Size: 17,400,340 bytes; 276 arm64 iPhoneOS application link objects.
- [Corresponding component source](components/raw-2026-10-23/README.md), using the same component manifest identified above.
- Unsigned archive executable SHA-256: `16c55bb01f76325553d5a23758b15312ebbbea5182f677225849d7886333365c`.

This is a development snapshot, not a new distributed app build. Its version fields still read 1.9.31 (62); it must not be confused with the earlier distributed build. The final unsigned archive succeeded. The captured full link-object set passed baseline, source-built replacement, modified-library probe, and relocated-directory relinking. The baseline matches the archive's external imports, dynamic-library dependencies, and exports after distribution stripping. Production decoding libraries were not changed. Public rebuilt libraries are separately identified in the kit.

Verification also checked all 14 bundled legal documents, the decoded pixels of the 12 updated light-leak images, and the retained Star resources. Focused simulator cache regression tests passed (6 passed, 1 existing RAW-fixture-dependent test skipped, no failures). These checks do not establish physical-device execution or runtime equivalence of a modified RawSpeed library.

## Other builds

[Request materials](https://github.com/SUNRNEHUI/rolli-open-source-compliance/issues/new) with version, build number, platform and approximate distribution date. An absent index entry does not cancel an earlier written offer.
