#include <cstddef>
#include <cstdint>
#include <exception>

#include "deserializer.h"
#include "parser.h"
#include "validator.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  try {
    fdf::ParserOptions options;
    options.strict_checksums = false;
    options.allow_unknown_sections = true;
    options.max_decoded_section_size = 1024 * 1024;

    const fdf::ParsedFile parsed = fdf::Parser().parse(data, size, options);
    const fdf::Document document = fdf::Deserializer().build(parsed);
    (void)fdf::Validator().validate(document);
  } catch (const std::exception&) {
  }
  return 0;
}
