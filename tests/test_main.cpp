#include "check.h"

std::vector<fwtest::Case>& fwtest::registry() {
  static std::vector<Case> cases;
  return cases;
}

int main() {
  for (const auto& c : fwtest::registry()) {
    const int before = fwtest::failures;
    c.fn();
    std::printf("%s %s\n", fwtest::failures == before ? "ok  " : "FAIL", c.name);
  }
  std::printf("%d failure(s)\n", fwtest::failures);
  return fwtest::failures == 0 ? 0 : 1;
}
