#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "format.h"

namespace fdf {

struct DocumentMapEntry {
  std::string key;
  ValueKind kind = ValueKind::Bytes;
  std::string text;
  int64_t integer = 0;
  bool boolean = false;
  std::vector<uint8_t> bytes;
};

struct DocumentValue {
  ValueKind kind = ValueKind::Bytes;
  std::string text;
  int64_t integer = 0;
  bool boolean = false;
  std::vector<uint8_t> bytes;
  std::vector<DocumentMapEntry> map_entries;
};

struct DocumentCommand {
  std::string command_name;
  std::string target;
  uint8_t flags = 0;
  std::vector<DocumentValue> arguments;
  std::vector<DocumentCommand> children;
};

struct DocumentEvent {
  uint32_t delta_time = 0;
  std::string event_name;
  std::string label;
  std::vector<uint8_t> payload;
  std::vector<std::pair<std::string, DocumentValue>> attributes;
};

struct Document {
  uint8_t major_version = 0;
  uint8_t minor_version = 0;
  std::vector<std::string> strings;
  std::map<std::string, DocumentValue> metadata;
  std::vector<DocumentCommand> commands;
  std::vector<DocumentEvent> events;
  std::vector<DataBlock> data_blocks;
  std::vector<std::string> diagnostics;
};

class Deserializer {
 public:
  Document build(const ParsedFile& parsed) const;

 private:
  DocumentValue convertValue(const ParsedFile& parsed,
                             const MetadataValue& value) const;
  DocumentMapEntry convertMapEntry(const ParsedFile& parsed,
                                   const MapEntry& entry) const;
  DocumentCommand convertCommand(const ParsedFile& parsed,
                                 const CommandRecord& command) const;
  DocumentEvent convertEvent(const ParsedFile& parsed,
                             const EventRecord& event) const;
  std::string resolveString(const ParsedFile& parsed, uint16_t index,
                            std::string fallback_prefix) const;
  std::string commandName(uint16_t command_id) const;
  std::string eventName(uint8_t event_kind) const;
};

}  // namespace fdf
