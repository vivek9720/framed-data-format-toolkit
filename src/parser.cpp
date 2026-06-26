#include "parser.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

#include "decoder.h"

namespace fdf {

namespace {

MetadataValue valueFromMapEntry(const MapEntry& entry) {
  MetadataValue value;
  value.kind = entry.kind;
  value.string_index = entry.string_index;
  value.integer = entry.integer;
  value.boolean = entry.boolean;
  value.bytes = entry.bytes;
  return value;
}

void appendWarnings(ParsedFile& file, const std::vector<ParseWarning>& warnings,
                    size_t offset) {
  for (ParseWarning warning : warnings) {
    warning.offset += offset;
    file.warnings.push_back(std::move(warning));
  }
}

}  // namespace

ParsedFile Parser::parse(const uint8_t* data, size_t size,
                         const ParserOptions& options) const {
  if (size < kHeaderSize) {
    throw FormatError("file is shorter than the fixed header", size);
  }

  ByteReader reader(data, size);
  ParsedFile file;

  const std::string magic = reader.readString(4, "magic");
  if (magic.size() != 4 ||
      std::memcmp(magic.data(), kMagic, sizeof(kMagic)) != 0) {
    throw FormatError("invalid FDF magic header", 0);
  }

  file.header.major_version = reader.readU8("major version");
  file.header.minor_version = reader.readU8("minor version");
  file.header.flags = reader.readU16("file flags");
  file.header.section_count = reader.readU16("section count");
  file.header.header_size = reader.readU16("header size");
  file.header.file_checksum = reader.readU32("file checksum");

  if (file.header.major_version != kSupportedMajorVersion) {
    throw FormatError("unsupported major version", 4);
  }
  if (file.header.header_size < kHeaderSize) {
    throw FormatError("header size is smaller than the fixed header", 10);
  }
  if (file.header.header_size > size) {
    throw FormatError("header size extends beyond input", 10);
  }
  if (file.header.section_count > options.max_sections) {
    throw FormatError("section count exceeds parser limit", 8);
  }

  if (file.header.header_size > kHeaderSize) {
    reader.skip(file.header.header_size - kHeaderSize, "extended header");
  }

  if (file.header.file_checksum != 0) {
    const uint32_t observed =
        rollingChecksum(data + file.header.header_size,
                        size - file.header.header_size);
    if (observed != file.header.file_checksum) {
      if (options.strict_checksums) {
        throw FormatError("file checksum mismatch", 12);
      }
      file.warnings.push_back(
          {"file-checksum-mismatch", "file checksum does not match body", 12});
    }
  }

  Decoder decoder;
  DecodeOptions decode_options;
  decode_options.max_output_size = options.max_decoded_section_size;
  decode_options.require_exact_size = false;

  for (uint16_t i = 0; i < file.header.section_count; ++i) {
    if (reader.remaining() < kSectionHeaderSize) {
      throw FormatError("truncated section header", reader.offset());
    }

    ParsedSection section;
    section.header.offset = reader.offset();
    section.header.tag = reader.readString(4, "section tag");
    section.header.type = sectionTypeFromTag(section.header.tag);

    const uint8_t encoding = reader.readU8("section encoding");
    if (!isKnownEncoding(encoding)) {
      throw FormatError("unknown section encoding", reader.offset() - 1);
    }
    section.header.encoding = static_cast<Encoding>(encoding);
    section.header.flags = reader.readU8("section flags");
    section.header.record_count = reader.readU16("record count");
    section.header.decoded_length = reader.readU32("decoded length");
    section.header.payload_length = reader.readU32("payload length");
    section.header.payload_checksum = reader.readU32("payload checksum");

    if (section.header.type == SectionType::Unknown &&
        !options.allow_unknown_sections) {
      throw FormatError("unknown section tag", section.header.offset);
    }
    if (section.header.decoded_length > options.max_decoded_section_size) {
      throw FormatError("decoded section length exceeds parser limit",
                        section.header.offset + 8);
    }

    section.raw_payload =
        reader.readBytes(section.header.payload_length, "section payload");
    if (section.header.payload_checksum != 0 &&
        rollingChecksum(section.raw_payload) !=
            section.header.payload_checksum) {
      if (options.strict_checksums) {
        throw FormatError("section payload checksum mismatch",
                          section.header.offset + 16);
      }
      file.warnings.push_back({"section-checksum-mismatch",
                               "section payload checksum does not match",
                               section.header.offset + 16});
    }

    DecodeResult decoded =
        decoder.decodeBlock(section.header.encoding, section.raw_payload,
                            section.header.decoded_length, decode_options);
    appendWarnings(file, decoded.warnings, section.header.offset);
    section.decoded_payload = std::move(decoded.bytes);

    switch (section.header.type) {
      case SectionType::StringTable:
        parseStringTable(file, section);
        break;
      case SectionType::Metadata:
        parseMetadata(file, section);
        break;
      case SectionType::Command:
        parseCommands(file, section);
        break;
      case SectionType::Event:
        parseEvents(file, section);
        break;
      case SectionType::Data:
      case SectionType::Extended:
        parseDataBlocks(file, section, options);
        break;
      case SectionType::Unknown:
        break;
    }

    file.sections.push_back(std::move(section));
  }

  if (!reader.empty()) {
    if (!options.allow_trailing_bytes) {
      throw FormatError("trailing bytes after final section", reader.offset());
    }
    file.warnings.push_back({"trailing-bytes",
                             "bytes remain after declared section list",
                             reader.offset()});
  }

  return file;
}

ParsedFile Parser::parse(const std::vector<uint8_t>& data,
                         const ParserOptions& options) const {
  return parse(data.data(), data.size(), options);
}

MetadataValue Parser::parseValue(ByteReader& reader, size_t depth) const {
  if (depth > kMaxRecordDepth) {
    throw FormatError("nested value depth exceeds parser limit",
                      reader.offset());
  }

  const uint8_t kind_byte = reader.readU8("value kind");
  if (!isKnownValueKind(kind_byte)) {
    throw FormatError("unknown metadata value kind", reader.offset() - 1);
  }

  MetadataValue value;
  value.kind = static_cast<ValueKind>(kind_byte);
  switch (value.kind) {
    case ValueKind::StringRef:
      value.string_index = reader.readU16("string reference");
      break;
    case ValueKind::Integer:
      value.integer = reader.readI64("integer value");
      break;
    case ValueKind::Boolean:
      value.boolean = reader.readU8("boolean value") != 0;
      break;
    case ValueKind::Bytes: {
      const uint16_t length = reader.readU16("byte value length");
      value.bytes = reader.readBytes(length, "byte value");
      break;
    }
    case ValueKind::Map: {
      const uint8_t count = reader.readU8("map entry count");
      if (count > 48) {
        throw FormatError("map entry count exceeds parser limit",
                          reader.offset() - 1);
      }
      value.map_entries.reserve(count);
      for (uint8_t i = 0; i < count; ++i) {
        value.map_entries.push_back(parseMapEntry(reader));
      }
      break;
    }
  }
  return value;
}

MapEntry Parser::parseMapEntry(ByteReader& reader) const {
  MapEntry entry;
  entry.key_index = reader.readU16("map key");
  const uint8_t kind_byte = reader.readU8("map value kind");
  if (!isKnownValueKind(kind_byte)) {
    throw FormatError("unknown map value kind", reader.offset() - 1);
  }
  entry.kind = static_cast<ValueKind>(kind_byte);
  if (entry.kind == ValueKind::Map) {
    throw FormatError("nested maps are not valid in metadata records",
                      reader.offset() - 1);
  }

  switch (entry.kind) {
    case ValueKind::StringRef:
      entry.string_index = reader.readU16("map string reference");
      break;
    case ValueKind::Integer:
      entry.integer = reader.readI64("map integer value");
      break;
    case ValueKind::Boolean:
      entry.boolean = reader.readU8("map boolean value") != 0;
      break;
    case ValueKind::Bytes: {
      const uint16_t length = reader.readU16("map byte value length");
      entry.bytes = reader.readBytes(length, "map byte value");
      break;
    }
    case ValueKind::Map:
      break;
  }
  return entry;
}

CommandRecord Parser::parseCommand(ByteReader& reader, size_t depth) const {
  if (depth > kMaxRecordDepth) {
    throw FormatError("command nesting exceeds parser limit", reader.offset());
  }

  CommandRecord command;
  command.offset = reader.offset();
  command.command_id = reader.readU16("command id");
  command.target_index = reader.readU16("command target");
  command.flags = reader.readU8("command flags");

  const uint8_t argument_count = reader.readU8("command argument count");
  if (argument_count > 32) {
    throw FormatError("command argument count exceeds parser limit",
                      reader.offset() - 1);
  }
  command.arguments.reserve(argument_count);
  for (uint8_t i = 0; i < argument_count; ++i) {
    command.arguments.push_back(parseValue(reader, depth + 1));
  }

  const uint8_t child_count = reader.readU8("command child count");
  if (child_count > 32) {
    throw FormatError("command child count exceeds parser limit",
                      reader.offset() - 1);
  }
  command.children.reserve(child_count);
  for (uint8_t i = 0; i < child_count; ++i) {
    command.children.push_back(parseCommand(reader, depth + 1));
  }
  return command;
}

EventRecord Parser::parseEvent(ByteReader& reader) const {
  EventRecord event;
  event.offset = reader.offset();
  event.delta_time = reader.readU32("event delta time");
  event.event_kind = reader.readU8("event kind");
  event.label_index = reader.readU16("event label");

  const uint16_t payload_length = reader.readU16("event payload length");
  event.payload = reader.readBytes(payload_length, "event payload");

  const uint8_t attribute_count = reader.readU8("event attribute count");
  if (attribute_count > 32) {
    throw FormatError("event attribute count exceeds parser limit",
                      reader.offset() - 1);
  }
  event.attributes.reserve(attribute_count);
  for (uint8_t i = 0; i < attribute_count; ++i) {
    MapEntry entry = parseMapEntry(reader);
    MetadataRecord record;
    record.key_index = entry.key_index;
    record.value = valueFromMapEntry(entry);
    record.offset = reader.offset();
    event.attributes.push_back(std::move(record));
  }
  return event;
}

DataBlock Parser::parseDataBlock(ByteReader& reader,
                                 const ParserOptions& options) const {
  DataBlock block;
  block.offset = reader.offset();
  block.name_index = reader.readU16("data block name");
  block.flags = reader.readU8("data block flags");

  if ((block.flags & 0x01u) != 0) {
    const uint8_t encoding_byte = reader.readU8("inline block encoding");
    if (!isKnownEncoding(encoding_byte)) {
      throw FormatError("unknown inline block encoding", reader.offset() - 1);
    }
    block.inline_encoding = static_cast<Encoding>(encoding_byte);
    const uint16_t decoded_length = reader.readU16("inline decoded length");
    const uint16_t encoded_length = reader.readU16("inline encoded length");
    block.advertised_checksum = reader.readU32("inline checksum");
    block.encoded_bytes = reader.readBytes(encoded_length, "inline data");

    DecodeOptions decode_options;
    decode_options.max_output_size = options.max_decoded_section_size;
    Decoder decoder;
    DecodeResult decoded = decoder.decodeBlock(
        block.inline_encoding, block.encoded_bytes, decoded_length,
        decode_options);
    block.bytes = std::move(decoded.bytes);
    block.decoded_inline = true;
  } else {
    const uint16_t length = reader.readU16("data block length");
    block.advertised_checksum = reader.readU32("data block checksum");
    block.bytes = reader.readBytes(length, "data block bytes");
  }

  if (options.strict_checksums && block.advertised_checksum != 0 &&
      rollingChecksum(block.bytes) != block.advertised_checksum) {
    throw FormatError("data block checksum mismatch", block.offset);
  }
  return block;
}

void Parser::parseStringTable(ParsedFile& file, ParsedSection& section) const {
  ByteReader reader(section.decoded_payload, section.header.offset);
  if (reader.empty()) {
    return;
  }

  const uint16_t count = reader.readU16("string table count");
  if (section.header.record_count != 0 &&
      section.header.record_count != count) {
    file.warnings.push_back({"string-count-mismatch",
                             "string table count differs from section header",
                             section.header.offset});
  }

  for (uint16_t i = 0; i < count; ++i) {
    const uint16_t length = reader.readU16("string length");
    if (length > kMaxStringLength) {
      throw FormatError("string length exceeds parser limit", reader.offset());
    }
    std::string value = reader.readString(length, "string bytes");
    if (!isPrintableUtf8Like(value)) {
      file.warnings.push_back({"nonprintable-string",
                               "string table entry contains control bytes",
                               reader.offset()});
    }
    file.string_table.push_back(std::move(value));
  }

  if (!reader.empty()) {
    file.warnings.push_back({"string-table-padding",
                             "string table payload has trailing bytes",
                             reader.offset()});
  }
}

void Parser::parseMetadata(ParsedFile& file, ParsedSection& section) const {
  ByteReader reader(section.decoded_payload, section.header.offset);
  size_t parsed = 0;
  while ((section.header.record_count == 0 && !reader.empty()) ||
         parsed < section.header.record_count) {
    MetadataRecord record;
    record.offset = reader.offset();
    record.key_index = reader.readU16("metadata key");
    record.value = parseValue(reader, 0);
    file.metadata.push_back(std::move(record));
    ++parsed;
  }

  if (!reader.empty()) {
    file.warnings.push_back({"metadata-padding",
                             "metadata payload has trailing bytes",
                             reader.offset()});
  }
}

void Parser::parseCommands(ParsedFile& file, ParsedSection& section) const {
  ByteReader reader(section.decoded_payload, section.header.offset);
  size_t parsed = 0;
  while ((section.header.record_count == 0 && !reader.empty()) ||
         parsed < section.header.record_count) {
    file.commands.push_back(parseCommand(reader, 0));
    ++parsed;
  }

  if (!reader.empty()) {
    file.warnings.push_back({"command-padding",
                             "command payload has trailing bytes",
                             reader.offset()});
  }
}

void Parser::parseEvents(ParsedFile& file, ParsedSection& section) const {
  ByteReader reader(section.decoded_payload, section.header.offset);
  size_t parsed = 0;
  while ((section.header.record_count == 0 && !reader.empty()) ||
         parsed < section.header.record_count) {
    file.events.push_back(parseEvent(reader));
    ++parsed;
  }

  if (!reader.empty()) {
    file.warnings.push_back(
        {"event-padding", "event payload has trailing bytes", reader.offset()});
  }
}

void Parser::parseDataBlocks(ParsedFile& file, ParsedSection& section,
                             const ParserOptions& options) const {
  ByteReader reader(section.decoded_payload, section.header.offset);
  size_t parsed = 0;
  while ((section.header.record_count == 0 && !reader.empty()) ||
         parsed < section.header.record_count) {
    DataBlock block = parseDataBlock(reader, options);
    if (block.advertised_checksum != 0 &&
        rollingChecksum(block.bytes) != block.advertised_checksum) {
      file.warnings.push_back({"data-checksum-mismatch",
                               "data block checksum does not match",
                               block.offset});
    }
    file.data_blocks.push_back(block);
    section.data_blocks.push_back(std::move(block));
    ++parsed;
  }

  if (!reader.empty()) {
    file.warnings.push_back({"data-padding",
                             "data section payload has trailing bytes",
                             reader.offset()});
  }
}

ParsedFile parseDocument(const uint8_t* data, size_t size,
                         const ParserOptions& options) {
  return Parser().parse(data, size, options);
}

}  // namespace fdf
