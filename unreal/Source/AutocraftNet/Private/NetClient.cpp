#include "NetClient.h"

namespace ac::net {

Client::Client(PacketIo& io, const ClientConfig& config) : endpoint_(io, config) {}

void Client::connect(double now) {
    disconnectReason_.clear();
    endpoint_.connect(now);
}

uint32_t Client::take(int64_t unit) {
    const RequestMessage::Take t{unit};
    return request(&t);
}

uint32_t Client::handBack() { return request(nullptr); }

uint32_t Client::request(const RequestMessage::Take* take) {
    RequestMessage m;
    m.id = nextRequest_++;
    if (take) m.what = *take;
    else m.what = RequestMessage::HandBack{};
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
        reader_.reset();
        return;
    }
    case MessageType::snapshot: {
        const std::optional<SnapshotPart> part = reader_.read(data);
        if (!part) {
            partsFailed += 1;
            return;
        }
        partsRead += 1;
        if (mirror_) {
            mirror_->apply(*part);
            if (onSnapshot) onSnapshot(*part);
        }
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
