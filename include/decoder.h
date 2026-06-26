#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "format.h"

namespace fdf {

struct DecodeOptions {
  size_t max_output_size = kMaxDecodedSectionSize;
  bool require_exact_size = false;
};

struct DecodeResult {
  std::vector<uint8_t> bytes;
  std::vector<ParseWarning> warnings;
};

class Decoder {
 public:
  DecodeResult decodeBlock(Encoding encoding, const uint8_t* data, size_t size,
                           size_t expected_size,
                           const DecodeOptions& options = {}) const;

  DecodeResult decodeBlock(Encoding encoding, const std::vector<uint8_t>& data,
                           size_t expected_size,
                           const DecodeOptions& options = {}) const;

 private:
  DecodeResult decodeRaw(const uint8_t* data, size_t size,
                         const DecodeOptions& options) const;
  DecodeResult decodeXorRle(const uint8_t* data, size_t size,
                            const DecodeOptions& options) const;
  DecodeResult decodeTokenPack(const uint8_t* data, size_t size,
                               const DecodeOptions& options) const;
  DecodeResult decodeDelta(const uint8_t* data, size_t size,
                           const DecodeOptions& options) const;
  static void appendChecked(std::vector<uint8_t>& out, uint8_t value,
                            size_t max_output_size, size_t offset);
  static void appendChecked(std::vector<uint8_t>& out,
                            const std::vector<uint8_t>& bytes,
                            size_t max_output_size, size_t offset);
};

}  // namespace fdf
