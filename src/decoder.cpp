#include "decoder.h"

#include <algorithm>
#include <limits>
#include <string>

namespace fdf {

namespace {

void checkExpectedSize(DecodeResult& result, size_t expected_size,
                       const DecodeOptions& options) {
  if (expected_size == 0 || result.bytes.size() == expected_size) {
    return;
  }

  result.warnings.push_back({"decoded-size-mismatch",
                             "decoded block length does not match the section "
                             "descriptor",
                             result.bytes.size()});

  if (options.require_exact_size) {
    throw FormatError("decoded block length mismatch", result.bytes.size());
  }
}

}  // namespace

DecodeResult Decoder::decodeBlock(Encoding encoding, const uint8_t* data,
                                  size_t size, size_t expected_size,
                                  const DecodeOptions& options) const {
  DecodeResult result;
  switch (encoding) {
    case Encoding::Raw:
      result = decodeRaw(data, size, options);
      break;
    case Encoding::XorRle:
      result = decodeXorRle(data, size, options);
      break;
    case Encoding::TokenPack:
      result = decodeTokenPack(data, size, options);
      break;
    case Encoding::Delta:
      result = decodeDelta(data, size, options);
      break;
  }
  checkExpectedSize(result, expected_size, options);
  return result;
}

DecodeResult Decoder::decodeBlock(Encoding encoding,
                                  const std::vector<uint8_t>& data,
                                  size_t expected_size,
                                  const DecodeOptions& options) const {
  return decodeBlock(encoding, data.data(), data.size(), expected_size,
                     options);
}

DecodeResult Decoder::decodeRaw(const uint8_t* data, size_t size,
                                const DecodeOptions& options) const {
  if (size > options.max_output_size) {
    throw FormatError("raw block exceeds maximum decoded size", 0);
  }
  if (size == 0) {
    return {};
  }
  return {std::vector<uint8_t>(data, data + size), {}};
}

DecodeResult Decoder::decodeXorRle(const uint8_t* data, size_t size,
                                   const DecodeOptions& options) const {
  DecodeResult result;
  if (size == 0) {
    return result;
  }

  const uint8_t key = data[0];
  size_t position = 1;
  while (position < size) {
    const uint8_t op = data[position++];
    const size_t count = static_cast<size_t>(op & 0x7fu) + 1u;
    if ((op & 0x80u) != 0) {
      if (position >= size) {
        throw FormatError("truncated xor-rle repeat value", position);
      }
      const uint8_t value = static_cast<uint8_t>(data[position++] ^ key);
      for (size_t i = 0; i < count; ++i) {
        appendChecked(result.bytes, value, options.max_output_size, position);
      }
    } else {
      if (count > size - position) {
        throw FormatError("truncated xor-rle literal span", position);
      }
      for (size_t i = 0; i < count; ++i) {
        appendChecked(result.bytes, static_cast<uint8_t>(data[position++] ^ key),
                      options.max_output_size, position);
      }
    }
  }
  return result;
}

DecodeResult Decoder::decodeTokenPack(const uint8_t* data, size_t size,
                                      const DecodeOptions& options) const {
  DecodeResult result;
  if (size == 0) {
    return result;
  }

  size_t position = 0;
  const uint8_t table_count = static_cast<uint8_t>(data[position++] & 0x0fu);
  std::vector<std::vector<uint8_t>> table;
  table.reserve(table_count);
  for (uint8_t i = 0; i < table_count; ++i) {
    if (position >= size) {
      throw FormatError("truncated token table length", position);
    }
    const uint8_t length = data[position++];
    if (length > 32) {
      throw FormatError("token entry exceeds maximum token length", position);
    }
    if (length > size - position) {
      throw FormatError("truncated token table entry", position);
    }
    table.emplace_back(data + position, data + position + length);
    position += length;
  }

  while (position < size) {
    const uint8_t op = data[position++];
    if ((op & 0x80u) != 0) {
      const uint8_t token_index = static_cast<uint8_t>(op & 0x0fu);
      const size_t repeat = static_cast<size_t>((op >> 4u) & 0x07u) + 1u;
      if (token_index >= table.size()) {
        throw FormatError("token-pack command references missing token",
                          position - 1);
      }
      for (size_t i = 0; i < repeat; ++i) {
        appendChecked(result.bytes, table[token_index],
                      options.max_output_size, position);
      }
    } else {
      const size_t literal_length = static_cast<size_t>(op & 0x3fu) + 1u;
      if (literal_length > size - position) {
        throw FormatError("truncated token-pack literal", position);
      }
      for (size_t i = 0; i < literal_length; ++i) {
        appendChecked(result.bytes, data[position++], options.max_output_size,
                      position);
      }
    }
  }
  return result;
}

DecodeResult Decoder::decodeDelta(const uint8_t* data, size_t size,
                                  const DecodeOptions& options) const {
  DecodeResult result;
  if (size == 0) {
    return result;
  }

  uint8_t current = data[0];
  appendChecked(result.bytes, current, options.max_output_size, 0);
  for (size_t position = 1; position < size; ++position) {
    const int8_t delta = static_cast<int8_t>(data[position]);
    current = static_cast<uint8_t>(current + delta);
    appendChecked(result.bytes, current, options.max_output_size, position);
  }
  return result;
}

void Decoder::appendChecked(std::vector<uint8_t>& out, uint8_t value,
                            size_t max_output_size, size_t offset) {
  if (out.size() >= max_output_size) {
    throw FormatError("decoded block exceeds maximum output size", offset);
  }
  out.push_back(value);
}

void Decoder::appendChecked(std::vector<uint8_t>& out,
                            const std::vector<uint8_t>& bytes,
                            size_t max_output_size, size_t offset) {
  if (bytes.size() > max_output_size - out.size()) {
    throw FormatError("decoded token block exceeds maximum output size", offset);
  }
  out.insert(out.end(), bytes.begin(), bytes.end());
}

}  // namespace fdf
