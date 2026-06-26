#include "validator.h"

#include <set>
#include <utility>

namespace fdf {

namespace {

bool isStringRefValid(const ParsedFile& parsed, uint16_t index) {
  return index < parsed.string_table.size();
}

}  // namespace

ValidationReport Validator::validate(const ParsedFile& parsed) const {
  ValidationReport report;

  if (parsed.header.major_version != kSupportedMajorVersion) {
    addIssue(report, Severity::Error, "unsupported-version",
             "unsupported FDF major version", 4);
  }

  for (const ParseWarning& warning : parsed.warnings) {
    addIssue(report, Severity::Warning, warning.code, warning.message,
             warning.offset);
  }

  std::set<std::string> seen_strings;
  for (size_t i = 0; i < parsed.string_table.size(); ++i) {
    const std::string& value = parsed.string_table[i];
    if (value.empty()) {
      addIssue(report, Severity::Warning, "empty-string-table-entry",
               "string table entry is empty", i);
    }
    if (!seen_strings.insert(value).second) {
      addIssue(report, Severity::Info, "duplicate-string",
               "string table contains a duplicate value", i);
    }
  }

  for (const MetadataRecord& record : parsed.metadata) {
    if (!isStringRefValid(parsed, record.key_index)) {
      addIssue(report, Severity::Error, "metadata-key-out-of-range",
               "metadata key index does not exist in the string table",
               record.offset);
    }
    validateValue(parsed, record.value, record.offset, report);
  }

  for (const CommandRecord& command : parsed.commands) {
    validateCommand(parsed, command, 0, report);
  }

  for (const EventRecord& event : parsed.events) {
    if (!isStringRefValid(parsed, event.label_index)) {
      addIssue(report, Severity::Error, "event-label-out-of-range",
               "event label index does not exist in the string table",
               event.offset);
    }
    for (const MetadataRecord& attribute : event.attributes) {
      if (!isStringRefValid(parsed, attribute.key_index)) {
        addIssue(report, Severity::Error, "event-attribute-key-out-of-range",
                 "event attribute key index does not exist in the string table",
                 attribute.offset);
      }
      validateValue(parsed, attribute.value, attribute.offset, report);
    }
  }

  for (const DataBlock& block : parsed.data_blocks) {
    if (!isStringRefValid(parsed, block.name_index)) {
      addIssue(report, Severity::Error, "data-name-out-of-range",
               "data block name index does not exist in the string table",
               block.offset);
    }
    if (block.advertised_checksum != 0 &&
        rollingChecksum(block.bytes) != block.advertised_checksum) {
      addIssue(report, Severity::Warning, "data-checksum-mismatch",
               "data block checksum does not match decoded bytes",
               block.offset);
    }
  }

  if (parsed.sections.empty()) {
    addIssue(report, Severity::Warning, "empty-document",
             "document contains no sections");
  }
  if (parsed.string_table.empty() &&
      (!parsed.metadata.empty() || !parsed.commands.empty() ||
       !parsed.events.empty() || !parsed.data_blocks.empty())) {
    addIssue(report, Severity::Error, "missing-string-table",
             "records are present without a string table");
  }

  return report;
}

ValidationReport Validator::validate(const Document& document) const {
  ValidationReport report;

  if (document.major_version != kSupportedMajorVersion) {
    addIssue(report, Severity::Error, "unsupported-version",
             "unsupported document major version");
  }
  for (const auto& entry : document.metadata) {
    if (entry.first.empty()) {
      addIssue(report, Severity::Warning, "empty-metadata-key",
               "document metadata contains an empty key");
    }
  }
  for (const DocumentCommand& command : document.commands) {
    if (command.command_name.empty() || command.target.empty()) {
      addIssue(report, Severity::Warning, "incomplete-command",
               "document command is missing a name or target");
    }
  }
  for (const std::string& diagnostic : document.diagnostics) {
    addIssue(report, Severity::Info, "document-diagnostic", diagnostic);
  }

  return report;
}

void Validator::addIssue(ValidationReport& report, Severity severity,
                         std::string code, std::string message,
                         size_t offset) const {
  if (severity == Severity::Error) {
    report.ok = false;
  }
  report.issues.push_back({severity, std::move(code), std::move(message),
                           offset});
}

void Validator::validateValue(const ParsedFile& parsed,
                              const MetadataValue& value, size_t offset,
                              ValidationReport& report) const {
  switch (value.kind) {
    case ValueKind::StringRef:
      if (!isStringRefValid(parsed, value.string_index)) {
        addIssue(report, Severity::Error, "string-ref-out-of-range",
                 "string reference does not exist in the string table",
                 offset);
      }
      break;
    case ValueKind::Bytes:
      if (value.bytes.size() > 64 * 1024) {
        addIssue(report, Severity::Warning, "large-byte-value",
                 "metadata byte value is unusually large", offset);
      }
      break;
    case ValueKind::Map:
      for (const MapEntry& entry : value.map_entries) {
        if (!isStringRefValid(parsed, entry.key_index)) {
          addIssue(report, Severity::Error, "map-key-out-of-range",
                   "map key index does not exist in the string table", offset);
        }
        MetadataValue nested;
        nested.kind = entry.kind;
        nested.string_index = entry.string_index;
        nested.integer = entry.integer;
        nested.boolean = entry.boolean;
        nested.bytes = entry.bytes;
        validateValue(parsed, nested, offset, report);
      }
      break;
    case ValueKind::Integer:
    case ValueKind::Boolean:
      break;
  }
}

void Validator::validateCommand(const ParsedFile& parsed,
                                const CommandRecord& command, size_t depth,
                                ValidationReport& report) const {
  if (depth > kMaxRecordDepth) {
    addIssue(report, Severity::Error, "command-depth-exceeded",
             "command nesting exceeds supported depth", command.offset);
    return;
  }
  if (command.command_id == 0) {
    addIssue(report, Severity::Warning, "command-id-zero",
             "command id zero is reserved", command.offset);
  }
  if (!isStringRefValid(parsed, command.target_index)) {
    addIssue(report, Severity::Error, "command-target-out-of-range",
             "command target index does not exist in the string table",
             command.offset);
  }
  for (const MetadataValue& argument : command.arguments) {
    validateValue(parsed, argument, command.offset, report);
  }
  for (const CommandRecord& child : command.children) {
    validateCommand(parsed, child, depth + 1, report);
  }
}

}  // namespace fdf
