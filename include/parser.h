#pragma once

#include <cstddef>
#include <cstdint>

#include "format.h"

namespace fdf {

struct ParserOptions {
  bool strict_checksums = false;
  bool allow_unknown_sections = true;
  bool allow_trailing_bytes = true;
  size_t max_sections = kMaxSections;
  size_t max_decoded_section_size = kMaxDecodedSectionSize;
};

class Parser {
 public:
  ParsedFile parse(const uint8_t* data, size_t size,
                   const ParserOptions& options = {}) const;
  ParsedFile parse(const std::vector<uint8_t>& data,
                   const ParserOptions& options = {}) const;

 private:
  MetadataValue parseValue(ByteReader& reader, size_t depth) const;
  MapEntry parseMapEntry(ByteReader& reader) const;
  CommandRecord parseCommand(ByteReader& reader, size_t depth) const;
  EventRecord parseEvent(ByteReader& reader) const;
  DataBlock parseDataBlock(ByteReader& reader,
                           const ParserOptions& options) const;

  void parseStringTable(ParsedFile& file, ParsedSection& section) const;
  void parseMetadata(ParsedFile& file, ParsedSection& section) const;
  void parseCommands(ParsedFile& file, ParsedSection& section) const;
  void parseEvents(ParsedFile& file, ParsedSection& section) const;
  void parseDataBlocks(ParsedFile& file, ParsedSection& section,
                       const ParserOptions& options) const;
};

ParsedFile parseDocument(const uint8_t* data, size_t size,
                         const ParserOptions& options = {});

}  // namespace fdf
