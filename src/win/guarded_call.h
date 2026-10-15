#pragma once
#include <cstdint>

namespace fw {

// Details of an access violation in the game's code.
struct Fault {
  unsigned long code = 0;
  std::uint64_t address = 0;
};

using Thunk = void (*)(void* context);

// A call into the game's code with exception protection (SEH): returns false on a fault, the code and address go into fault.
// The thunk is a captureless lambda (function pointer), context is the struct holding the call's arguments.
// Without SEH (MinGW) it does not call the game's code and returns false.
bool guarded_call(Thunk thunk, void* context, Fault* fault);

}  // namespace fw
