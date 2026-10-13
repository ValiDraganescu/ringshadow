#include "NetClient.h"

namespace ac::net {

Client::Client(PacketIo& io, const ClientConfig& config) : endpoint_(io, config) {}

void Client::connect(double now) {
    disconnectReason_.clear();
    reader_.reset();
    waiting_.clear();
    visit_ = 0;
    endpoint_.connect(now);
}

uint32_t Client::take(int64_t unit) { return request(RequestMessage::Take{unit}); }

uint32_t Client::handBack() { return request(RequestMessage::HandBack{}); }

uint32_t Client::fly(const std::string& region, const std::string& pad) {
    return request(RequestMessage::Fly{region, pad});
}

uint32_t Client::request(const decltype(RequestMessage::what)& what) {
    RequestMessage m;
    m.id = nextRequest_++;
    m.what = what;
    const std::vector<uint8_t> bytes = encode(m);
    endpoint_.send(Channel::reliable, bytes.data(), bytes.size());
    return m.id;
}

void Client::message(const std::vector<uint8_t>& data) {
    const auto type = messageType(data);
    if (!type) return;
    switch (*type) {
    case MessageType::welcome: {
        Welcome w;
        if (!decode(data, w)) return;
        welcome_ = w;
        mirror_ = std::make_unique<Mirror>(w.region);
        // Parts that came before it (the snapshots are unreliable, the
        // Welcome may come in pieces).
        std::deque<SnapshotPart> early;
        early.swap(waiting_);
        for (const SnapshotPart& p : early) applyPart(p);
        return;
    }
    case MessageType::regionChange: {
        RegionChange rc;
        if (!decode(data, rc) || rc.visit <= visit_) return;
        regionChange_ = rc;
        visit_ = rc.visit;
        mirror_ = std::make_unique<Mirror>(rc.region);
        if (onRegionChange) onRegionChange(rc);
        std::deque<SnapshotPart> early;
        early.swap(waiting_);
        for (const SnapshotPart& p : early) applyPart(p);
        return;
    }
    case MessageType::world: {
        WorldMessage m;
        if (decode(data, m)) world_ = m;
        return;
    }
    case MessageType::snapshot: {
        const std::optional<SnapshotPart> part = reader_.read(data);
        if (!part) {
            partsFailed += 1;
            return;
        }
        partsRead += 1;
        applyPart(*part);
        return;
    }
    case MessageType::event: {
        EventMessage m;
        if (decode(data, m)) events.insert(events.end(), m.events.begin(), m.events.end());
        return;
    }
    case MessageType::reply: {
        ReplyMessage m;
        if (decode(data, m)) replies[m.request] = m;
        return;
    }
    default:
        return;  // the client's own kinds
    }
}

void Client::applyPart(const SnapshotPart& part) {
    const int64_t visit = visitOf(part.snapshot.game);
    // An old region's: gone. A region not reached yet (its RegionChange is on
    // its way), or no mirror yet: it waits.
    if (visit < visit_) return;
    if (!mirror_ || visit > visit_) {
        waiting_.push_back(part);
        while (waiting_.size() > kMaxWaiting) waiting_.pop_front();
        return;
    }
    mirror_->apply(part);
    if (onSnapshot) onSnapshot(part);
}

void Client::receive(double now) {
    endpoint_.update(now);
    for (Event e; endpoint_.poll(e);) {
        if (e.type == Event::Type::disconnected) disconnectReason_ = e.reason;
        else if (e.type == Event::Type::message) message(e.data);
    }
}

std::optional<PilotInput> Client::sendInput() {
    if (!connected() || !welcome_) return std::nullopt;
    tick_ += 1;
    PilotInput in = steer;
    in.tick = tick_;
    sent_.push_front(in);
    while (sent_.size() > InputMessage::kMaxInputs) sent_.pop_back();
    InputMessage m;
    m.inputs.assign(sent_.begin(), sent_.end());
    const std::vector<uint8_t> input = encode(m);
    endpoint_.send(Channel::unreliable, input.data(), input.size());
    // Presses go out once; the repeats carry them for loss.
    steer.act = false;
    steer.ability = false;
    const AckMessage ack = reader_.ack();
    if (!ack.received.empty()) {
        const std::vector<uint8_t> bytes = encode(ack);
        endpoint_.send(Channel::unreliable, bytes.data(), bytes.size());
    }
    return in;
}

void Client::flush(double now) { endpoint_.flush(now); }

void Client::tick(double now) {
    receive(now);
    sendInput();
    flush(now);
}

} // namespace ac::net
