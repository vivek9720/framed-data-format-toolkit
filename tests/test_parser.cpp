#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "deserializer.h"
#include "format.h"
#include "parser.h"
#include "validator.h"

namespace {

std::vector<uint8_t> xorRleEncode(const std::vector<uint8_t>& input,
                                  uint8_t key) {
  std::vector<uint8_t> out;
  out.push_back(key);
  out.push_back(static_cast<uint8_t>(input.size() - 1));
  for (uint8_t byte : input) {
    out.push_back(static_cast<uint8_t>(byte ^ key));
  }
  return out;
}

std::vector<uint8_t> section(std::string tag, fdf::Encoding encoding,
                             uint16_t records,
                             const std::vector<uint8_t>& decoded) {
  std::vector<uint8_t> payload = decoded;
  if (encoding == fdf::Encoding::XorRle) {
    payload = xorRleEncode(decoded, 0x5a);
  }

  std::vector<uint8_t> out;
  fdf::appendString(out, tag);
  out.push_back(static_cast<uint8_t>(encoding));
  out.push_back(0);
  fdf::appendU16(out, records);
  fdf::appendU32(out, static_cast<uint32_t>(decoded.size()));
  fdf::appendU32(out, static_cast<uint32_t>(payload.size()));
  fdf::appendU32(out, fdf::rollingChecksum(payload));
  fdf::appendBytes(out, payload);
  return out;
}

void appendValueStringRef(std::vector<uint8_t>& out, uint16_t index) {
  out.push_back(static_cast<uint8_t>(fdf::ValueKind::StringRef));
  fdf::appendU16(out, index);
}

void appendValueBool(std::vector<uint8_t>& out, bool value) {
  out.push_back(static_cast<uint8_t>(fdf::ValueKind::Boolean));
  out.push_back(value ? 1 : 0);
}

void appendValueInteger(std::vector<uint8_t>& out, int64_t value) {
  out.push_back(static_cast<uint8_t>(fdf::ValueKind::Integer));
  fdf::appendI64(out, value);
}

void appendValueBytes(std::vector<uint8_t>& out, std::string text) {
  out.push_back(static_cast<uint8_t>(fdf::ValueKind::Bytes));
  fdf::appendU16(out, static_cast<uint16_t>(text.size()));
  fdf::appendString(out, text);
}

std::vector<uint8_t> makeStringTable() {
  const std::vector<std::string> strings = {
      "title", "author", "root", "enabled",
      "payload", "phase", "ready", "sample"};
  std::vector<uint8_t> out;
  fdf::appendU16(out, static_cast<uint16_t>(strings.size()));
  for (const std::string& value : strings) {
    fdf::appendU16(out, static_cast<uint16_t>(value.size()));
    fdf::appendString(out, value);
  }
  return out;
}

std::vector<uint8_t> makeMetadata() {
  std::vector<uint8_t> out;
  fdf::appendU16(out, 0);
  appendValueStringRef(out, 7);
  fdf::appendU16(out, 3);
  appendValueBool(out, true);
  return out;
}

std::vector<uint8_t> makeCommands() {
  std::vector<uint8_t> out;
  fdf::appendU16(out, 1);
  fdf::appendU16(out, 2);
  out.push_back(0);
  out.push_back(2);
  appendValueStringRef(out, 6);
  appendValueBytes(out, "payload");
  out.push_back(1);

  fdf::appendU16(out, 2);
  fdf::appendU16(out, 5);
  out.push_back(0);
  out.push_back(1);
  appendValueInteger(out, 42);
  out.push_back(0);
  return out;
}

std::vector<uint8_t> makeEvents() {
  std::vector<uint8_t> out;
  fdf::appendU32(out, 10);
  out.push_back(1);
  fdf::appendU16(out, 5);
  fdf::appendU16(out, 3);
  fdf::appendString(out, "evt");
  out.push_back(1);
  fdf::appendU16(out, 6);
  out.push_back(static_cast<uint8_t>(fdf::ValueKind::Boolean));
  out.push_back(1);
  return out;
}

std::vector<uint8_t> makeDataBlocks() {
  const std::vector<uint8_t> body = {'p', 'a', 'y', 'l', 'o', 'a', 'd'};
  const std::vector<uint8_t> encoded = xorRleEncode(body, 0x33);
  std::vector<uint8_t> out;
  fdf::appendU16(out, 4);
  out.push_back(1);
  out.push_back(static_cast<uint8_t>(fdf::Encoding::XorRle));
  fdf::appendU16(out, static_cast<uint16_t>(body.size()));
  fdf::appendU16(out, static_cast<uint16_t>(encoded.size()));
  fdf::appendU32(out, fdf::rollingChecksum(body));
  fdf::appendBytes(out, encoded);
  return out;
}

std::vector<uint8_t> makeDocument() {
  std::vector<uint8_t> body;
  fdf::appendBytes(body,
                   section("STRT", fdf::Encoding::Raw, 8, makeStringTable()));
  fdf::appendBytes(body,
                   section("META", fdf::Encoding::Raw, 2, makeMetadata()));
  fdf::appendBytes(body,
                   section("CMND", fdf::Encoding::Raw, 1, makeCommands()));
  fdf::appendBytes(body,
                   section("EVNT", fdf::Encoding::Raw, 1, makeEvents()));
  fdf::appendBytes(body,
                   section("DATA", fdf::Encoding::XorRle, 1, makeDataBlocks()));

  std::vector<uint8_t> file;
  fdf::appendString(file, "FDF2");
  file.push_back(1);
  file.push_back(0);
  fdf::appendU16(file, 0);
  fdf::appendU16(file, 5);
  fdf::appendU16(file, static_cast<uint16_t>(fdf::kHeaderSize));
  fdf::appendU32(file, fdf::rollingChecksum(body));
  fdf::appendBytes(file, body);
  return file;
}

}  // namespace

int main() {
  const std::vector<uint8_t> document_bytes = makeDocument();

  fdf::ParserOptions options;
  options.strict_checksums = true;
  const fdf::ParsedFile parsed = fdf::Parser().parse(document_bytes, options);

  assert(parsed.header.major_version == 1);
  assert(parsed.sections.size() == 5);
  assert(parsed.string_table.size() == 8);
  assert(parsed.metadata.size() == 2);
  assert(parsed.commands.size() == 1);
  assert(parsed.commands[0].children.size() == 1);
  assert(parsed.events.size() == 1);
  assert(parsed.data_blocks.size() == 1);
  assert(parsed.data_blocks[0].bytes.size() == 7);

  const fdf::ValidationReport report = fdf::Validator().validate(parsed);
  assert(report.ok);

  const fdf::Document document = fdf::Deserializer().build(parsed);
  assert(document.metadata.at("title").text == "sample");
  assert(document.commands[0].command_name == "open");
  assert(document.commands[0].children[0].command_name == "set");
  assert(document.events[0].event_name == "started");
  return 0;
}
