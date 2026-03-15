#pragma once
#include <game/registry_container.hpp>
#include <game/registry_store.hpp>
#include <iostream>
#include <liblog/liblog.hpp>
#include <utils/data/container.hpp>
#include <utils/entt.hpp>

class Prototypes : public RegistryContainer {
  LibLog::Logger log = LibLog::Logger(fmt::color::orange, "Proto");

  LibLog::Logger &getLog() override { return log; }
  int8_t getVersion() override { return version; }
  int8_t getType() override { return type; }

public:
  int8_t version = 2;
  int8_t type = 2;

  using RegistryContainer::RegistryContainer;
};
