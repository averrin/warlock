#pragma once

#include <cereal/types/string.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <map>
#include <string>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

class Store {
  int8_t magic = 0x00;
  int8_t expected_magic = 0xAF;
  friend class cereal::access;
  template <class Archive> void load(Archive &ar) {
    ar(magic, type, version, name, attributes);
    if (magic != expected_magic) {
      throw std::runtime_error("Invalid magic number");
    }
    if (type != expected_type) {
      throw std::runtime_error("Invalid type");
    }
    if (version != expected_version) {
      throw std::runtime_error("Invalid version");
    }
  };
  template <class Archive> void save(Archive &ar) const {
    ar(magic, type, version, name, attributes);
    // ar(expected_magic, expected_type, expected_version, name, attributes);
  };

public:
  int8_t type = 0;
  int8_t version = 0;
  int8_t expected_type = 0;
  int8_t expected_version = 0;
  std::string name;
  fs::path path;
  std::map<std::string, std::string> attributes = {};

  Store(int8_t type, std::string name, fs::path path, int version)
      : expected_type(type), path(path), expected_version(version), name(name) {}
};
