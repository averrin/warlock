#pragma once
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <unistd.h>
#include <limits.h>
#include <libproc.h>
#else
#include <unistd.h>
#include <limits.h>
#include <linux/limits.h>
#endif

inline fs::path get_selfpath() {
#ifdef _WIN32
  char buff[MAX_PATH];
  GetModuleFileName(NULL, buff, sizeof(buff));
#elif __APPLE__
  char buff[PROC_PIDPATHINFO_MAXSIZE];
  proc_pidpath(getpid(), buff, sizeof(buff));
#else
  char buff[PATH_MAX];
  ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff) - 1);
  if (len != -1) {
    buff[len] = '\0';
  }
#endif
  auto path = fs::path(buff);
  return path.parent_path();
}
