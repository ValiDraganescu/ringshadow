#include "HeadlessClient.h"

namespace ac::server {

HeadlessClient::HeadlessClient(net::PacketIo& io, const net::ClientConfig& config) : endpoint_(io, config) {}

void HeadlessClient::connect(double now) {
    disconnectReason_.clear();
    endpoint_.connect(now);
}

uint32_t HeadlessClient::take(int64_t unit) {
    const net::RequestMessage::Take t{unit};
    return request(&t);
}

uint32_t HeadlessClient::handBack() { return request(nullptr); }

uint32_t HeadlessClient::request(const net::RequestMessage::Take* take) {
    net::RequestMessage m;
    m.id = nextRequest_++;
    if (take) m.what = *take;
    else m.what = net::RequestMessage::HandBack{};
    const std::vector<uint8_t> bytes = net::encode(m);
    endpoint_.send(net::Channel::reliable, bytes.data(), bytes.size());
    return m.id;
}

void HeadlessClient::message(const std::vector<uint8_t>& data) {
    const auto type = net::messageType(data);
    if (!type) return;
    switch (*type) {
    case net::MessageType::welcome: {
        net::Welcome w;
        if (!net::decode(data, w)) return;
        welcome_ = w;
        mirror_ = std::make_unique<net::Mirror>(w.region);
        reader_.reset();
        return;
    }
    case net::MessageType::snapshot: {
        const std::optional<net::SnapshotPart> part = reader_.read(data);
        if (!part) {
            partsFailed += 1;
            return;
        }
        partsRead += 1;
        if (mirror_) mirror_->apply(*part);
        return;
    }
    case net::MessageType::event: {
        net::EventMessage m;
        if (net::decode(data, m)) events.insert(events.end(), m.events.begin(), m.events.end());
        return;
    }
    case net::MessageType::reply: {
        net::ReplyMessage m;
        if (net::decode(data, m)) replies[m.request] = m;
        return;
    }
    default:
        return;  // the client's own kinds
    }
}

void HeadlessClient::tick(double now) {
    endpoint_.update(now);
    for (net::Event e; endpoint_.poll(e);) {
        if (e.type == net::Event::Type::disconnected) disconnectReason_ = e.reason;
        else if (e.type == net::Event::Type::message) message(e.data);
    }
    if (connected() && welcome_) {
        tick_ += 1;
        net::PilotInput in = steer;
        in.tick = tick_;
        sent_.push_front(in);
        while (sent_.size() > net::InputMessage::kMaxInputs) sent_.pop_back();
        net::InputMessage m;
        m.inputs.assign(sent_.begin(), sent_.end());
        const std::vector<uint8_t> input = net::encode(m);
        endpoint_.send(net::Channel::unreliable, input.data(), input.size());
        // Presses go out once; the repeats carry them for loss.
        steer.act = false;
        steer.ability = false;
        const net::AckMessage ack = reader_.ack();
        if (!ack.received.empty()) {
            const std::vector<uint8_t> bytes = net::encode(ack);
            endpoint_.send(net::Channel::unreliable, bytes.data(), bytes.size());
        }
    }
    endpoint_.flush(now);
}

} // namespace ac::server
