// The headless client of the tests and the bench is the game's own client
// end of the protocol (net::Client, AutocraftNet/Public/NetClient.h): it
// connects, keeps a mirror of the region, and each tick sends its input and
// its ack, as the game does at 60 Hz.
#pragma once

#include "NetClient.h"

namespace ac::server {

using HeadlessClient = net::Client;

} // namespace ac::server
