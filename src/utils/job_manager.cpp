#include <memory>
#include <thread>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <utils/job_manager.hpp>

JobManager::JobManager() {
  // Initialize the job manager
}
JobManager::~JobManager() {
  // for (auto [id, thread] : threads) {
  //   if (thread->joinable()) {
  //     try {
  //       thread->join();
  //     } catch (const std::exception &e) {
  //       log.error("Error joining thread: {}", e.what());
  //     }
  //   }
  // }
}

/*
template <typename T>
void bindJobEventToLua(std::string event_name) {
  auto &emitter = entt::locator<event_emitter>::value();

  emitter.connect<T>([&](const auto &e, const auto &em) {
    auto &lua = entt::locator<sol::state>::value();
    auto args = lua.create_table();
    args["job"] = e.job;
    emitter.publish(lua_event{event_name, args});
  });
}*/

void JobManager::init(LibLog::Logger parentLog) {
  // log.setParent(&parentLog);
  log.setAsync(true);
  auto label = "Initializing JobManager";
  log.start(label);
  auto &emitter = entt::locator<event_emitter>::value();

  emitter.connect<add_job_event>(
      [&](auto &e, auto &em) { add(e.job, e.start); });

  auto &lua = entt::locator<sol::state>::value();

  lua.new_usertype<Job>("Job", "id", &Job::id, "title", &Job::title, "status",
                        &Job::status, "progress", &Job::progress, "error",
                        &Job::error, "func", &Job::func);
  lua.new_enum("JobStatus", "PROGRESS", JobStatus::PROGRESS, "COMPLETE",
               JobStatus::COMPLETE, "ERROR", JobStatus::ERROR);

  lua.new_usertype<JobManager>("JobManager", "new", sol::no_constructor, "add",
                               &JobManager::add);

  lua.set("jobs", this);

  emitter.connect<job_start_event>([&](const auto &e, const auto &em) {
    auto &lua = entt::locator<sol::state>::value();
    auto args = lua.create_table();
    args["job"] = e.job;
    emitter.publish(lua_event{"job_start", args});
  });

  emitter.connect<job_complete_event>([&](const auto &e, const auto &em) {
    auto &lua = entt::locator<sol::state>::value();
    auto args = lua.create_table();
    args["job"] = e.job;
    emitter.publish(lua_event{"job_complete", args});
  });

  emitter.connect<job_error_event>([&](const auto &e, const auto &em) {
    auto &lua = entt::locator<sol::state>::value();
    auto args = lua.create_table();
    args["job"] = e.job;
    emitter.publish(lua_event{"job_error", args});
  });

  emitter.connect<job_update_event>([&](const auto &e, const auto &em) {
    auto &lua = entt::locator<sol::state>::value();
    auto args = lua.create_table();
    args["job"] = e.job;
    emitter.publish(lua_event{"job_update", args});
  });

  emitter.publish(init_event{"job_manager"});
  log.stop(label);
}

int JobManager::add(std::shared_ptr<Job> job, bool start) {
  if (job->id == -1) {
    job->id = jobs.size();
  }
  log.info("Adding a job: {} [{}]", job->title, job->id);
  jobs[job->id] = job;
  if (start) {
    auto t = std::thread(
        [&](std::shared_ptr<Job> job) {
          auto &emitter = entt::locator<event_emitter>::value();
          emitter.publish(job_start_event{job});
          job->status = JobStatus::PROGRESS;
          try {
            job->func();
          } catch (const std::exception &e) {
            job->status = JobStatus::ERROR;
            job->error = e.what();
            emitter.publish(job_error_event{job});
            return;
          }
          job->status = JobStatus::COMPLETE;
          job->progress = 100;
          emitter.publish(job_update_event{job});
          emitter.publish(job_complete_event{job});
        },
        job);
    threads[job->id] = &t;
    t.detach();
  }
  return job->id;
}
