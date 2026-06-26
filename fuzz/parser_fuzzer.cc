#include <cstddef>
#include <cstdint>
#include <exception>

#include "parser.h"
#include "validator.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  try {
    fdf::ParserOptions options;
    options.strict_checksums = false;
    options.allow_unknown_sections = true;
    options.allow_trailing_bytes = true;
    options.max_decoded_section_size = 1024 * 1024;

    const fdf::ParsedFile parsed = fdf::Parser().parse(data, size, options);
    (void)fdf::Validator().validate(parsed);
  } catch (const std::exception&) {
  }
  return 0;
}
