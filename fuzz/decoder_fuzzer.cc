#include <cstddef>
#include <cstdint>
#include <exception>
#include <vector>

#include "decoder.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0) {
    return 0;
  }

  fdf::Decoder decoder;
  fdf::DecodeOptions options;
  options.max_output_size = 128 * 1024;
  const size_t expected_size =
      size > 2 ? (static_cast<size_t>(data[1]) |
                  (static_cast<size_t>(data[2]) << 8))
               : 0;

  for (uint8_t encoding = 0;
       encoding <= static_cast<uint8_t>(fdf::Encoding::Delta); ++encoding) {
    try {
      (void)decoder.decodeBlock(static_cast<fdf::Encoding>(encoding), data,
                                size, expected_size, options);
    } catch (const std::exception&) {
    }
  }

  return 0;
}
