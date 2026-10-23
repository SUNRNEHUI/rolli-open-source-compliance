# Rolli open-source component materials

This repository provides the source and notices for the RAW components used by Rolli / pica cam, and an index of release-specific relinking materials available on request. The application source repository remains private. Proprietary application source, Git history, user photographs, signing credentials, and provisioning profiles are not included.

## Component source

The [RAW component snapshot](components/raw-2026-10-23/README.md) contains pinned RawSpeed source, its functional and notice patches, the LibRaw component source and local modifications, the C-API wrapper, camera definitions, pugixml, and their applicable licenses. Its [file manifest](components/raw-2026-10-23/MANIFEST.json) records source hashes and modifications. There is no blanket license that replaces the individual component licenses.

RawSpeed is retained in the application. These materials do not disable it or replace the application's production decoding libraries.

## Relinking materials

See the [build index](RELEASES.md) for the matching kit identity and checksums. The two application relinking archives formerly attached to GitHub Releases are no longer offered as public downloads. Recipients can request the corresponding kit by email at [xihuan666sun@gmail.com](mailto:xihuan666sun@gmail.com). Each retained kit supplies the complete application link-object set described in its provenance, a compatible source-built RawSpeed library pair, and a relocatable command for linking a modified compatible library. The proprietary application's source is not needed for this procedure. The object-code permissions are stated in each kit's `OBJECT-LICENSE.txt`.

The 1.9.31 (62) kit reconstructs link objects from the retained release source and configuration. It is explicitly not a claim that the original build objects or the original signed executable have been reproduced byte-for-byte. The current development snapshot is a separate, unreleased entry, even where the application version fields still read 1.9.31 (62).

The retained kits contain newly source-built RawSpeed archives. Their hashes are distinguished from the production archives. Earlier written offers, including the offer to supply exact prebuilt libraries used in an earlier build, remain in force. Those original archives have been retained separately and can be requested through the channel below.

Relinking validation produces an unsigned arm64 iPhoneOS Mach-O executable. It does not establish installation or execution on an iPhone, or supply Apple signing credentials. Follow the kit's toolchain and compatibility requirements; a library change must preserve the interface expected by the application objects.

## Request materials

Email [xihuan666sun@gmail.com](mailto:xihuan666sun@gmail.com) to request materials. If email is unavailable, [open a GitHub Issue](https://github.com/SUNRNEHUI/rolli-open-source-compliance/issues/new). Include the application version, build number, platform, and approximate distribution date. Requests for earlier builds are accepted even if the build is absent from the index.

For recipients of builds containing statically linked RawSpeed, the LGPL-2.1 Section 6 offer remains valid for at least three years after the last distribution of the relevant build, without shortening an earlier written offer. Materials will be supplied for no more than the cost of performing their distribution. Modification for the recipient's own use and reverse engineering for debugging those modifications are permitted to the extent required by LGPL-2.1; the component licenses retain their full effect.
