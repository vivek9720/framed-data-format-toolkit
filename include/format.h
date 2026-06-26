#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fdf {

constexpr char kMagic[4] = {'F', 'D', 'F', '2'};
constexpr uint8_t kSupportedMajorVersion = 1;
constexpr size_t kHeaderSize = 16;
constexpr size_t kSectionHeaderSize = 20;
constexpr size_t kMaxSections = 64;
constexpr size_t kMaxDecodedSectionSize = 1024 * 1024;
constexpr size_t kMaxStringLength = 4096;
constexpr size_t kMaxRecordDepth = 8;

class FormatError : public std::runtime_error {
 public:
  FormatError(std::string message, size_t offset);

  size_t offset() const noexcept { return offset_; }

 private:
  size_t offset_;
};

struct ParseWarning {
  std::string code;
  std::string message;
  size_t offset = 0;
};

enum class SectionType {
  StringTable,
  Metadata,
  Data,
  Command,
  Event,
  Extended,
  Unknown,
};

enum class Encoding : uint8_t {
  Raw = 0,
  XorRle = 1,
  TokenPack = 2,
  Delta = 3,
};

enum class ValueKind : uint8_t {
  StringRef = 0,
  Integer = 1,
  Boolean = 2,
  Bytes = 3,
  Map = 4,
};

struct Header {
  uint8_t major_version = 0;
  uint8_t minor_version = 0;
  uint16_t flags = 0;
  uint16_t section_count = 0;
  uint16_t header_size = 0;
  uint32_t file_checksum = 0;
};

struct SectionHeader {
  SectionType type = SectionType::Unknown;
  std::string tag;
  Encoding encoding = Encoding::Raw;
  uint8_t flags = 0;
  uint16_t record_count = 0;
  uint32_t decoded_length = 0;
  uint32_t payload_length = 0;
  uint32_t payload_checksum = 0;
  size_t offset = 0;
};

struct MapEntry {
  uint16_t key_index = 0;
  ValueKind kind = ValueKind::Bytes;
  uint16_t string_index = 0;
  int64_t integer = 0;
  bool boolean = false;
  std::vector<uint8_t> bytes;
};

struct MetadataValue {
  ValueKind kind = ValueKind::Bytes;
  uint16_t string_index = 0;
  int64_t integer = 0;
  bool boolean = false;
  std::vector<uint8_t> bytes;
  std::vector<MapEntry> map_entries;
};

struct MetadataRecord {
  uint16_t key_index = 0;
  MetadataValue value;
  size_t offset = 0;
};

struct CommandRecord {
  uint16_t command_id = 0;
  uint16_t target_index = 0;
  uint8_t flags = 0;
  std::vector<MetadataValue> arguments;
  std::vector<CommandRecord> children;
  size_t offset = 0;
};

struct EventRecord {
  uint32_t delta_time = 0;
  uint8_t event_kind = 0;
  uint16_t label_index = 0;
  std::vector<uint8_t> payload;
  std::vector<MetadataRecord> attributes;
  size_t offset = 0;
};

struct DataBlock {
  uint16_t name_index = 0;
  uint8_t flags = 0;
  Encoding inline_encoding = Encoding::Raw;
  uint32_t advertised_checksum = 0;
  std::vector<uint8_t> encoded_bytes;
  std::vector<uint8_t> bytes;
  bool decoded_inline = false;
  size_t offset = 0;
};

struct ParsedSection {
  SectionHeader header;
  std::vector<uint8_t> raw_payload;
  std::vector<uint8_t> decoded_payload;
  std::vector<DataBlock> data_blocks;
};

struct ParsedFile {
  Header header;
  std::vector<ParsedSection> sections;
  std::vector<std::string> string_table;
  std::vector<MetadataRecord> metadata;
  std::vector<CommandRecord> commands;
  std::vector<EventRecord> events;
  std::vector<DataBlock> data_blocks;
  std::vector<ParseWarning> warnings;
};

class ByteReader {
 public:
  ByteReader(const uint8_t* data, size_t size, size_t base_offset = 0);
  explicit ByteReader(const std::vector<uint8_t>& data, size_t base_offset = 0);

  bool empty() const noexcept { return position_ == size_; }
  size_t remaining() const noexcept { return size_ - position_; }
  size_t offset() const noexcept { return base_offset_ + position_; }
  size_t localOffset() const noexcept { return position_; }

  uint8_t readU8(std::string_view field = {});
  uint16_t readU16(std::string_view field = {});
  uint32_t readU32(std::string_view field = {});
  int64_t readI64(std::string_view field = {});
  std::vector<uint8_t> readBytes(size_t count, std::string_view field = {});
  std::string readString(size_t count, std::string_view field = {});
  void skip(size_t count, std::string_view field = {});

 private:
  void require(size_t count, std::string_view field);

  const uint8_t* data_;
  size_t size_;
  size_t position_;
  size_t base_offset_;
};

SectionType sectionTypeFromTag(std::string_view tag);
std::string sectionTag(SectionType type);
std::string encodingName(Encoding encoding);
std::string valueKindName(ValueKind kind);
bool isKnownEncoding(uint8_t value);
bool isKnownValueKind(uint8_t value);
bool isPrintableUtf8Like(std::string_view value);

uint32_t rollingChecksum(const uint8_t* data, size_t size);
uint32_t rollingChecksum(const std::vector<uint8_t>& data);

void appendU16(std::vector<uint8_t>& out, uint16_t value);
void appendU32(std::vector<uint8_t>& out, uint32_t value);
void appendI64(std::vector<uint8_t>& out, int64_t value);
void appendBytes(std::vector<uint8_t>& out, const std::vector<uint8_t>& bytes);
void appendString(std::vector<uint8_t>& out, std::string_view value);

}  // namespace fdf
