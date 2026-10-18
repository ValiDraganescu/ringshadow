#include "NetClient.h"

#include <algorithm>

namespace ac::net {

Client::Client(PacketIo& io, const ClientConfig& config) : endpoint_(io, config) {}

void Client::connect(double now) {
    disconnectReason_.clear();
    reader_.reset();
    waiting_.clear();
    visit_ = 0;
    joinSent_ = false;
    endpoint_.connect(now);
}

void Client::join(const PilotKey& pilot) {
    pilotKey_ = pilot;
    joinSent_ = false;
    sendJoin();
}

void Client::sendJoin() {
    if (joinSent_ || !pilotKey_ || !connected()) return;
    const std::vector<uint8_t> bytes = encode(JoinMessage{*pilotKey_});
    endpoint_.send(Channel::reliable, bytes.data(), bytes.size());
    joinSent_ = true;
}

uint32_t Client::take(int64_t unit) { return request(RequestMessage::Take{unit}); }

uint32_t Client::handBack() { return request(RequestMessage::HandBack{}); }

uint32_t Client::fly(const std::string& region, const std::string& pad) {
    return request(RequestMessage::Fly{region, pad});
}

uint32_t Client::build(StructureKind kind, Vec2 at) {
    return request(RequestMessage::Build{kind, at});
}

uint32_t Client::pick(UnitKind kind, Perk perk) { return request(RequestMessage::Pick{kind, perk}); }

uint32_t Client::spend(Skill skill) { return request(RequestMessage::Spend{skill}); }

uint32_t Client::undo(Skill skill) { return request(RequestMessage::Undo{skill}); }

uint32_t Client::respec() { return request(RequestMessage::Respec{}); }

uint32_t Client::slot(int64_t index, std::optional<Skill> skill) { return request(RequestMessage::Slot{index, skill}); }
uint32_t Client::abandon(uint32_t contract) { return request(RequestMessage::Abandon{contract}); }
uint32_t Client::make(Module module, int64_t mark) { return request(RequestMessage::Make{module, mark}); }
uint32_t Client::research(Module module, int64_t mark) { return request(RequestMessage::Research{module, mark}); }
uint32_t Client::fit(Module module, int64_t mark) { return request(RequestMessage::Fit{module, mark}); }

uint32_t Client::donate(int64_t ore, int64_t hydrogen) { return request(RequestMessage::Donate{ore, hydrogen}); }
uint32_t Client::buyBlueprint(StructureKind kind) { return request(RequestMessage::Buy{kind, Item::repairKit}); }
uint32_t Client::buyItem(Item item) { return request(RequestMessage::Buy{std::nullopt, item}); }
uint32_t Client::use(Item item) { return request(RequestMessage::Use{item}); }
uint32_t Client::takeMission(uint32_t mission) { return request(RequestMessage::MissionAsk{mission, true}); }
uint32_t Client::abandonMission(uint32_t mission) { return request(RequestMessage::MissionAsk{mission, false}); }
uint32_t Client::merc(const RequestMessage::Merc& ask) { return request(ask); }
uint32_t Client::upgradeCitadel(DefenceStat stat) { return request(RequestMessage::UpgradeCitadel{stat}); }
uint32_t Client::startProject(uint8_t kind) { return request(RequestMessage::StartProject{kind}); }

uint32_t Client::callHunt() { return request(RequestMessage::CallHunt{}); }
uint32_t Client::upgradeTurret(int64_t turret, DefenceStat stat) { return request(RequestMessage::UpgradeTurret{turret, stat}); }
uint32_t Client::door(bool open) { return request(RequestMessage::Door{open}); }
uint32_t Client::man(int gun) { return request(RequestMessage::Man{int8_t(std::clamp(gun, -1, 3))}); }

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
        // One built ahead for this region (W4), else built now; the pilot's
        // side from the one it replaces until its first snapshot.
        const std::unique_ptr<Mirror> old = std::move(mirror_);
        tookPrepared_ = false;
        for (auto it = prepared_.begin(); it != prepared_.end(); ++it) {
            if ((*it)->region() != rc.region) continue;
            mirror_ = std::move(*it);
            prepared_.erase(it);
            tookPrepared_ = true;
            break;
        }
        if (!tookPrepared_) mirror_ = std::make_unique<Mirror>(rc.region);
        if (old) mirror_->inherit(*old);
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
    case MessageType::progress: {
        ProgressMessage m;
        if (!decode(data, m)) return;
        news.insert(news.end(), m.news.begin(), m.news.end());
        if (m.away) away = m.away;
        m.news.clear();
        m.away = std::nullopt;
        progress_ = m;
        return;
    }
    case MessageType::standings: {
        StandingsMessage m;
        if (decode(data, m)) standings_ = std::move(m);
        return;
    }
    case MessageType::hunt: {
        HuntMessage m;
        if (decode(data, m)) hunt_ = std::move(m);
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

void Client::prepare(std::unique_ptr<Mirror> mirror) {
    if (!mirror) return;
    // The same region again: the newer one.
    for (auto& m : prepared_) {
        if (m->region() != mirror->region()) continue;
        m = std::move(mirror);
        return;
    }
    prepared_.push_back(std::move(mirror));
    while (prepared_.size() > kMaxPrepared) prepared_.erase(prepared_.begin());
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
    sendJoin();
}

std::optional<PilotInput> Client::sendInput() {
    if (!connected() || !welcome_) return std::nullopt;
    tick_ += 1;
    PilotInput in = steer;
    in.tick = tick_;
    in.visit = visit_;
    sent_.push_front(in);
    while (sent_.size() > InputMessage::kMaxInputs) sent_.pop_back();
    InputMessage m;
    m.inputs.assign(sent_.begin(), sent_.end());
    const std::vector<uint8_t> input = encode(m);
    endpoint_.send(Channel::unreliable, input.data(), input.size());
    // Presses go out once; the repeats carry them for loss.
    steer.act = false;
    steer.ability = false;
    steer.slot = 0;
    const AckMessage ack = reader_.ack();
    if (!ack.received.empty()) {
        const std::vector<uint8_t> bytes = encode(ack);
        endpoint_.send(Channel::unreliable, bytes.data(), bytes.size());
    }
    return in;
}

void Client::flush(double now) { endpoint_.flush(now); }

void Client::disconnect(double now, const std::string& reason) { endpoint_.disconnect(now, reason); }

void Client::tick(double now) {
    receive(now);
    sendInput();
    flush(now);
}

} // namespace ac::net
