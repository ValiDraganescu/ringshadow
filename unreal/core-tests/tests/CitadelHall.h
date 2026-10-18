// The Citadel's hall switch for one test's scope (`Rules::setCitadelHall`,
// docs/citadel-interior.md section 1). The scenes on the strategy game's maps
// (window maps, the home map, the Swift goldens) run with it off: their base
// layouts and ore arcs are built round the solid 5x5 Citadel.
#pragma once

#include "Rules.h"

namespace simtest {

/// Sets the hall for a scope and puts back what it was.
struct CitadelHall {
    bool was;
    explicit CitadelHall(bool on) : was(ac::Rules::citadelHall()) { ac::Rules::setCitadelHall(on); }
    ~CitadelHall() { ac::Rules::setCitadelHall(was); }
    CitadelHall(const CitadelHall&) = delete;
    CitadelHall& operator=(const CitadelHall&) = delete;
};

} // namespace simtest
