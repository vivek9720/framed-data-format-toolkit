#include "format.h"

#include <algorithm>
#include <array>
#include <limits>

namespace fdf {

FormatError::FormatError(std::string message, size_t offset)
    : std::runtime_error(message + " at offset " + std::to_string(offset)),
      offset_(offset) {}

ByteReader::ByteReader(const uint8_t* data, size_t size, size_t base_offset)
    : data_(data), size_(size), position_(0), base_offset_(base_offset) {}

ByteReader::ByteReader(const std::vector<uint8_t>& data, size_t base_offset)
    : ByteReader(data.data(), data.size(), base_offset) {}

void ByteReader::require(size_t count, std::string_view field) {
  if (count > remaining()) {
    std::string name = field.empty() ? "field" : std::string(field);
    throw FormatError("truncated " + name, offset());
  }
}

uint8_t ByteReader::readU8(std::string_view field) {
  require(1, field);
  return data_[position_++];
}

uint16_t ByteReader::readU16(std::string_view field) {
  require(2, field);
  uint16_t value = static_cast<uint16_t>(data_[position_]) |
                   (static_cast<uint16_t>(data_[position_ + 1]) << 8);
  position_ += 2;
  return value;
}

uint32_t ByteReader::readU32(std::string_view field) {
  require(4, field);
  uint32_t value = static_cast<uint32_t>(data_[position_]) |
                   (static_cast<uint32_t>(data_[position_ + 1]) << 8) |
                   (static_cast<uint32_t>(data_[position_ + 2]) << 16) |
                   (static_cast<uint32_t>(data_[position_ + 3]) << 24);
  position_ += 4;
  return value;
}

int64_t ByteReader::readI64(std::string_view field) {
  require(8, field);
  uint64_t value = 0;
  for (size_t i = 0; i < 8; ++i) {
    value |= static_cast<uint64_t>(data_[position_ + i]) << (i * 8);
  }
  position_ += 8;
  return static_cast<int64_t>(value);
}

std::vector<uint8_t> ByteReader::readBytes(size_t count,
                                           std::string_view field) {
  require(count, field);
  std::vector<uint8_t> out(data_ + position_, data_ + position_ + count);
  position_ += count;
  return out;
}

std::string ByteReader::readString(size_t count, std::string_view field) {
  std::vector<uint8_t> bytes = readBytes(count, field);
  return std::string(bytes.begin(), bytes.end());
}

void ByteReader::skip(size_t count, std::string_view field) {
  require(count, field);
  position_ += count;
}

SectionType sectionTypeFromTag(std::string_view tag) {
  if (tag == "STRT") {
    return SectionType::StringTable;
  }
  if (tag == "META") {
    return SectionType::Metadata;
  }
  if (tag == "DATA") {
    return SectionType::Data;
  }
  if (tag == "CMND") {
    return SectionType::Command;
  }
  if (tag == "EVNT") {
    return SectionType::Event;
  }
  if (tag == "XBLK") {
    return SectionType::Extended;
  }
  return SectionType::Unknown;
}

std::string sectionTag(SectionType type) {
  switch (type) {
    case SectionType::StringTable:
      return "STRT";
    case SectionType::Metadata:
      return "META";
    case SectionType::Data:
      return "DATA";
    case SectionType::Command:
      return "CMND";
    case SectionType::Event:
      return "EVNT";
    case SectionType::Extended:
      return "XBLK";
    case SectionType::Unknown:
      return "UNKN";
  }
  return "UNKN";
}

std::string encodingName(Encoding encoding) {
  switch (encoding) {
    case Encoding::Raw:
      return "raw";
    case Encoding::XorRle:
      return "xor-rle";
    case Encoding::TokenPack:
      return "token-pack";
    case Encoding::Delta:
      return "delta";
  }
  return "unknown";
}

std::string valueKindName(ValueKind kind) {
  switch (kind) {
    case ValueKind::StringRef:
      return "string";
    case ValueKind::Integer:
      return "integer";
    case ValueKind::Boolean:
      return "boolean";
    case ValueKind::Bytes:
      return "bytes";
    case ValueKind::Map:
      return "map";
  }
  return "unknown";
}

bool isKnownEncoding(uint8_t value) {
  return value <= static_cast<uint8_t>(Encoding::Delta);
}

bool isKnownValueKind(uint8_t value) {
  return value <= static_cast<uint8_t>(ValueKind::Map);
}

bool isPrintableUtf8Like(std::string_view value) {
  return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
    return ch == '\t' || ch == '\n' || ch == '\r' || ch >= 0x20;
  });
}

uint32_t rollingChecksum(const uint8_t* data, size_t size) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < size; ++i) {
    hash ^= static_cast<uint32_t>(data[i]);
    hash *= 16777619u;
    hash ^= static_cast<uint32_t>((i + 1) * 0x45d9f3bu);
    hash = (hash << 7) | (hash >> 25);
  }
  return hash;
}

uint32_t rollingChecksum(const std::vector<uint8_t>& data) {
  return rollingChecksum(data.data(), data.size());
}

void appendU16(std::vector<uint8_t>& out, uint16_t value) {
  out.push_back(static_cast<uint8_t>(value & 0xffu));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xffu));
}

void appendU32(std::vector<uint8_t>& out, uint32_t value) {
  out.push_back(static_cast<uint8_t>(value & 0xffu));
  out.push_back(static_cast<uint8_t>((value >> 8) & 0xffu));
  out.push_back(static_cast<uint8_t>((value >> 16) & 0xffu));
  out.push_back(static_cast<uint8_t>((value >> 24) & 0xffu));
}

void appendI64(std::vector<uint8_t>& out, int64_t value) {
  uint64_t raw = static_cast<uint64_t>(value);
  for (size_t i = 0; i < 8; ++i) {
    out.push_back(static_cast<uint8_t>((raw >> (i * 8)) & 0xffu));
  }
}

void appendBytes(std::vector<uint8_t>& out, const std::vector<uint8_t>& bytes) {
  out.insert(out.end(), bytes.begin(), bytes.end());
}

void appendString(std::vector<uint8_t>& out, std::string_view value) {
  out.insert(out.end(), value.begin(), value.end());
}

}  // namespace fdf
