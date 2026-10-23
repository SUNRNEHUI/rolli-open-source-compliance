/* LGPL component modification notice: Modified by SUEN and renhui sun on 2026-07-12. This source-package comment does not change behavior. */
#pragma once
#include <stdint.h>
#include <stdlib.h>

#ifdef _MSC_VER
#ifdef RAWSPEED_BUILDLIB
#define DllDef __declspec(dllexport)
#else
#define DllDef __declspec(dllimport)
#endif
#else
#define DllDef
#endif


#ifdef __cplusplus
extern "C"
{
#endif

enum rawspeed3_error_codes
{
    rawspeed_inited = -2,
    rawspeed3_param_error = -1,
    rawspeed3_ok = 0,
    rawspeed3_ok_warnings = 1,
    rawspeed3_not_inited = 2,
    rawspeed3_processing_error = 3,
    rawspeed3_no_decoder = 4,
    rawspeed3_not_supported = 5,
};

typedef void *rawspeed3_handle_t;

/*
 * Handles are independent: different handles may decode concurrently.
 * Calls that release or close a handle must be serialized with every call on
 * that handle. The worker-count setter may overlap decode only; it affects a
 * decode only when that decode reads the new value after the setter completes.
 */

/* RAW file parsing results */
typedef struct 
{
    int status; /* -1: param error, 0 => OK, >=1 error code */
    uint16_t width, height, bpp, cpp; 
    unsigned pitch, filters;
    const void *pixeldata;
}rawspeed3_ret_t;

typedef struct
{
    size_t struct_size;
    rawspeed3_ret_t decoded;
    uint16_t predictor, compression, components, bits_per_sample;
    uint32_t tile_count;
    double parse_support_ms, decode_ms;
    char reason[256];
    uint16_t dng_tile_workers_requested, dng_tile_workers_used;
    uint32_t dng_tiles_completed;
}rawspeed3_ret_v2_t;

/* API calls */
DllDef void rawspeed3_clearresult(rawspeed3_ret_t*);
DllDef void rawspeed3_clearresult_v2(rawspeed3_ret_v2_t*);
/* 
    rawspeed3_init()
    Init rawspeed3 Camera, returns: 0 on failure, pointer to data block on success 
    Cameradefs: cameras.xml in string (is_file == false) or file (is_file == true)
*/
DllDef rawspeed3_handle_t rawspeed3_init(const char* cameradefs, bool is_file);

/* init with built-in cameras.xml */
DllDef rawspeed3_handle_t rawspeed3_initdefault();

/* Configure bounded lossless-JPEG DNG tile workers for this handle (1..4). */
DllDef int rawspeed3_set_dng_tile_workers(rawspeed3_handle_t handle,
                                          unsigned workers);

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
      1 -> decode warnings (not fatal)
      >1 -> error code
     -1 -> Incorrect parameters passed (handle, or data, or datasize)
*/
DllDef int rawspeed3_decodefile(rawspeed3_handle_t handle, rawspeed3_ret_t* resultp, 
    const void *data, size_t datasize, bool allowunknown);
DllDef int rawspeed3_decodefile_v2(rawspeed3_handle_t handle, rawspeed3_ret_v2_t* resultp,
    const void *data, size_t datasize, bool allowunknown);

/* release internal raw data buffer */
DllDef void rawspeed3_release(rawspeed3_handle_t handle);

/* close handle: release all internal data */
DllDef void rawspeed3_close(rawspeed3_handle_t handle);


#ifdef __cplusplus
} /* Extern C */

/* Synchronous caller context used only to correlate decode diagnostics. */
extern thread_local uint64_t rawspeed3_attempt_context;
#endif
