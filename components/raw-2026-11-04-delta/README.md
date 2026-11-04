# RAW source delta for the next app build

The complete unchanged component source and license texts are in the base package named in `MANIFEST.json`. Apply `libraw-2026-10-28.patch` to a copy of that exact package with `patch -p1 -i <patch path>` from its root. The two complete replacement files are also provided. The patch adds dated CDDL modification notices while preserving the original file notices. Verify the two resulting SHA-256 values against `MANIFEST.json`. The existing six packaging-only notice files remain valid and are enumerated there.

This delta supplies corresponding component source. The release's statically linked RawSpeed also requires matching application objects and relink instructions for its exact archive; those are a separate release artifact.
