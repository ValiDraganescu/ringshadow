#include "NetLagSimulator.h"

#include <cstring>

namespace ac::net {

const LagSettings& LagSimulator::settingsFor(const Address& remote) const {
    const auto it = perRemote_.find(remote.key());
    return it == perRemote_.end() ? default_ : it->second;
}

void LagSimulator::hold(Queue& queue, double now, const Address& remote, const uint8_t* data, size_t size) {
    const LagSettings& s = settingsFor(remote);
    if (random_.uniform() < s.loss) {
        lost_ += 1;
        return;
    }
    const int copies = random_.uniform() < s.duplicate ? 2 : 1;
    for (int i = 0; i < copies; i++) {
        Held h;
        h.due = now + s.delay + s.jitter * random_.uniform();
        if (random_.uniform() < s.reorder) h.due += s.reorderHold;
        h.order = order_++;
        h.remote = remote;
        h.data.assign(data, data + size);
        queue.push(std::move(h));
    }
}

void LagSimulator::pump(double now) {
    while (!outgoing_.empty() && outgoing_.top().due <= now) {
        const Held& h = outgoing_.top();
        inner_.send(now, h.remote, h.data.data(), h.data.size());
        outgoing_.pop();
    }
}

bool LagSimulator::send(double now, const Address& to, const uint8_t* data, size_t size) {
    hold(outgoing_, now, to, data, size);
    pump(now);
    return true;
}

bool LagSimulator::receive(double now, Address& from, uint8_t* buffer, size_t capacity, size_t& size) {
    pump(now);
    uint8_t scratch[2048];
    Address remote;
    size_t got = 0;
    while (inner_.receive(now, remote, scratch, sizeof scratch, got)) hold(incoming_, now, remote, scratch, got);
    while (!incoming_.empty() && incoming_.top().due <= now) {
        const Held& h = incoming_.top();
        const bool fits = h.data.size() <= capacity;
        if (fits) {
            from = h.remote;
            size = h.data.size();
            if (size > 0) std::memcpy(buffer, h.data.data(), size);
        }
        incoming_.pop();
        if (fits) return true;
    }
    return false;
}

} // namespace ac::net
