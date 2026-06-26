#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

#include "decoder.h"

namespace {

std::string asString(const std::vector<uint8_t>& bytes) {
  return std::string(bytes.begin(), bytes.end());
}

}  // namespace

int main() {
  fdf::Decoder decoder;
  fdf::DecodeOptions options;
  options.max_output_size = 1024;

  const std::vector<uint8_t> raw = {'r', 'a', 'w'};
  assert(asString(decoder.decodeBlock(fdf::Encoding::Raw, raw, raw.size(),
                                      options)
                      .bytes) == "raw");

  const std::vector<uint8_t> xor_rle = {0x20, 0x02, 0x41, 0x42, 0x43,
                                        0x82, 0x5a};
  assert(asString(decoder.decodeBlock(fdf::Encoding::XorRle, xor_rle, 6,
                                      options)
                      .bytes) == "abczzz");

  const std::vector<uint8_t> token_pack = {0x01, 0x05, 'm', 'e', 't', 'a',
                                           ':',  0x90, 0x01, 'A', 'B'};
  assert(asString(decoder.decodeBlock(fdf::Encoding::TokenPack, token_pack, 12,
                                      options)
                      .bytes) == "meta:meta:AB");

  const std::vector<uint8_t> delta = {100, 1, 255, 2};
  const auto decoded_delta =
      decoder.decodeBlock(fdf::Encoding::Delta, delta, 4, options).bytes;
  assert(decoded_delta.size() == 4);
  assert(decoded_delta[0] == 100);
  assert(decoded_delta[1] == 101);
  assert(decoded_delta[2] == 100);
  assert(decoded_delta[3] == 102);

  bool threw = false;
  try {
    const std::vector<uint8_t> bad_token = {0x00, 0x80};
    decoder.decodeBlock(fdf::Encoding::TokenPack, bad_token, 0, options);
  } catch (const std::exception&) {
    threw = true;
  }
  assert(threw);

  return 0;
}
