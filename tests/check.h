#pragma once
#include <cmath>
#include <cstdio>
#include <vector>

// Minimal test framework: the TEST macro registers the function, the CHECK macros count the failures.
namespace fwtest {
struct Case { const char* name; void (*fn)(); };
std::vector<Case>& registry();
inline int failures = 0;
struct Register { Register(const char* name, void (*fn)()) { registry().push_back({name, fn}); } };
}

#define TEST(name) \
  static void name(); \
  static fwtest::Register register_##name(#name, name); \
  static void name()

#define CHECK(cond) \
  do { if (!(cond)) { std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++fwtest::failures; } } while (0)

#define CHECK_NEAR(a, b, eps) \
  do { const double a_ = (a), b_ = (b); \
       if (std::fabs(a_ - b_) > (eps)) { std::printf("  FAIL %s:%d: %s = %.9g, expected %.9g\n", __FILE__, __LINE__, #a, a_, b_); ++fwtest::failures; } } while (0)
