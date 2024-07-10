#pragma once
#include <string>
#include <functional>
#include <memory>

enum JobStatus {
  INIT, PROGRESS, COMPLETE, ERROR
};

struct Job {
  std::string title;
  std::function<void()> func;


  JobStatus status = JobStatus::INIT;
  int id = -1;
  int progress = 0;
  std::string error;

  Job(){}
  Job(std::string title, std::function<void()> func) : title(title), func(func){}
  Job(std::string title, std::function<void()> func, int id) : title(title), func(func), id(id){}
};

struct add_job_event {
  std::shared_ptr<Job> job;
  bool start = true;
};

struct job_start_event {
  std::shared_ptr<Job> job;
};

struct job_complete_event {
  std::shared_ptr<Job> job;
};

struct job_error_event {
  std::shared_ptr<Job> job;
};

struct job_update_event {
  std::shared_ptr<Job> job;
};
