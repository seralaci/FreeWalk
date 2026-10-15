#include "win/guarded_call.h"
#include <windows.h>

namespace fw {

#if defined(_MSC_VER)
namespace {

int capture_fault(EXCEPTION_POINTERS* e, Fault* f) {
  f->code = e->ExceptionRecord->ExceptionCode;
  f->address = reinterpret_cast<std::uint64_t>(e->ExceptionRecord->ExceptionAddress);
  return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

bool guarded_call(Thunk thunk, void* context, Fault* fault) {
  __try {
    thunk(context);
    return true;
  } __except (capture_fault(GetExceptionInformation(), fault)) {
    return false;
  }
}
#else
bool guarded_call(Thunk, void*, Fault* fault) {
  fault->code = 0;
  return false;
}
#endif

}  // namespace fw
