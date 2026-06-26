#include "deserializer.h"

#include <sstream>

namespace fdf {

Document Deserializer::build(const ParsedFile& parsed) const {
  Document document;
  document.major_version = parsed.header.major_version;
  document.minor_version = parsed.header.minor_version;
  document.strings = parsed.string_table;
  document.data_blocks = parsed.data_blocks;

  for (const MetadataRecord& record : parsed.metadata) {
    std::string key = resolveString(parsed, record.key_index, "metadata");
    if (document.metadata.find(key) != document.metadata.end()) {
      document.diagnostics.push_back("duplicate metadata key: " + key);
    }
    document.metadata[std::move(key)] = convertValue(parsed, record.value);
  }

  document.commands.reserve(parsed.commands.size());
  for (const CommandRecord& command : parsed.commands) {
    document.commands.push_back(convertCommand(parsed, command));
  }

  document.events.reserve(parsed.events.size());
  for (const EventRecord& event : parsed.events) {
    document.events.push_back(convertEvent(parsed, event));
  }

  for (const ParseWarning& warning : parsed.warnings) {
    document.diagnostics.push_back(warning.code + ": " + warning.message);
  }

  return document;
}

DocumentValue Deserializer::convertValue(const ParsedFile& parsed,
                                         const MetadataValue& value) const {
  DocumentValue out;
  out.kind = value.kind;
  switch (value.kind) {
    case ValueKind::StringRef:
      out.text = resolveString(parsed, value.string_index, "string");
      break;
    case ValueKind::Integer:
      out.integer = value.integer;
      break;
    case ValueKind::Boolean:
      out.boolean = value.boolean;
      break;
    case ValueKind::Bytes:
      out.bytes = value.bytes;
      break;
    case ValueKind::Map:
      out.map_entries.reserve(value.map_entries.size());
      for (const MapEntry& entry : value.map_entries) {
        out.map_entries.push_back(convertMapEntry(parsed, entry));
      }
      break;
  }
  return out;
}

DocumentMapEntry Deserializer::convertMapEntry(const ParsedFile& parsed,
                                               const MapEntry& entry) const {
  DocumentMapEntry out;
  out.key = resolveString(parsed, entry.key_index, "map-key");
  out.kind = entry.kind;
  switch (entry.kind) {
    case ValueKind::StringRef:
      out.text = resolveString(parsed, entry.string_index, "string");
      break;
    case ValueKind::Integer:
      out.integer = entry.integer;
      break;
    case ValueKind::Boolean:
      out.boolean = entry.boolean;
      break;
    case ValueKind::Bytes:
      out.bytes = entry.bytes;
      break;
    case ValueKind::Map:
      break;
  }
  return out;
}

DocumentCommand Deserializer::convertCommand(
    const ParsedFile& parsed, const CommandRecord& command) const {
  DocumentCommand out;
  out.command_name = commandName(command.command_id);
  out.target = resolveString(parsed, command.target_index, "target");
  out.flags = command.flags;
  out.arguments.reserve(command.arguments.size());
  for (const MetadataValue& argument : command.arguments) {
    out.arguments.push_back(convertValue(parsed, argument));
  }
  out.children.reserve(command.children.size());
  for (const CommandRecord& child : command.children) {
    out.children.push_back(convertCommand(parsed, child));
  }
  return out;
}

DocumentEvent Deserializer::convertEvent(const ParsedFile& parsed,
                                         const EventRecord& event) const {
  DocumentEvent out;
  out.delta_time = event.delta_time;
  out.event_name = eventName(event.event_kind);
  out.label = resolveString(parsed, event.label_index, "event-label");
  out.payload = event.payload;
  out.attributes.reserve(event.attributes.size());
  for (const MetadataRecord& attribute : event.attributes) {
    out.attributes.emplace_back(resolveString(parsed, attribute.key_index,
                                              "event-attribute"),
                                convertValue(parsed, attribute.value));
  }
  return out;
}

std::string Deserializer::resolveString(const ParsedFile& parsed,
                                        uint16_t index,
                                        std::string fallback_prefix) const {
  if (index < parsed.string_table.size()) {
    return parsed.string_table[index];
  }
  return fallback_prefix + "#" + std::to_string(index);
}

std::string Deserializer::commandName(uint16_t command_id) const {
  switch (command_id) {
    case 1:
      return "open";
    case 2:
      return "set";
    case 3:
      return "append";
    case 4:
      return "emit";
    case 5:
      return "link";
    case 6:
      return "checkpoint";
    default:
      return "command#" + std::to_string(command_id);
  }
}

std::string Deserializer::eventName(uint8_t event_kind) const {
  switch (event_kind) {
    case 1:
      return "started";
    case 2:
      return "metadata";
    case 3:
      return "data";
    case 4:
      return "completed";
    case 5:
      return "warning";
    default:
      return "event#" + std::to_string(event_kind);
  }
}

}  // namespace fdf
