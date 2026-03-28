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

inline fs::path resolve_content_root(fs::path exe_dir) {
  std::error_code ec;
  fs::path cur = fs::weakly_canonical(fs::absolute(exe_dir), ec);
  if (ec)
    cur = fs::absolute(exe_dir);
  for (int i = 0; i < 12; ++i) {
    const auto proto = cur / "data" / "frame.proto";
    if (fs::is_regular_file(proto, ec)) {
      const auto sz = fs::file_size(proto, ec);
      if (!ec && sz >= 8)
        return cur;
    }
    if (!cur.has_parent_path() || cur == cur.parent_path())
      break;
    cur = cur.parent_path();
  }
  return fs::absolute(exe_dir);
}
