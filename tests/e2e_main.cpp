#include <ixwebsocket/IXNetSystem.h>

#ifdef _WIN32
#include <crtdbg.h>
#include <cstdlib>
#include <signal.h>
#include <eh.h>
#endif

#include <cstdio>
#include <exception>
#include <typeinfo>
#include <catch2/catch_session.hpp>

#ifdef _WIN32
static void my_terminate() {
  fprintf(stderr, "[DIAG] std::terminate() called");
  auto eptr = std::current_exception();
  if (eptr) {
    try { std::rethrow_exception(eptr); }
    catch (const std::exception& e) {
      fprintf(stderr, ": %s\n", e.what());
    }
    catch (...) {
      fprintf(stderr, ": unknown exception\n");
    }
  } else {
    fprintf(stderr, " (no active exception)\n");
  }
  fflush(stderr);
  _exit(3);
}

static void my_purecall() {
  fprintf(stderr, "[DIAG] pure virtual function call\n");
  fflush(stderr);
  _exit(3);
}

static void my_invalid_param(const wchar_t* expr, const wchar_t* func,
                              const wchar_t* file, unsigned int line,
                              uintptr_t) {
  fprintf(stderr, "[DIAG] invalid parameter: %ls in %ls (%ls:%u)\n",
          expr ? expr : L"(null)", func ? func : L"(null)",
          file ? file : L"(null)", line);
  fflush(stderr);
  _exit(3);
}

static LONG WINAPI my_seh(EXCEPTION_POINTERS* ep) {
  fprintf(stderr, "[DIAG] SEH exception 0x%08lX at %p\n",
          ep->ExceptionRecord->ExceptionCode,
          ep->ExceptionRecord->ExceptionAddress);
  fflush(stderr);
  _exit(3);
}
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  std::set_terminate(my_terminate);
  _set_purecall_handler(my_purecall);
  _set_invalid_parameter_handler(my_invalid_param);
  SetUnhandledExceptionFilter(my_seh);
  signal(SIGABRT, [](int) {
    fprintf(stderr, "[DIAG] SIGABRT caught\n");
    fflush(stderr);
    _exit(3);
  });
#endif
  ix::initNetSystem();
  int result = Catch::Session().run(argc, argv);
  ix::uninitNetSystem();
  return result;
}
