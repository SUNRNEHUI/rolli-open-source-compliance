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

## Current development snapshot

The current unreleased snapshot is still undergoing final archive and relinking validation. No kit for that snapshot is published by this entry. Its application version fields may still show 1.9.31 (62); it must not be confused with the distributed build above. A separate snapshot entry will identify it when validation is complete.

## Other builds

[Request materials](https://github.com/SUNRNEHUI/rolli-open-source-compliance/issues/new) with version, build number, platform and approximate distribution date. An absent index entry does not cancel an earlier written offer.
