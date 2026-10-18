// The server's tests run with the MMO's Citadel (C1, docs/citadel-interior.md):
// 12x12 with a hall inside, as the server program sets it at its start
// (program/main.cpp). One switch for the process, on before any test runs.

#include "Rules.h"

namespace {

[[maybe_unused]] const bool hallOn = [] {
    ac::Rules::setCitadelHall(true);
    return true;
}();

} // namespace
