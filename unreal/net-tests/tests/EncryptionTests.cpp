// The encryption (docs/networking.md, "Encryption"): both ends derive the
// same keys, sealed packets open, and a packet changed on the way, replayed,
// or sealed with another connection's key is dropped and counted. The
// nonce never repeats, across the 16-bit sequence's wrap; the token never
// shows on the wire. One server, one client, captured on the in-memory net.
#include "test.h"

#include "NetConnection.h"
#include "NetCrypto.h"
#include "rig.h"

#include <algorithm>
#include <set>
#include <string>

using namespace nettest;

namespace {

/// Passes packets through and keeps a copy of each one sent.
class Tap final : public PacketIo {
public:
    explicit Tap(PacketIo& inner) : inner_(inner) {}

    bool send(double now, const Address& to, const uint8_t* data, size_t size) override {
        if (keep) sent.emplace_back(data, data + size);
        if (size >= kHeaderBytes) sequences.push_back(uint16_t(data[11] | data[12] << 8));
        return inner_.send(now, to, data, size);
    }
    bool receive(double now, Address& from, uint8_t* buffer, size_t capacity, size_t& size) override {
        return inner_.receive(now, from, buffer, capacity, size);
    }

    bool keep = true;
    std::vector<std::vector<uint8_t>> sent;
    /// Every packet's wire sequence, in order.
    std::vector<uint16_t> sequences;

private:
    PacketIo& inner_;
};

/// A server and a client on the in-memory net with both ends tapped.
struct Pair {
    explicit Pair(const ServerConfig& sc = serverConfig(), const ClientConfig& cc = clientConfig())
        : serverPort(net, kServerAddress), clientPort(net, kClientAddress), serverTap(serverPort),
          clientTap(clientPort), server(serverTap, sc), client(clientTap, cc) {}

    void step() {
        now += 1.0 / 60;
        server.update(now);
        for (Event e; server.poll(e);) serverEvents.push_back(std::move(e));
        client.update(now);
        for (Event e; client.poll(e);) clientEvents.push_back(std::move(e));
        server.flush(now);
        client.flush(now);
    }
    bool connect() {
        client.connect(now);
        for (int i = 0; i < 600; i++) {
            step();
            if (client.state() == ClientEndpoint::State::connected && server.connectionCount() == 1) {
                id = client.connectionId();
                return true;
            }
        }
        return false;
    }
    /// What the server has not read yet, taken away.
    std::deque<std::pair<Address, std::vector<uint8_t>>> takeServerInbox() {
        auto box = std::move(net.inbox[kServerAddress.key()]);
        net.inbox[kServerAddress.key()].clear();
        return box;
    }
    /// A packet as if from the client, read by the server now.
    void inject(const std::vector<uint8_t>& packet) {
        clientPort.send(now, kServerAddress, packet.data(), packet.size());
        server.update(now);
        for (Event e; server.poll(e);) serverEvents.push_back(std::move(e));
    }
    const ConnectionStats& serverStats() const { return *server.stats(id); }

    MemoryNet net;
    MemoryPort serverPort;
    MemoryPort clientPort;
    Tap serverTap;
    Tap clientTap;
    ServerEndpoint server;
    ClientEndpoint client;
    std::vector<Event> serverEvents;
    std::vector<Event> clientEvents;
    ConnectionId id = 0;
    double now = 100;
};

std::vector<uint8_t> bytesOf(const std::string& s) { return std::vector<uint8_t>(s.begin(), s.end()); }

bool contains(const std::vector<uint8_t>& hay, const std::vector<uint8_t>& needle) {
    return std::search(hay.begin(), hay.end(), needle.begin(), needle.end()) != hay.end();
}

/// The client sends one unreliable message; the sealed packet it made is
/// taken from the server's inbox before the server reads it.
std::vector<uint8_t> sealedFromClient(Pair& p, const std::string& message) {
    p.takeServerInbox();
    const auto m = bytesOf(message);
    p.client.send(Channel::unreliable, m.data(), m.size());
    p.client.flush(p.now);
    auto box = p.takeServerInbox();
    return box.empty() ? std::vector<uint8_t>{} : box.back().second;
}

size_t unreliableCount(const Pair& p, const std::string& message) {
    size_t n = 0;
    for (const auto& m : Rig::messages(p.serverEvents, Channel::unreliable)) n += m == bytesOf(message) ? 1 : 0;
    return n;
}

} // namespace

TEST(encryption_handshakeGivesBothEndsTheSameKeys) {
    Pair p;
    ASSERT_TRUE(p.connect());
    const Connection* c = p.client.connectionForTests();
    const Connection* s = p.server.connectionForTests(p.id);
    ASSERT_TRUE(c && s);
    EXPECT_TRUE(c->keys().clientToServer == s->keys().clientToServer);
    EXPECT_TRUE(c->keys().serverToClient == s->keys().serverToClient);
    EXPECT_TRUE(c->keys().clientToServer != c->keys().serverToClient);
    EXPECT_TRUE(c->keys().clientToServer != crypto::Key{});

    // Another connection, other keys.
    Pair q;
    ASSERT_TRUE(q.connect());
    EXPECT_TRUE(q.client.connectionForTests()->keys().clientToServer != c->keys().clientToServer);
    EXPECT_TRUE(q.client.connectionForTests()->keys().serverToClient != c->keys().serverToClient);
}

TEST(encryption_sealedRoundTrip) {
    // The primitive: what is sealed opens, with the header as it was.
    crypto::KeyPair a, b;
    ASSERT_TRUE(crypto::makeKeyPair(a) && crypto::makeKeyPair(b));
    crypto::SessionKeys ka, kb;
    ASSERT_TRUE(crypto::deriveKeys(a.secret, b.publicKey, a.publicKey, b.publicKey, 77, ka));
    ASSERT_TRUE(crypto::deriveKeys(b.secret, a.publicKey, a.publicKey, b.publicKey, 77, kb));
    EXPECT_TRUE(ka.clientToServer == kb.clientToServer && ka.serverToClient == kb.serverToClient);
    std::vector<uint8_t> packet(kHeaderBytes, 0x11);
    const auto text = bytesOf("the prospectors live in its shadow");
    packet.insert(packet.end(), text.begin(), text.end());
    std::vector<uint8_t> sealed, opened;
    crypto::seal(ka.clientToServer, crypto::Direction::clientToServer, 5, packet.data(), kHeaderBytes, packet.size(),
                 sealed);
    EXPECT_EQ(sealed.size(), packet.size() + kMacBytes);
    EXPECT_TRUE(std::equal(packet.begin(), packet.begin() + kHeaderBytes, sealed.begin()));
    EXPECT_TRUE(!contains(sealed, text));
    ASSERT_TRUE(crypto::open(kb.clientToServer, crypto::Direction::clientToServer, 5, sealed.data(), kHeaderBytes,
                             sealed.size(), opened));
    EXPECT_TRUE(opened == text);
    // Another counter or the other direction does not open it.
    EXPECT_TRUE(!crypto::open(kb.clientToServer, crypto::Direction::clientToServer, 6, sealed.data(), kHeaderBytes,
                              sealed.size(), opened));
    EXPECT_TRUE(!crypto::open(kb.clientToServer, crypto::Direction::serverToClient, 5, sealed.data(), kHeaderBytes,
                              sealed.size(), opened));

    // Through the endpoints, every channel both ways.
    Pair p;
    ASSERT_TRUE(p.connect());
    const auto r = bytesOf("reliable: a build request");
    const auto u = bytesOf("unreliable: an input");
    std::vector<uint8_t> bulk(20000);
    for (size_t i = 0; i < bulk.size(); i++) bulk[i] = uint8_t(i * 7);
    p.client.send(Channel::reliable, r.data(), r.size());
    p.client.send(Channel::unreliable, u.data(), u.size());
    p.server.send(p.id, Channel::reliable, r.data(), r.size());
    p.server.send(p.id, Channel::bulk, bulk.data(), bulk.size());
    for (int i = 0; i < 60; i++) p.step();
    EXPECT_TRUE(Rig::messages(p.serverEvents, Channel::reliable) == std::vector<std::vector<uint8_t>>{r});
    EXPECT_TRUE(Rig::messages(p.serverEvents, Channel::unreliable) == std::vector<std::vector<uint8_t>>{u});
    EXPECT_TRUE(Rig::messages(p.clientEvents, Channel::reliable) == std::vector<std::vector<uint8_t>>{r});
    EXPECT_TRUE(Rig::messages(p.clientEvents, Channel::bulk) == std::vector<std::vector<uint8_t>>{bulk});
    // None of it readable on the wire.
    for (const auto& sentPacket : p.clientTap.sent) EXPECT_TRUE(!contains(sentPacket, r) && !contains(sentPacket, u));
    for (const auto& sentPacket : p.serverTap.sent) EXPECT_TRUE(!contains(sentPacket, r));
    EXPECT_EQ(p.serverStats().packetsRejected, uint64_t(0));
    EXPECT_EQ(p.client.stats()->packetsRejected, uint64_t(0));
}

TEST(encryption_tamperedPacketsAreDropped) {
    Pair p;
    ASSERT_TRUE(p.connect());
    const std::vector<uint8_t> packet = sealedFromClient(p, "steer left");
    ASSERT_TRUE(packet.size() > kPacketOverheadBytes);

    // A byte of the header (the ack bits, the sequence), of the payload, of the tag.
    for (size_t at : {size_t(17), size_t(11), kHeaderBytes, kHeaderBytes + 3, packet.size() - 1}) {
        std::vector<uint8_t> changed = packet;
        changed[at] ^= 0x01;
        const uint64_t before = p.serverStats().packetsRejected;
        p.inject(changed);
        EXPECT_EQ(p.serverStats().packetsRejected, before + 1);
    }
    // Cut short, or just a header: dropped too.
    p.inject(std::vector<uint8_t>(packet.begin(), packet.end() - 1));
    p.inject(std::vector<uint8_t>(packet.begin(), packet.begin() + kHeaderBytes));
    EXPECT_EQ(p.serverStats().packetsRejected, uint64_t(7));
    EXPECT_EQ(unreliableCount(p, "steer left"), size_t(0));
    EXPECT_EQ(p.server.connectionCount(), size_t(1));

    // The packet as sent still opens: nothing was spent on the forgeries.
    p.inject(packet);
    EXPECT_EQ(unreliableCount(p, "steer left"), size_t(1));
}

TEST(encryption_replayedPacketsAreDropped) {
    Pair p;
    ASSERT_TRUE(p.connect());
    const std::vector<uint8_t> packet = sealedFromClient(p, "take the Prospector");
    p.inject(packet);
    EXPECT_EQ(unreliableCount(p, "take the Prospector"), size_t(1));
    p.inject(packet);
    p.inject(packet);
    EXPECT_EQ(unreliableCount(p, "take the Prospector"), size_t(1));
    EXPECT_EQ(p.serverStats().packetsReplayed, uint64_t(2));

    // Fifty packets later, all of them again: none gets through.
    std::vector<std::vector<uint8_t>> old;
    for (int i = 0; i < 50; i++) {
        old.push_back(sealedFromClient(p, "input " + std::to_string(i)));
        p.inject(old.back());
    }
    const size_t delivered = Rig::messages(p.serverEvents, Channel::unreliable).size();
    for (const auto& packet2 : old) p.inject(packet2);
    EXPECT_EQ(Rig::messages(p.serverEvents, Channel::unreliable).size(), delivered);
    EXPECT_EQ(p.serverStats().packetsReplayed, uint64_t(52));
    EXPECT_EQ(p.serverStats().packetsRejected, uint64_t(0));

    // The window: newest and the 1,024 before it; anything older is dropped.
    crypto::ReplayWindow w;
    EXPECT_TRUE(w.fresh(0));
    w.record(3);
    EXPECT_TRUE(w.fresh(0) && w.fresh(2) && !w.fresh(3) && w.fresh(4));
    w.record(2000);
    EXPECT_TRUE(!w.fresh(3) && !w.fresh(976) && w.fresh(977) && !w.fresh(2000) && w.fresh(1999));
    w.record(1999);
    EXPECT_TRUE(!w.fresh(1999) && w.fresh(1998) && w.fresh(2001));
}

TEST(encryption_anotherConnectionsKeyIsDropped) {
    Pair p;
    Pair other;
    ASSERT_TRUE(p.connect());
    ASSERT_TRUE(other.connect());
    const std::vector<uint8_t> packet = sealedFromClient(p, "build a Citadel");
    ASSERT_TRUE(!packet.empty());

    // The same header and payload, sealed with the other connection's key.
    const Connection* mine = p.client.connectionForTests();
    const uint64_t counter = mine->sendCounter() - 1;
    std::vector<uint8_t> plain;
    ASSERT_TRUE(crypto::open(mine->keys().clientToServer, crypto::Direction::clientToServer, counter, packet.data(),
                             kHeaderBytes, packet.size(), plain));
    std::vector<uint8_t> clear(packet.begin(), packet.begin() + kHeaderBytes);
    clear.insert(clear.end(), plain.begin(), plain.end());
    std::vector<uint8_t> forged;
    crypto::seal(other.client.connectionForTests()->keys().clientToServer, crypto::Direction::clientToServer, counter,
                 clear.data(), kHeaderBytes, clear.size(), forged);
    p.inject(forged);
    EXPECT_EQ(p.serverStats().packetsRejected, uint64_t(1));
    EXPECT_EQ(unreliableCount(p, "build a Citadel"), size_t(0));

    // The server's key of this connection, the other direction's: dropped too.
    crypto::seal(mine->keys().serverToClient, crypto::Direction::clientToServer, counter, clear.data(), kHeaderBytes,
                 clear.size(), forged);
    p.inject(forged);
    EXPECT_EQ(p.serverStats().packetsRejected, uint64_t(2));

    p.inject(packet);
    EXPECT_EQ(unreliableCount(p, "build a Citadel"), size_t(1));
}

TEST(encryption_nonceNeverRepeatsAcrossTheSequenceWrap) {
    ServerConfig sc = serverConfig();
    ClientConfig cc = clientConfig();
    cc.connection.bytesPerSecond = 1e9;
    Pair p(sc, cc);
    p.clientTap.keep = false;
    ASSERT_TRUE(p.connect());
    const uint64_t first = p.client.connectionForTests()->sendCounter();
    // Twenty full packets a tick until the client has sent more than 70,000.
    std::vector<uint8_t> m(kMaxUnreliableBytes, 0x42);
    uint64_t queued = 0;
    while (p.client.connectionForTests()->sendCounter() < 70000) {
        for (int i = 0; i < 20; i++) {
            m[0] = uint8_t(queued++);
            p.client.send(Channel::unreliable, m.data(), m.size());
        }
        p.step();
    }
    p.step();
    const uint64_t sent = p.client.connectionForTests()->sendCounter();
    EXPECT_TRUE(sent > 70000);

    // Each packet took the next counter; the sequence on the wire is its low
    // 16 bits and wrapped past 65,535.
    const std::vector<uint16_t>& seqs = p.clientTap.sequences;
    const size_t start = seqs.size() - size_t(sent - first);
    bool wrapped = false;
    for (size_t i = start; i < seqs.size(); i++) {
        const uint64_t counter = first + (i - start);
        if (seqs[i] != uint16_t(counter)) {
            EXPECT_EQ(seqs[i], uint16_t(counter));
            break;
        }
        wrapped = wrapped || (i > start && seqs[i - 1] == 65535 && seqs[i] == 0);
    }
    EXPECT_TRUE(wrapped);

    // The server widened every sequence to the right counter: all opened, none
    // taken for a replay, every message there.
    EXPECT_EQ(p.serverStats().packetsRejected, uint64_t(0));
    EXPECT_EQ(p.serverStats().packetsReplayed, uint64_t(0));
    EXPECT_EQ(Rig::messages(p.serverEvents, Channel::unreliable).size(), size_t(queued));

    // The nonces of every counter used, both directions: all different.
    std::set<crypto::Nonce> nonces;
    for (uint64_t c = 0; c < sent; c++) {
        nonces.insert(crypto::nonce(crypto::Direction::clientToServer, c));
        nonces.insert(crypto::nonce(crypto::Direction::serverToClient, c));
    }
    EXPECT_EQ(nonces.size(), size_t(2 * sent));

    // Widening near the wrap, both ways.
    EXPECT_EQ(crypto::widenCounter(65530, 3), uint64_t(65539));
    EXPECT_EQ(crypto::widenCounter(65539, 65530), uint64_t(65530));
    EXPECT_EQ(crypto::widenCounter(0, 5), uint64_t(5));
    EXPECT_EQ(crypto::widenCounter(10, 65535), uint64_t(65535));
    EXPECT_EQ(crypto::widenCounter(70000, uint16_t(70001)), uint64_t(70001));
}

TEST(encryption_theTokenNeverShowsOnTheWire) {
    std::vector<uint8_t> token;
    for (int i = 0; i < 32; i++) token.push_back(uint8_t(0xC0 ^ (i * 37)));
    ServerConfig sc = serverConfig();
    sc.token = token;
    ClientConfig cc = clientConfig();
    cc.token = token;
    Pair p(sc, cc);
    ASSERT_TRUE(p.connect());
    for (int i = 0; i < 120; i++) p.step();
    ASSERT_TRUE(!p.clientTap.sent.empty());
    const std::vector<uint8_t> head(token.begin(), token.begin() + 8);
    const std::vector<uint8_t> tail(token.end() - 8, token.end());
    size_t tokenPackets = 0;
    for (const auto& packet : p.clientTap.sent) {
        EXPECT_TRUE(!contains(packet, head) && !contains(packet, tail));
        tokenPackets += (packet.size() > 6 && (packet[6] & 0x7F) == uint8_t(PacketType::token)) ? 1 : 0;
    }
    for (const auto& packet : p.serverTap.sent) EXPECT_TRUE(!contains(packet, head) && !contains(packet, tail));
    EXPECT_TRUE(tokenPackets >= 1);

    // A wrong token is still refused, sealed or not.
    ClientConfig wrong = cc;
    wrong.token[31] ^= 1;
    Pair q(sc, wrong);
    EXPECT_TRUE(!q.connect());
    const Event* e = Rig::find(q.clientEvents, Event::Type::disconnected);
    ASSERT_TRUE(e);
    EXPECT_EQ(e->reason, std::string("bad token"));
}
