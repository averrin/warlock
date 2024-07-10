#pragma once
#include <functional>
#include <liblog/liblog.hpp>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utils/jobs.hpp>

class JobManager {
  LibLog::Logger log = LibLog::Logger(fmt::color::yellow, "JM");
  std::map<int, std::shared_ptr<Job>> jobs;
  std::map<int, std::thread *> threads;

public:
  JobManager();
  ~JobManager();

  void init(LibLog::Logger parentLog);

  int add(std::shared_ptr<Job> job, bool start = true);
  bool restart(int id);

  // template <typename T> void bindJobEventToLua(std::string event_name);
};
