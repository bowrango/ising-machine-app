#include "tests.h"

namespace tests {

const char *toString(Result r) {
  switch (r) {
    case Result::Pass: return "PASS";
    case Result::Fail: return "FAIL";
    case Result::Skip: return "SKIP";
  }
  return "?";
}

}  // namespace tests
