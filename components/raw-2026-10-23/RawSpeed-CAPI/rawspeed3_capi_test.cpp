/* LGPL component modification notice: Modified by SUEN and renhui sun on 2026-07-12. This source-package comment does not change behavior. */
#include "rawspeed3_capi.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <stdint.h>

namespace {
void write16(std::vector<uint8_t>& data, size_t offset, uint16_t value) {
    data[offset] = uint8_t(value);
    data[offset + 1] = uint8_t(value >> 8);
}

void write32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    data[offset] = uint8_t(value);
    data[offset + 1] = uint8_t(value >> 8);
    data[offset + 2] = uint8_t(value >> 16);
    data[offset + 3] = uint8_t(value >> 24);
}

std::vector<uint8_t> makeSelfReferencingSubIfd() {
    std::vector<uint8_t> data(26, 0);
    data[0] = 'I'; data[1] = 'I';
    write16(data, 2, 42);
    write32(data, 4, 8);
    write16(data, 8, 1);
    write16(data, 10, 330);
    write16(data, 12, 4);
    write32(data, 14, 1);
    write32(data, 18, 8);
    return data;
}

std::vector<uint8_t> makeOverBudgetIfd() {
    constexpr uint16_t entries = 65535;
    std::vector<uint8_t> data(8U + 2U + size_t(entries) * 12U + 4U, 0);
    data[0] = 'I'; data[1] = 'I';
    write16(data, 2, 42);
    write32(data, 4, 8);
    write16(data, 8, entries);
    return data;
}

bool diagnosticWasBounded(rawspeed3_handle_t handle, const std::vector<uint8_t>& data) {
    rawspeed3_ret_v2_t result = {};
    result.struct_size = sizeof(result);
    const int status = rawspeed3_decodefile_v2(handle, &result, data.data(), data.size(), true);
    rawspeed3_release(handle);
    return status == result.decoded.status && status != rawspeed3_ok
        && std::string(result.reason).find("TIFF diagnostic scan stopped") != std::string::npos;
}
} // namespace



std::vector<uint8_t> readfile(char *fn)
{
    std::ifstream is(fn,std::ifstream::binary);
    if(is)
    {
      is.seekg(0, is.end);
      size_t length = is.tellg();
      is.seekg(0, is.beg);
      std::vector<uint8_t> ret;
      ret.resize(length+1);
      is.read ((char*)ret.data(),length);
      is.close();
      ret[length] = 0; // zero terminated
      std::cout << "File: " << fn << "  size:" << ret.size() << std::endl;
      return ret;
    }
    else
        return std::vector<uint8_t>();
}


int main(int ac, char *av[])
{
    bool predictor7Mode = false;
    int firstFile = 1;
    if (ac > 1 && std::string(av[1]) == "--predictor7") {
        predictor7Mode = true;
        firstFile = 2;
    }
    if(ac <= firstFile)
    {
        std::cout << "Usage: " << av[0] << " [--predictor7] rawfile rawfile2 ...." << std::endl;
        return 1;
    }
    rawspeed3_handle_t  handle = rawspeed3_initdefault();
    if(!handle)
    {
      std::cerr << "Unable to init rs3" << std::endl;
      return 2;
    }

    int failures = 0;

    if (rawspeed3_set_dng_tile_workers(nullptr, 3) != rawspeed3_param_error
        || rawspeed3_set_dng_tile_workers(handle, 0) != rawspeed3_param_error
        || rawspeed3_set_dng_tile_workers(handle, 5) != rawspeed3_param_error
        || rawspeed3_set_dng_tile_workers(handle, 3) != rawspeed3_ok) {
        std::cerr << "DNG tile worker configuration contract failed" << std::endl;
        ++failures;
    }

    if (!diagnosticWasBounded(handle, makeSelfReferencingSubIfd())) {
        std::cerr << "Self-referencing SubIFD diagnostic was not bounded and marked" << std::endl;
        ++failures;
    }
    if (!diagnosticWasBounded(handle, makeOverBudgetIfd())) {
        std::cerr << "Over-budget TIFF diagnostic was not bounded and marked" << std::endl;
        ++failures;
    }

    uint8_t dummy = 0;
    rawspeed3_ret_v2_t shortResult = {};
    shortResult.struct_size = sizeof(size_t);
    if (rawspeed3_decodefile_v2(handle, &shortResult, &dummy, 1, true) != rawspeed3_param_error)
        ++failures;

    rawspeed3_ret_v2_t paramResult = {};
    paramResult.struct_size = sizeof(paramResult);
    if (rawspeed3_decodefile_v2(handle, &paramResult, nullptr, 0, true) != rawspeed3_param_error
        || std::string(paramResult.reason).empty())
        ++failures;

    rawspeed3_ret_t v1Param = {};
    if (rawspeed3_decodefile(handle, &v1Param, nullptr, 0, true) != rawspeed3_param_error
        || v1Param.status != rawspeed3_param_error)
        ++failures;

    for(int i = firstFile; i < ac; i++)
    {
       std::vector<uint8_t> rawdata = readfile(av[i]);
       if (rawdata.size() < 100) {
         std::cerr << "Input file " << av[i] << " too small or nonexistent" << std::endl;
         ++failures;
         continue;
       }
       rawspeed3_ret_v2_t result = {};
       result.struct_size = sizeof(result);
       int q = rawspeed3_decodefile_v2(handle, &result, rawdata.data(),
                                    rawdata.size(), true);
       const std::string reason = result.reason;
       if (predictor7Mode && !(q == rawspeed3_processing_error
             && result.decoded.status == rawspeed3_processing_error
             && reason.find("predictor 7") != std::string::npos
             && result.predictor == 7 && result.compression == 7
             && result.components == 3 && result.bits_per_sample == 12
             && result.tile_count == 48
             && result.parse_support_ms > 0.0 && result.decode_ms > 0.0)) {
         std::cerr << "Expected status=3 predictor=7 compression=7 components=3 bps=12 tiles=48 with parse/decode timings; got status="
                   << q << " resultStatus=" << result.decoded.status
                   << " reason=" << reason << " predictor=" << result.predictor
                   << " compression=" << result.compression << " components=" << result.components
                   << " bps=" << result.bits_per_sample << " tiles=" << result.tile_count
                   << " parseSupportMs=" << result.parse_support_ms << " decodeMs=" << result.decode_ms
                   << std::endl;
         ++failures;
       }
       if (q >= rawspeed3_ok && q <= rawspeed3_ok_warnings) {
         std::cout << "File decoded code=" << result.decoded.status <<" width=" << result.decoded.width
                   << " height=" << result.decoded.height
                   << " pitch=" << result.decoded.pitch
                   << " filters=" << std::hex << result.decoded.filters << std::dec
                   << " channels=" << result.decoded.cpp
                   << " bpp=" << result.decoded.bpp
                   << std::endl;
       } else
         std::cout << "RawSpeed wrapper error code:" << result.decoded.status
                   << std::endl;

       rawspeed3_release(handle);

       rawspeed3_ret_t v1 = {};
       const int v1Status = rawspeed3_decodefile(handle, &v1, rawdata.data(), rawdata.size(), true);
       if (v1Status != q || v1.status != result.decoded.status) {
         std::cerr << "V1/V2 status mismatch: v1=" << v1Status << " v2=" << q << std::endl;
         ++failures;
       }
       rawspeed3_release(handle);
    }

    rawspeed3_close(handle);

    return failures == 0 ? 0 : 3;
}
