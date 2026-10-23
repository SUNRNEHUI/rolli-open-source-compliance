/* CDDL 1.0 modification notice: Component modifications recorded 2026-07-12 by SUEN and renhui sun; 2026-08-31 by sunrenhui. This source-package comment does not change behavior. */
/* -*- C++ -*-
 * Copyright 2019-2025 LibRaw LLC (info@libraw.org)
 *
 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */
#include "../../internal/libraw_cameraids.h"
#include "../../internal/libraw_cxx_defs.h"

#ifdef USE_RAWSPEED3
#include "rawspeed3_capi.h"
#include <array>
#include <chrono>
#include <cstdio>
#endif

#ifdef USE_RAWSPEED3
thread_local uint64_t rawspeed3_attempt_context = 0;
thread_local int rawspeed3_last_status_context = rawspeed3_not_inited;
thread_local char rawspeed3_last_reason_context[256] = {};
thread_local unsigned rawspeed3_attempt_count_context = 0;

int rawspeed3_libraw_error_for_status(int status)
{
  return status == rawspeed3_not_supported || status == rawspeed3_no_decoder
             ? LIBRAW_FILE_UNSUPPORTED
             : LIBRAW_DATA_ERROR;
}
#endif

int LibRaw::unpack(void)
{
  CHECK_ORDER_HIGH(LIBRAW_PROGRESS_LOAD_RAW);
  CHECK_ORDER_LOW(LIBRAW_PROGRESS_IDENTIFY);
  try
  {
#ifdef USE_RAWSPEED3
    using RawSpeedClock = std::chrono::steady_clock;
    auto rawspeedElapsedMs = [](RawSpeedClock::time_point start) {
      return std::chrono::duration<double, std::milli>(RawSpeedClock::now() - start).count();
    };
    const uint64_t rawspeed3AttemptId = rawspeed3_attempt_context;
    rawspeed3_last_status_context = rawspeed3_not_inited;
    rawspeed3_attempt_count_context = 0;
    std::snprintf(rawspeed3_last_reason_context,
                  sizeof(rawspeed3_last_reason_context), "%s",
                  "RawSpeed3 was not attempted");
#endif

    if (!libraw_internal_data.internal_data.input)
      return LIBRAW_INPUT_CLOSED;

    RUN_CALLBACK(LIBRAW_PROGRESS_LOAD_RAW, 0, 2);
    if (imgdata.rawparams.shot_select >= P1.raw_count)
      return LIBRAW_REQUEST_FOR_NONEXISTENT_IMAGE;

    if (!load_raw)
      return LIBRAW_UNSPECIFIED_ERROR;

    // already allocated ?
    if (imgdata.image)
    {
      free(imgdata.image);
      imgdata.image = 0;
    }
    if (imgdata.rawdata.raw_alloc)
    {
      free(imgdata.rawdata.raw_alloc);
      imgdata.rawdata.raw_alloc = 0;
    }
    if (libraw_internal_data.unpacker_data.meta_length)
    {
      if (libraw_internal_data.unpacker_data.meta_length >
          INT64(imgdata.rawparams.max_raw_memory_mb) * INT64(1024 * 1024))
        throw LIBRAW_EXCEPTION_TOOBIG;

      libraw_internal_data.internal_data.meta_data =
#ifdef LIBRAW_CALLOC_RAWSTORE
          (char *)calloc(libraw_internal_data.unpacker_data.meta_length,1);
#else
          (char *)malloc(libraw_internal_data.unpacker_data.meta_length);
#endif
    }

    libraw_decoder_info_t decoder_info;
    get_decoder_info(&decoder_info);

    int save_iwidth = S.iwidth, save_iheight = S.iheight,
        save_shrink = IO.shrink;

    int rwidth = S.raw_width, rheight = S.raw_height;
    if (!IO.fuji_width)
    {
      // adjust non-Fuji allocation
      if (rwidth < S.width + S.left_margin)
        rwidth = S.width + S.left_margin;
      if (rheight < S.height + S.top_margin)
        rheight = S.height + S.top_margin;
    }
    if (rwidth > 65535 ||
        rheight > 65535) // No way to make image larger than 64k pix
      throw LIBRAW_EXCEPTION_IO_CORRUPT;

    imgdata.rawdata.raw_image = 0;
    imgdata.rawdata.color4_image = 0;
    imgdata.rawdata.color3_image = 0;
    imgdata.rawdata.float_image = 0;
    imgdata.rawdata.float3_image = 0;

#ifdef USE_RAWSPEED3
    // USE_RAWSPEED_BITS exposes separate v1/v3 request bits.  Do not enter
    // the v3 decoder merely because this source file was compiled with the
    // RawSpeed3 integration: callers may intentionally select native LibRaw
    // unpacking (for diagnostics or compliance builds).
#ifdef USE_RAWSPEED_BITS
    const bool rawspeed3Requested =
        (imgdata.rawparams.use_rawspeed & LIBRAW_RAWSPEEDV3_USE) != 0;
#else
    const bool rawspeed3Requested = imgdata.rawparams.use_rawspeed != 0;
#endif

    if (rawspeed3Requested && !raw_was_read() && ID.input->size() > 0 &&
        ID.input->size() < 2147483615LL)
    {
      INT64 pixcount = INT64(MAX(S.width, S.raw_width)) * INT64(MAX(S.height, S.raw_height));
      INT64 planecount = (imgdata.idata.filters || P1.colors == 1) ? 1 : LIM(P1.colors, 3, 4);
      INT64 bytes = pixcount * planecount * 2; // sample size is always 2 for rawspeed
      if (bytes + INT64(libraw_internal_data.unpacker_data.meta_length) 
				> INT64(imgdata.rawparams.max_raw_memory_mb) * INT64(1024 * 1024))
        throw LIBRAW_EXCEPTION_TOOBIG;

        double rawspeed3InitMs = 0.0;
        if (!_rawspeed3_handle)
        {
            const auto initStart = RawSpeedClock::now();
            _rawspeed3_handle = rawspeed3_initdefault();
            if (_rawspeed3_handle)
              rawspeed3_set_dng_tile_workers(_rawspeed3_handle, 3);
            rawspeed3InitMs = rawspeedElapsedMs(initStart);
        }

        if (_rawspeed3_handle && ID.input->size() > 0) // large bound is checked at identify
        {
            void *_rawspeed_buffer = 0;
            try {
                ID.input->seek(0, SEEK_SET);
                INT64 _rawspeed_buffer_sz = ID.input->size() + 32;
                _rawspeed_buffer = malloc(_rawspeed_buffer_sz);
                if (!_rawspeed_buffer)
                    throw LIBRAW_EXCEPTION_ALLOC;
                const auto copyStart = RawSpeedClock::now();
                ID.input->read(_rawspeed_buffer, ID.input->size(), 1);
                const double rawspeed3CopyMs = rawspeedElapsedMs(copyStart);

                rawspeed3_ret_v2_t rs3ret = {};
                rs3ret.struct_size = sizeof(rs3ret);
                const auto attemptStart = RawSpeedClock::now();
                int status = rawspeed3_decodefile_v2(_rawspeed3_handle, &rs3ret, _rawspeed_buffer, ID.input->size(),
#ifdef USE_RAWSPEED_BITS
                    !(imgdata.rawparams.use_rawspeed & LIBRAW_RAWSPEEDV3_FAILONUNKNOWN)
#else
                    false
#endif
                );
                ++rawspeed3_attempt_count_context;
                const double rawspeed3AttemptMs = rawspeedElapsedMs(attemptStart);
                std::fprintf(stderr,
                  "[LibRaw][RawSpeed3] attemptId=%llu status=%d reason=%s predictor=%u compression=%u components=%u bps=%u tiles=%u workers=%u/%u completed=%u init=%.3fms inputCopy=%.3fms parseSupport=%.3fms decode=%.3fms attempt=%.3fms\n",
                  static_cast<unsigned long long>(rawspeed3AttemptId), status,
                  rs3ret.reason[0] ? rs3ret.reason : "none",
                  rs3ret.predictor, rs3ret.compression, rs3ret.components,
                  rs3ret.bits_per_sample, rs3ret.tile_count,
                  rs3ret.dng_tile_workers_used,
                  rs3ret.dng_tile_workers_requested,
                  rs3ret.dng_tiles_completed, rawspeed3InitMs,
                  rawspeed3CopyMs, rs3ret.parse_support_ms, rs3ret.decode_ms,
                  rawspeed3AttemptMs);
                if (status != rawspeed3_ok)
                  imgdata.process_warnings |= LIBRAW_WARN_RAWSPEED_UNSUPPORTED;

                rawspeed3_last_status_context = status;
                std::snprintf(rawspeed3_last_reason_context,
                              sizeof(rawspeed3_last_reason_context), "%s",
                              rs3ret.reason[0]
                                  ? rs3ret.reason
                                  : (status == rawspeed3_ok
                                         ? "none"
                                         : "RawSpeed3 decode failed without a reason"));

                if (status == rawspeed3_not_supported)
                  imgdata.process_warnings |= LIBRAW_WARN_RAWSPEED3_NOTLISTED;

                if (status == rawspeed3_ok
#ifdef USE_RAWSPEED_BITS
                    ||
                    (status == rawspeed3_ok_warnings && (imgdata.rawparams.use_rawspeed & LIBRAW_RAWSPEEDV3_IGNOREERRORS))
#endif
                    )
                {

                  if ((S.raw_width != rs3ret.decoded.width) || (S.raw_height != rs3ret.decoded.height))
                      throw "Size mismatch";

                    // DECODED w/ success
                  if (rs3ret.decoded.cpp == 4)
                  {
                      imgdata.rawdata.color4_image = (ushort(*)[4])rs3ret.decoded.pixeldata;
                    //if (r->whitePoint > 0 && r->whitePoint < 65536)
                    //  C.maximum = r->whitePoint;
                  }
                  else if (rs3ret.decoded.cpp == 3)
                  {
                      imgdata.rawdata.color3_image = (ushort(*)[3])rs3ret.decoded.pixeldata;
                    //if (r->whitePoint > 0 && r->whitePoint < 65536)
                    //  C.maximum = r->whitePoint;
                  }
                  else if (rs3ret.decoded.cpp == 1 && rs3ret.decoded.filters > 1) // Fuji or bayer
                  {
                    imgdata.rawdata.raw_image = (ushort *)rs3ret.decoded.pixeldata;
                  }

                  if (raw_was_read()) // buffers are assigned above
                  {
                    // set sizes
                    S.raw_pitch = rs3ret.decoded.pitch;
                    S.raw_width = rs3ret.decoded.width;
                    S.raw_height = rs3ret.decoded.height;
                    imgdata.process_warnings |= LIBRAW_WARN_RAWSPEED3_PROCESSED;

					if (imgdata.rawdata.raw_image && 
						load_raw == &LibRaw::phase_one_load_raw_c && imgdata.color.phase_one_data.format != 8)
                    {
                      // Scale data to match libraw own decoder
                      for (unsigned row = 0; row < rs3ret.decoded.height; row++)
                        for (unsigned col = 0; col < rs3ret.decoded.pitch / 2; col++)
                          imgdata.rawdata.raw_image[row * rs3ret.decoded.pitch / 2 + col] <<= 2;
                    }

                    // if (r->whitePoint > 0 && r->whitePoint < 65536)
                    // C.maximum = r->whitePoint;
                  }
                }
                free(_rawspeed_buffer);
            }
            catch (...)
            {
                imgdata.process_warnings |= LIBRAW_WARN_RAWSPEED3_PROBLEM;
                rawspeed3_last_status_context = rawspeed3_processing_error;
                std::snprintf(rawspeed3_last_reason_context,
                              sizeof(rawspeed3_last_reason_context), "%s",
                              "RawSpeed3 integration failed while unpacking the decoded buffer");
                if (_rawspeed_buffer)
                    free(_rawspeed_buffer);
            }
         }
    }
#endif
#ifdef USE_RAWSPEED3
    if (!raw_was_read() && rawspeed3Requested)
    {
      if (rawspeed3_last_status_context == rawspeed3_not_inited)
      {
        rawspeed3_last_status_context = rawspeed3_not_supported;
        std::snprintf(rawspeed3_last_reason_context,
                      sizeof(rawspeed3_last_reason_context), "%s",
                      "RAW input is not eligible for the RawSpeed3 decoder");
      }
      // RawSpeed is strict when explicitly requested. Do not silently switch
      // to native LibRaw after a failed or unsupported attempt.
      return rawspeed3_libraw_error_for_status(rawspeed3_last_status_context);
    }
#endif

    if (!raw_was_read()) // RawSpeed disabled or not compiled: use native LibRaw.
    {
      // This is the original LibRaw allocation/decode path. Keeping it here
      // makes FLV_DISABLE_RAWSPEED=1 a real A/B switch when USE_RAWSPEED3 is
      // compiled into the target.
      int zero_rawimage = 0;
      if (decoder_info.decoder_flags & LIBRAW_DECODER_OWNALLOC)
      {
        // x3f foveon decoder and DNG float
        // Do nothing! Decoder will allocate data internally
      }
      if (decoder_info.decoder_flags & LIBRAW_DECODER_SINAR4SHOT)
      {
        if (imgdata.rawparams.shot_select) // single image extract
        {
          if (INT64(rwidth) * INT64(rheight + 8) *
                  INT64(sizeof(imgdata.rawdata.raw_image[0])) +
                  INT64(libraw_internal_data.unpacker_data.meta_length) >
              INT64(imgdata.rawparams.max_raw_memory_mb) * INT64(1024 * 1024))
            throw LIBRAW_EXCEPTION_TOOBIG;
#ifdef LIBRAW_CALLOC_RAWSTORE
          imgdata.rawdata.raw_alloc = calloc(
              size_t(rwidth) * (size_t(rheight) + 8),
              sizeof(imgdata.rawdata.raw_image[0]));
#else
          imgdata.rawdata.raw_alloc = malloc(
              size_t(rwidth) * (size_t(rheight) + 8) *
              sizeof(imgdata.rawdata.raw_image[0]));
#endif
          imgdata.rawdata.raw_image = (ushort *)imgdata.rawdata.raw_alloc;
          if (!S.raw_pitch)
            S.raw_pitch = S.raw_width * 2; // Bayer case, not set before
        }
        else // Full image extract
        {
          if (INT64(rwidth) * INT64(rheight + 8) *
                  INT64(sizeof(imgdata.rawdata.raw_image[0])) * 4 +
                  INT64(libraw_internal_data.unpacker_data.meta_length) >
              INT64(imgdata.rawparams.max_raw_memory_mb) * INT64(1024 * 1024))
            throw LIBRAW_EXCEPTION_TOOBIG;
          S.raw_pitch = S.raw_width * 8;
          imgdata.rawdata.raw_alloc = 0;
#ifdef LIBRAW_CALLOC_RAWSTORE
          imgdata.image = (ushort(*)[4])calloc(
              size_t(MAX(S.width, S.raw_width)) *
                  (size_t(MAX(S.height, S.raw_height)) + 8),
              sizeof(*imgdata.image));
#else
          imgdata.image = (ushort(*)[4])malloc(
              size_t(MAX(S.width, S.raw_width)) *
              (size_t(MAX(S.height, S.raw_height)) + 8) *
              sizeof(*imgdata.image));
#endif
        }
      }
      else if (decoder_info.decoder_flags & LIBRAW_DECODER_3CHANNEL)
      {
        if (INT64(rwidth) * INT64(rheight + 8) *
                INT64(sizeof(imgdata.rawdata.raw_image[0])) * 3 +
                INT64(libraw_internal_data.unpacker_data.meta_length) >
            INT64(imgdata.rawparams.max_raw_memory_mb) * INT64(1024 * 1024))
          throw LIBRAW_EXCEPTION_TOOBIG;

#ifdef LIBRAW_CALLOC_RAWSTORE
        imgdata.rawdata.raw_alloc =
            calloc(size_t(rwidth) * (size_t(rheight) + 8),
                   sizeof(imgdata.rawdata.raw_image[0]) * 3);
#else
        imgdata.rawdata.raw_alloc = malloc(
            size_t(rwidth) * (size_t(rheight) + 8) *
            sizeof(imgdata.rawdata.raw_image[0]) * 3);
#endif
        imgdata.rawdata.color3_image =
            (ushort(*)[3])imgdata.rawdata.raw_alloc;
        if (!S.raw_pitch)
          S.raw_pitch = S.raw_width * 6;
      }
      else if (imgdata.idata.filters || P1.colors == 1)
      {
        // Bayer image or single color -> decode to raw_image.
        if (INT64(rwidth) * INT64(rheight + 8) *
                INT64(sizeof(imgdata.rawdata.raw_image[0])) +
                INT64(libraw_internal_data.unpacker_data.meta_length) >
            INT64(imgdata.rawparams.max_raw_memory_mb) * INT64(1024 * 1024))
          throw LIBRAW_EXCEPTION_TOOBIG;
#ifdef LIBRAW_CALLOC_RAWSTORE
        imgdata.rawdata.raw_alloc =
            calloc(size_t(rwidth) * (size_t(rheight) + 8),
                   sizeof(imgdata.rawdata.raw_image[0]));
#else
        imgdata.rawdata.raw_alloc = malloc(
            size_t(rwidth) * (size_t(rheight) + 8) *
            sizeof(imgdata.rawdata.raw_image[0]));
#endif
        imgdata.rawdata.raw_image = (ushort *)imgdata.rawdata.raw_alloc;
        if (!S.raw_pitch)
          S.raw_pitch = S.raw_width * 2; // Bayer case, not set before
      }
      else // NO LEGACY FLAG if (decoder_info.decoder_flags & LIBRAW_DECODER_LEGACY)
      {
        if (decoder_info.decoder_flags & LIBRAW_DECODER_ADOBECOPYPIXEL)
        {
          S.raw_pitch = S.raw_width * 8;
        }
        else
        {
          S.iwidth = S.width;
          S.iheight = S.height;
          IO.shrink = 0;
          if (!S.raw_pitch)
            S.raw_pitch = (decoder_info.decoder_flags &
                           LIBRAW_DECODER_LEGACY_WITH_MARGINS)
                              ? S.raw_width * 8
                              : S.width * 8;
        }
        // sRAW and old Foveon decoders only, so extra buffer size is just 1/4.
        if (INT64(MAX(S.width, S.raw_width)) *
                INT64(MAX(S.height, S.raw_height) + 8) *
                INT64(sizeof(*imgdata.image)) +
                INT64(libraw_internal_data.unpacker_data.meta_length) >
            INT64(imgdata.rawparams.max_raw_memory_mb) * INT64(1024 * 1024))
          throw LIBRAW_EXCEPTION_TOOBIG;

        imgdata.rawdata.raw_alloc = 0;
        imgdata.image =
            (ushort(*)[4])calloc(unsigned(MAX(S.width, S.raw_width)),
                                  unsigned(MAX(S.height, S.raw_height) + 8) *
                                  sizeof(*imgdata.image));
        if (!(decoder_info.decoder_flags & LIBRAW_DECODER_ADOBECOPYPIXEL))
        {
          imgdata.rawdata.raw_image = (ushort *)imgdata.image;
          zero_rawimage = 1;
        }
      }
      ID.input->seek(libraw_internal_data.unpacker_data.data_offset, SEEK_SET);

      unsigned m_save = C.maximum;
      if (load_raw == &LibRaw::unpacked_load_raw &&
          (!strcasecmp(imgdata.idata.make, "Nikon") ||
           !strcasecmp(imgdata.idata.make, "Hasselblad")))
        C.maximum = 65535;
      (this->*load_raw)();
      if (zero_rawimage)
        imgdata.rawdata.raw_image = 0;
      if (load_raw == &LibRaw::unpacked_load_raw &&
          (!strcasecmp(imgdata.idata.make, "Nikon") ||
           !strcasecmp(imgdata.idata.make, "Hasselblad")))
        C.maximum = m_save;
      if (decoder_info.decoder_flags & LIBRAW_DECODER_OWNALLOC)
      {
        // x3f foveon decoder only: do nothing
      }
      else if (decoder_info.decoder_flags & LIBRAW_DECODER_SINAR4SHOT &&
               imgdata.rawparams.shot_select == 0)
      {
        imgdata.rawdata.raw_alloc = imgdata.image;
        imgdata.rawdata.color4_image =
            (ushort(*)[4])imgdata.rawdata.raw_alloc;
        imgdata.image = 0;
      }
      else if (!(imgdata.idata.filters || P1.colors == 1))
      {
        // Successfully decoded legacy image, attach image to raw_alloc.
        imgdata.rawdata.raw_alloc = imgdata.image;
        imgdata.rawdata.color4_image =
            (ushort(*)[4])imgdata.rawdata.raw_alloc;
        imgdata.image = 0;
        // Restore saved values. Foveon has masked frame; other 4-color legacy
        // data has no borders.
        if (!(libraw_internal_data.unpacker_data.load_flags & 256) &&
            !(decoder_info.decoder_flags & LIBRAW_DECODER_ADOBECOPYPIXEL) &&
            !(decoder_info.decoder_flags & LIBRAW_DECODER_LEGACY_WITH_MARGINS))
        {
          S.raw_width = S.width;
          S.left_margin = 0;
          S.raw_height = S.height;
          S.top_margin = 0;
        }
      }
    }
    if (imgdata.rawdata.raw_image)
      crop_masked_pixels(); // calculate black levels

    // recover image sizes
    S.iwidth = save_iwidth;
    S.iheight = save_iheight;
    IO.shrink = save_shrink;

    // adjust black to possible maximum
    unsigned int i = C.cblack[3];
    unsigned int c;
    for (c = 0; c < 3; c++)
      if (i > C.cblack[c])
        i = C.cblack[c];
    for (c = 0; c < 4; c++)
      C.cblack[c] -= i;
    C.black += i;

    // Save color,sizes and internal data into raw_image fields
    memmove(&imgdata.rawdata.color, &imgdata.color, sizeof(imgdata.color));
    memmove(&imgdata.rawdata.sizes, &imgdata.sizes, sizeof(imgdata.sizes));
    memmove(&imgdata.rawdata.iparams, &imgdata.idata, sizeof(imgdata.idata));
    memmove(&imgdata.rawdata.ioparams,
            &libraw_internal_data.internal_output_params,
            sizeof(libraw_internal_data.internal_output_params));

    SET_PROC_FLAG(LIBRAW_PROGRESS_LOAD_RAW);
    RUN_CALLBACK(LIBRAW_PROGRESS_LOAD_RAW, 1, 2);

    return 0;
  }
  catch (const std::bad_alloc&)
  {
      EXCEPTION_HANDLER(LIBRAW_EXCEPTION_ALLOC);
  }
  catch (const LibRaw_exceptions& err)
  {
    EXCEPTION_HANDLER(err);
  }
  catch (const std::exception& )
  {
    EXCEPTION_HANDLER(LIBRAW_EXCEPTION_IO_CORRUPT);
  }
}
