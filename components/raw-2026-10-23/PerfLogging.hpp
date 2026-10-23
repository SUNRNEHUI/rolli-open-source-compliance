// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

// Source-package-only shim. The distributed App uses its own private
// PerfLogging.hpp. This file lets the modified LibRaw source compile alone
// while disabling two diagnostic timing messages. It does not replace the
// original header in a released App or an exact relink kit.
namespace FujiLUTViewer {
namespace PerfLogging {
inline bool rawDecoderLogsEnabled() { return false; }
}  // namespace PerfLogging
}  // namespace FujiLUTViewer

#define FLV_PERF_LOG_IF(enabled, ...) do { } while (0)
