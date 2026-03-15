#pragma once
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <fmt/format.h>
#include <liblog/liblog.hpp>
#include <map>
#include <sstream>
#include <string>

// Convenience macro: ar.field("x", x) -> FIELD(ar, x)
#define FIELD(ar, x) (ar).field(#x, (x))

// ---------------------------------------------------------------------------
// FieldOutputArchive
// Collects named fields into a self-describing binary format.
// Usage:
//   FieldOutputArchive foa;
//   foa.field("x", someValue);
//   foa.writeTo(binaryOutputArchive);
// ---------------------------------------------------------------------------
class FieldOutputArchive {
public:
  struct FieldEntry {
    std::string name;
    std::string data; // raw cereal-binary bytes
  };

  template <typename T> void field(const char *name, const T &value) {
    std::ostringstream buf;
    {
      cereal::BinaryOutputArchive ar{buf};
      ar(value);
    }
    entries_.push_back({name, buf.str()});
  }

  // Write all collected fields into an outer cereal binary archive.
  // Format per component:
  //   uint16_t field_count
  //   for each field:
  //     string   name
  //     uint32_t data_size
  //     byte[]   data
  void writeTo(cereal::BinaryOutputArchive &ar) const {
    uint16_t count = static_cast<uint16_t>(entries_.size());
    ar(count);
    for (const auto &e : entries_) {
      uint32_t sz = static_cast<uint32_t>(e.data.size());
      ar(e.name);
      ar(sz);
      ar(cereal::binary_data(e.data.data(), sz));
    }
  }

private:
  std::vector<FieldEntry> entries_;
};

// ---------------------------------------------------------------------------
// FieldInputArchive
// Reads the named-field format written by FieldOutputArchive.
// Known fields are deserialized; unknown fields are skipped (forward compat);
// fields in code but not in the stream keep their C++ default values.
// ---------------------------------------------------------------------------
class FieldInputArchive {
public:
  // Read all fields from the stream into an in-memory map.
  void readFrom(cereal::BinaryInputArchive &ar) {
    uint16_t count = 0;
    ar(count);
    for (uint16_t i = 0; i < count; i++) {
      std::string name;
      uint32_t sz = 0;
      ar(name);
      ar(sz);
      std::string data(sz, '\0');
      if (sz > 0) {
        ar(cereal::binary_data(data.data(), sz));
      }
      fields_[name] = std::move(data);
    }
  }

  // Try to deserialize field `name` into `value`.
  // Returns true if the field was present in the stream.
  // Returns false (and leaves value unchanged) if missing.
  template <typename T> bool field(const char *name, T &value) {
    auto it = fields_.find(name);
    if (it == fields_.end()) {
      return false; // new field added in code — keep default
    }
    try {
      std::istringstream buf(it->second);
      cereal::BinaryInputArchive ar{buf};
      ar(value);
      return true;
    } catch (cereal::Exception &e) {
      static LibLog::Logger log =
          LibLog::Logger(fmt::color::yellow, "FieldAr");
      log.warn("Failed to deserialize field '{}': {}", name, e.what());
      return false;
    }
  }

  bool has(const char *name) const {
    return fields_.find(name) != fields_.end();
  }

private:
  std::map<std::string, std::string> fields_;
};
