/* LGPL component modification notice: Modified by SUEN and renhui sun on 2026-07-12. This source-package comment does not change behavior. */
#include "rawspeed3_capi.h"
#include "RawSpeed-API.h"
#define HAVE_PUGIXML
#include <../pugixml/pugixml.hpp> // for xml_document, xml_pars...
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <unordered_set>

extern const char* _rawspeed3_data_xml;

namespace {
using Clock = std::chrono::steady_clock;

double elapsedMs(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

void setReason(rawspeed3_ret_v2_t* result, const char* reason) {
    if (std::strncmp(result->reason, "TIFF diagnostic scan stopped", 28) == 0) {
        const size_t used = std::strlen(result->reason);
        std::snprintf(result->reason + used, sizeof(result->reason) - used,
                      "; RawSpeed: %s", reason ? reason : "unknown error");
        return;
    }
    std::snprintf(result->reason, sizeof(result->reason), "%s", reason ? reason : "unknown error");
}

uint16_t read16(const uint8_t* p, bool little) {
    return little ? uint16_t(p[0] | (uint16_t(p[1]) << 8))
                  : uint16_t((uint16_t(p[0]) << 8) | p[1]);
}

uint32_t read32(const uint8_t* p, bool little) {
    return little ? uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24)
                  : (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

bool checkedAdd(size_t a, size_t b, size_t* result) {
    if (b > std::numeric_limits<size_t>::max() - a) return false;
    *result = a + b;
    return true;
}

bool checkedMul(size_t a, size_t b, size_t* result) {
    if (a && b > std::numeric_limits<size_t>::max() / a) return false;
    *result = a * b;
    return true;
}

struct TiffDiagnosticBudget {
    static constexpr size_t maxIfds = 256;
    static constexpr size_t maxEntries = 32768;
    static constexpr size_t maxWork = 4U * 1024U * 1024U;

    rawspeed3_ret_v2_t* result;
    std::unordered_set<uint32_t> visitedIfds;
    size_t ifds = 0;
    size_t entries = 0;
    size_t work = 0;
    bool stopped = false;

    explicit TiffDiagnosticBudget(rawspeed3_ret_v2_t* output) : result(output) {
        visitedIfds.reserve(maxIfds);
    }

    void stop(const char* detail) {
        if (stopped) return;
        stopped = true;
        std::snprintf(result->reason, sizeof(result->reason),
                      "TIFF diagnostic scan stopped: %s", detail);
    }

    bool chargeWork(size_t amount) {
        if (amount > maxWork - work) {
            stop("work budget exceeded");
            return false;
        }
        work += amount;
        return true;
    }
};

void inspectIfd(rawspeed3_ret_v2_t* result, const uint8_t* data, size_t size,
                uint32_t offset, bool little, unsigned depth, TiffDiagnosticBudget& budget) {
    if (result->tile_count || budget.stopped) return;
    if (depth > 8) {
        budget.stop("depth limit exceeded");
        return;
    }
    if (!budget.visitedIfds.insert(offset).second) {
        budget.stop("repeated IFD offset");
        return;
    }
    if (budget.ifds == TiffDiagnosticBudget::maxIfds) {
        budget.stop("IFD budget exceeded");
        return;
    }
    ++budget.ifds;
    if (offset > size || size - offset < 2 || !budget.chargeWork(2)) return;
    const uint16_t count = read16(data + offset, little);
    size_t entriesBytes = 0;
    size_t ifdBytes = 0;
    if (!checkedMul(size_t(count), 12U, &entriesBytes)
        || !checkedAdd(2U, entriesBytes, &ifdBytes)
        || ifdBytes > size - offset)
        return;
    if (size_t(count) > TiffDiagnosticBudget::maxEntries - budget.entries) {
        budget.stop("entry budget exceeded");
        return;
    }
    budget.entries += count;
    if (!budget.chargeWork(entriesBytes)) return;
    uint32_t tileOffset = 0;
    uint32_t tileByteCount = 0;
    uint32_t tileCount = 0;
    for (uint16_t i = 0; i < count; ++i) {
        if (result->tile_count) break;
        const uint8_t* e = data + offset + 2U + size_t(i) * 12U;
        const uint16_t tag = read16(e, little);
        const uint16_t type = read16(e + 2, little);
        const uint32_t values = read32(e + 4, little);
        const uint32_t valueOffset = read32(e + 8, little);
        const size_t unit = type == 3 ? 2U : type == 4 ? 4U : 0U;
        const size_t bytes = unit && values <= SIZE_MAX / unit ? size_t(values) * unit : 0;
        const uint8_t* value = bytes && bytes <= 4 ? e + 8
                             : bytes && valueOffset <= size && bytes <= size - valueOffset ? data + valueOffset
                             : nullptr;
        const uint32_t first = value ? (type == 3 ? read16(value, little) : read32(value, little)) : 0;
        switch (tag) {
            case 258: result->bits_per_sample = uint16_t(first); break;
            case 259: result->compression = uint16_t(first); break;
            case 277: result->components = uint16_t(first); break;
            case 324:
                if ((type == 3 || type == 4) && value && values > 0) {
                    tileOffset = first;
                    tileCount = values;
                }
                break;
            case 325:
                if ((type == 3 || type == 4) && value && values > 0)
                    tileByteCount = first;
                break;
            case 330:
                if (value) for (uint32_t j = 0; j < values && !result->tile_count && !budget.stopped; ++j) {
                    size_t valueIndex = 0;
                    if (!checkedMul(size_t(j), unit, &valueIndex) || valueIndex > bytes - unit) {
                        budget.stop("SubIFD offset arithmetic overflow");
                        break;
                    }
                    inspectIfd(result, data, size,
                               type == 3 ? read16(value + valueIndex, little)
                                         : read32(value + valueIndex, little),
                               little, depth + 1, budget);
                }
                break;
        }
    }
    if (!tileCount || !tileByteCount || tileOffset >= size || tileByteCount > size - tileOffset) return;
    result->tile_count = tileCount;
    size_t tileEnd = 0;
    if (!checkedAdd(size_t(tileOffset), size_t(tileByteCount), &tileEnd)
        || !budget.chargeWork(tileByteCount))
        return;
    for (size_t p = tileOffset; p + 4 < tileEnd; ++p) {
        if (data[p] != 0xff || data[p + 1] != 0xda) continue;
        const uint16_t length = uint16_t((uint16_t(data[p + 2]) << 8) | data[p + 3]);
        if (length >= 6 && p + 2U + length <= tileEnd) {
            const uint8_t components = data[p + 4];
            const size_t predictorAt = p + 5U + size_t(components) * 2U;
            if (predictorAt < p + 2U + length) result->predictor = data[predictorAt];
        }
        break;
    }
}

void inspectTiff(rawspeed3_ret_v2_t* result, const void* input, size_t size) {
    if (size < 8) return;
    const auto* data = static_cast<const uint8_t*>(input);
    const bool little = data[0] == 'I' && data[1] == 'I';
    const bool big = data[0] == 'M' && data[1] == 'M';
    if ((!little && !big) || read16(data + 2, little) != 42) return;
    TiffDiagnosticBudget budget(result);
    uint32_t offset = read32(data + 4, little);
    for (unsigned i = 0; offset && i < 8; ++i) {
        inspectIfd(result, data, size, offset, little, 0, budget);
        if (result->tile_count || budget.stopped) break;
        if (offset > size || size - offset < 2) break;
        const uint16_t entries = read16(data + offset, little);
        size_t entriesBytes = 0;
        size_t next = 0;
        if (!checkedMul(size_t(entries), 12U, &entriesBytes)
            || !checkedAdd(size_t(offset), 2U, &next)
            || !checkedAdd(next, entriesBytes, &next))
            break;
        if (next > size || size - next < 4) break;
        offset = read32(data + next, little);
    }
}
} // namespace

class rawspeed3_handle_data
{
public:
    rawspeed3_handle_data(const char* cameradefs, bool is_file);
    void release();
    int setDngTileWorkers(unsigned workers);
    int decodefile(rawspeed3_ret_v2_t* resultp, const void *data, size_t datasize, bool allowunknown);
    ~rawspeed3_handle_data();
private:
    std::unique_ptr <rawspeed::CameraMetaData> cameraMeta;
    std::unique_ptr<rawspeed::RawParser> rawParser;
    std::unique_ptr<rawspeed::RawDecoder> rawDecoder;
    std::atomic<unsigned> dngTileWorkers{3};
};


/* API calls */

extern "C"
{

/*
    void rawspeed3_clearresult(rawspeed3_ret_v2_t* r)
   
    Clears (inits) results structure
*/
DllDef void rawspeed3_clearresult(rawspeed3_ret_t* r)
{
    if(!r) return;
    std::memset(r, 0, sizeof(*r));
    r->status = rawspeed_inited;
}

DllDef void rawspeed3_clearresult_v2(rawspeed3_ret_v2_t* r)
{
    if (!r) return;
    const size_t callerSize = r->struct_size;
    const size_t writable = std::min(callerSize, sizeof(*r));
    std::memset(r, 0, writable);
    if (writable >= sizeof(r->struct_size)) r->struct_size = callerSize;
    const size_t statusEnd = offsetof(rawspeed3_ret_v2_t, decoded)
                           + offsetof(rawspeed3_ret_t, status) + sizeof(r->decoded.status);
    if (writable >= statusEnd)
        r->decoded.status = rawspeed_inited;
}

/* V1 remains bounded to the original result size. */
DllDef int rawspeed3_decodefile(rawspeed3_handle_t handle, rawspeed3_ret_t* resultp,
    const void *data, size_t datasize, bool allowunknown)
{
    if (!resultp) return rawspeed3_param_error;
    rawspeed3_ret_v2_t v2 = {};
    v2.struct_size = sizeof(v2);
    rawspeed3_clearresult_v2(&v2);
    const int status = rawspeed3_decodefile_v2(handle, &v2, data, datasize, allowunknown);
    *resultp = v2.decoded;
    return status;
}

/*
    rawspeed3_init()
    Init rawspeed3 Camera, returns: 0 on failure, pointer to data block on success
    Cameradefs: cameras.xml in string (is_file == false) or file (is_file == true)
*/

DllDef rawspeed3_handle_t rawspeed3_init(const char* cameradefs, bool is_file)
{
    /* Create rawspeed3_handle_data and return it as void document */
    if(!cameradefs) return nullptr;
    try
    {
        /* code */
		auto *handle = new rawspeed3_handle_data(cameradefs, is_file);
        return (void*)handle;
    }
    catch(const std::exception& e)
    {
        return nullptr;
    }  
}

/*
    rawspeed3_initdefault()
    Init rawspeed3 Cameradefs with built-in cameras.xml (converted to _rawspeed3_data_xml),
    returns: 0 on failure, pointer to data block on success
*/

DllDef rawspeed3_handle_t rawspeed3_initdefault()
{
    return rawspeed3_init(_rawspeed3_data_xml,false);
}

DllDef int rawspeed3_set_dng_tile_workers(rawspeed3_handle_t handle,
                                          unsigned workers)
{
    if (!handle || workers < 1 || workers > 4)
        return rawspeed3_param_error;
    return static_cast<rawspeed3_handle_data*>(handle)->setDngTileWorkers(workers);
}

/*
    rawspeed3_decodefile(..)
    parse/decode RAW file passed via memory
    Parameters:
      handle - rawspeed3_handle_t => handle created by rawspeed3_init()
      resultp -> pointer to rawspeed3_ret_t to be filled with
      data -> data buffer with raw file
      datasize -> size of this buffer
      allowunknown -> allow to process unknown cameras (not listed in cameras.xml)
    Return values:
      0 -> OK 
      >=1 -> decode error
     -1 -> Incorrect parameters passed (handle, or data, or datasize)
*/

DllDef int rawspeed3_decodefile_v2(rawspeed3_handle_t handle, rawspeed3_ret_v2_t* resultp,
    const void *data, size_t datasize, bool allowunknown)
{
    const size_t minimumSize = offsetof(rawspeed3_ret_v2_t, decoded)
                             + offsetof(rawspeed3_ret_t, status) + sizeof(resultp->decoded.status);
    if (!resultp || resultp->struct_size < minimumSize) return rawspeed3_param_error;
    const size_t callerSize = resultp->struct_size;
    rawspeed3_ret_v2_t full = {};
    full.struct_size = sizeof(full);
    rawspeed3_clearresult_v2(&full);
    auto publish = [&] {
        full.struct_size = callerSize;
        std::memcpy(resultp, &full, std::min(callerSize, sizeof(full)));
    };
    if(!handle || !data || datasize > 2UL * 1024UL * 1024UL * 1024UL)
    {
        full.decoded.status = rawspeed3_param_error;
        setReason(&full, !handle ? "RawSpeed handle is null" : !data ? "input data is null" : "input exceeds 2 GiB");
        publish();
        return rawspeed3_param_error;
    }
    auto *p = static_cast<rawspeed3_handle_data*>(handle);
    const int status = p->decodefile(&full, data, datasize, allowunknown);
    publish();
    return status;
}

/*
    void rawspeed3_release(rawspeed3_handle_t handle)

    release internal raw data buffer and error code;
*/

/* release internal raw data buffer and errmessage (if any) */
DllDef void rawspeed3_release(rawspeed3_handle_t handle)
{
    if(!handle) return;
	auto *p =  static_cast<rawspeed3_handle_data*>(handle);
    p->release();
}

/* close handle: release all internal data */
DllDef void rawspeed3_close(rawspeed3_handle_t handle)
{
    if(!handle) return;
	auto *p = static_cast<rawspeed3_handle_data*>(handle);
    delete p;
}

} /* extern "C" */

// == Implementation

int rawspeed3_handle_data::decodefile(rawspeed3_ret_v2_t* resultp,
    const void *data, size_t datasize, bool allowunknown)
{
    const auto attemptStart = Clock::now();
    auto decodeStart = attemptStart;
    bool decodeStarted = false;
    if(!cameraMeta)
    {
        resultp->decoded.status = rawspeed3_not_inited;
        setReason(resultp, "RawSpeed camera metadata is not initialized");
        return rawspeed3_not_inited;
    }
    try
    {
        inspectTiff(resultp, data, datasize);
        const auto parseStart = Clock::now();
        rawspeed::Buffer buffer( static_cast<const uint8_t*>(data),datasize);
        release();
        rawParser = std::make_unique<rawspeed::RawParser>(buffer);
        auto d(rawParser->getDecoder(cameraMeta.get()));
        if(!d)
        {
            resultp->parse_support_ms = elapsedMs(parseStart);
            resultp->decoded.status = rawspeed3_no_decoder;
            setReason(resultp, "RawSpeed found no decoder for the input");
            return rawspeed3_no_decoder;
        }

        d->applyCrop = false;
        const unsigned tileWorkers = dngTileWorkers.load(std::memory_order_relaxed);
        d->dngTileWorkers = tileWorkers;
        resultp->dng_tile_workers_requested = static_cast<uint16_t>(tileWorkers);
        d->failOnUnknown = !allowunknown;
        d->interpolateBadPixels = false;
        d->applyStage1DngOpcodes = false;
        d->fujiRotate = false;
        d->applyCrop = false;

        try {
          d->checkSupport(cameraMeta.get());
        } catch (const std::exception& e) {
            resultp->parse_support_ms = elapsedMs(parseStart);
            setReason(resultp, e.what());
            release();
            resultp->decoded.status = rawspeed3_not_supported;
            return resultp->decoded.status;
        } catch (...) {
            resultp->parse_support_ms = elapsedMs(parseStart);
            setReason(resultp, "RawSpeed support check failed");
            release();
            resultp->decoded.status = rawspeed3_not_supported;
            return resultp->decoded.status;
        }

        rawspeed::RawImage r = d->mRaw;
        d->decodeMetaData(cameraMeta.get());

        d->checkSupport(cameraMeta.get());
        resultp->parse_support_ms = elapsedMs(parseStart);
        decodeStart = Clock::now();
        decodeStarted = true;
        d->decodeRaw();
        resultp->decode_ms = elapsedMs(decodeStart);
        decodeStarted = false;
        d->decodeMetaData(cameraMeta.get());
        r = d->mRaw;

        const unsigned workersUsed = d->dngTileWorkersUsed;
        const uint32_t tilesCompleted = d->dngTilesCompleted;
        rawDecoder = std::move(d);
        // we're here w/o exceptions: success
        const rawspeed::iPoint2D dimUncropped = r->getUncroppedDim();
        resultp->decoded.width = dimUncropped.x;
        resultp->decoded.height = dimUncropped.y;
        resultp->decoded.filters = r->cfa.getDcrawFilter();
        resultp->decoded.cpp = r->getCpp();
        resultp->decoded.bpp = r->getBpp();
        resultp->decoded.pitch = r->pitch;
        resultp->decoded.pixeldata = r->getDataUncropped(0,0);
        resultp->dng_tile_workers_used = static_cast<uint16_t>(workersUsed);
        resultp->dng_tiles_completed = tilesCompleted;
        const auto errors = r->getErrors();
        resultp->decoded.status = errors.empty()? rawspeed3_ok : rawspeed3_ok_warnings;
        return resultp->decoded.status;
        /* code */
    }
    catch(const std::exception& e)
    {
        if (decodeStarted) resultp->decode_ms = elapsedMs(decodeStart);
        if (resultp->parse_support_ms == 0.0) resultp->parse_support_ms = elapsedMs(attemptStart);
        if (resultp->predictor != 0)
            std::snprintf(resultp->reason, sizeof(resultp->reason), "%s [predictor %u]", e.what(), resultp->predictor);
        else
            setReason(resultp, e.what());
        resultp->decoded.status = rawspeed3_processing_error;
        release();
        return rawspeed3_processing_error;
    }
    catch(...)
    {
        if (decodeStarted) resultp->decode_ms = elapsedMs(decodeStart);
        if (resultp->parse_support_ms == 0.0) resultp->parse_support_ms = elapsedMs(attemptStart);
        setReason(resultp, "unknown RawSpeed processing error");
        resultp->decoded.status = rawspeed3_processing_error;
        release();
        return rawspeed3_processing_error;
    }
}

int rawspeed3_handle_data::setDngTileWorkers(unsigned workers)
{
    dngTileWorkers.store(workers, std::memory_order_relaxed);
    return rawspeed3_ok;
}

namespace rawspeed
{
	class CameraMetaDataFromMem : public CameraMetaData
	{
	public:
		explicit CameraMetaDataFromMem(const char* xmlstring);
	};
}


rawspeed3_handle_data::rawspeed3_handle_data(const char* cameradefs, bool is_file)
    : rawParser(nullptr)
{
  if (is_file)
    cameraMeta = std::make_unique<rawspeed::CameraMetaData>(cameradefs);
  else
    cameraMeta = std::unique_ptr<rawspeed::CameraMetaData>(
        new rawspeed::CameraMetaDataFromMem(cameradefs));
}
rawspeed3_handle_data::~rawspeed3_handle_data()
{
    release();
    cameraMeta.reset();
}

void rawspeed3_handle_data::release()
{
    if(rawDecoder)
        rawDecoder.reset();
    if (rawParser)
        rawParser.reset();
}

// Camera metadata from mem
namespace rawspeed
{
	CameraMetaDataFromMem::CameraMetaDataFromMem(const char* document)
	{
		using pugi::xml_node;
		using pugi::xml_document;
		using pugi::xml_parse_result;

		xml_document doc;
		xml_parse_result result = doc.load_string(document);

		if (!result)
			throw "Camera definitions parse error";

		for (xml_node camera : doc.child("Cameras").children("Camera"))
		{
			const auto* cam = addCamera(std::make_unique<Camera>(camera));

			if (cam == nullptr)
				continue;

			// Create cameras for aliases.
			for (auto i = 0UL; i < cam->aliases.size(); i++)
				addCamera(std::make_unique<Camera>(cam, i));
		}
	}
} // namespace rawspeed
