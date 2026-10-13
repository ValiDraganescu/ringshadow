// The handshake: hello, cookie, answer, welcome, token; the refusals;
// time-outs, keep-alive and disconnect. One server, one client, a few
// seconds each.
#include "test.h"

#include "NetConnection.h"
#include "NetCrypto.h"
#include "rig.h"

#include <string>

using namespace nettest;

namespace {

/// A hand-made handshake packet, padded to a full packet like the client's.
std::vector<uint8_t> handshakePacket(PacketType type, uint16_t version, uint64_t cookie,
                                     const crypto::Key& publicKey = {}) {
    PacketHeader h;
    h.version = version;
    h.type = type;
    BitWriter w;
    writeHeader(w, h);
    if (type == PacketType::answer) {
        w.writeU64(cookie);
        w.writeBytes(publicKey.data(), publicKey.size());
    }
    while (w.byteCount() < kMaxPacketBytes) w.writeU8(0);
    return w.bytes();
}

/// What the server sent back to `port`: the type, and the cookie or reason.
struct Reply {
    bool any = false;
    PacketType type = PacketType::data;
    uint64_t cookie = 0;
    uint32_t connection = 0;
    crypto::Key serverPublic{};
    std::string reason;
    size_t size = 0;
};

Reply readReply(MemoryPort& port) {
    Reply out;
    uint8_t buf[1500];
    Address from;
    size_t size = 0;
    if (!port.receive(0, from, buf, sizeof buf, size)) return out;
    BitReader r(buf, size);
    PacketHeader h;
    if (!readHeader(r, h)) return out;
    out.any = true;
    out.type = h.type;
    out.size = size;
    out.connection = h.connection;
    if (h.type == PacketType::challenge) out.cookie = r.readU64();
    if (h.type == PacketType::welcome) r.readBytes(out.serverPublic.data(), out.serverPublic.size());
    if (h.type == PacketType::refuse) {
        out.reason.resize(r.readU8());
        r.readBytes(reinterpret_cast<uint8_t*>(out.reason.data()), out.reason.size());
    }
    return out;
}

} // namespace

TEST(handshake_connects) {
    Rig rig;
    ASSERT_TRUE(rig.connect());
    const Event* c = Rig::find(rig.clientEvents, Event::Type::connected);
    const Event* s = Rig::find(rig.serverEvents, Event::Type::connected);
    ASSERT_TRUE(c && s);
    EXPECT_TRUE(c->connection != 0);
    EXPECT_EQ(c->connection, s->connection);
    EXPECT_TRUE(rig.server.stats(s->connection) != nullptr);
}

TEST(handshake_connectsThroughLoss) {
    LagSettings lag = roughLag();
    lag.loss = 0.3;  // hellos, cookies, answers and welcomes get lost and are sent again
    Rig rig(lag);
    ASSERT_TRUE(rig.connect());
    EXPECT_EQ(rig.server.connectionCount(), size_t(1));
}

TEST(handshake_versionMismatchIsRefusedWithTheServersVersion) {
    ClientConfig cc = clientConfig();
    cc.version = kProtocolVersion + 1;
    Rig rig({}, serverConfig(), cc);
    EXPECT_TRUE(!rig.connect());
    EXPECT_EQ(rig.client.state(), ClientEndpoint::State::disconnected);
    const Event* e = Rig::find(rig.clientEvents, Event::Type::disconnected);
    ASSERT_TRUE(e);
    EXPECT_EQ(e->reason, "the server runs version " + std::to_string(kProtocolVersion));
    EXPECT_EQ(rig.server.connectionCount(), size_t(0));
}

TEST(handshake_badTokenIsRefused) {
    ClientConfig cc = clientConfig();
    cc.token = {1, 2, 3, 4, 5, 6, 7, 9};
    Rig rig({}, serverConfig(), cc);
    EXPECT_TRUE(!rig.connect());
    const Event* e = Rig::find(rig.clientEvents, Event::Type::disconnected);
    ASSERT_TRUE(e);
    EXPECT_EQ(e->reason, std::string("bad token"));
    EXPECT_EQ(rig.server.connectionCount(), size_t(0));

    ClientConfig shorter = clientConfig();
    shorter.token = {1, 2, 3};
    Rig rig2({}, serverConfig(), shorter);
    EXPECT_TRUE(!rig2.connect());
    const Event* e2 = Rig::find(rig2.clientEvents, Event::Type::disconnected);
    ASSERT_TRUE(e2);
    EXPECT_EQ(e2->reason, std::string("bad token"));
}

TEST(handshake_badCookieIsRefused) {
    MemoryNet net;
    MemoryPort serverPort(net, kServerAddress);
    MemoryPort a(net, Address::ipv4(10, 0, 0, 7, 7000));
    MemoryPort b(net, Address::ipv4(10, 0, 0, 8, 7000));
    ServerEndpoint server(serverPort, serverConfig());
    double now = 50;

    crypto::KeyPair keys;
    ASSERT_TRUE(crypto::makeKeyPair(keys));
    // A made-up cookie.
    auto answer = handshakePacket(PacketType::answer, kProtocolVersion, 0x1234, keys.publicKey);
    a.send(now, kServerAddress, answer.data(), answer.size());
    server.update(now);
    Reply r = readReply(a);
    EXPECT_EQ(r.type, PacketType::refuse);
    EXPECT_EQ(r.reason, std::string("bad cookie"));
    EXPECT_EQ(server.connectionCount(), size_t(0));

    // A's real cookie, sent from B: the cookie is bound to the address.
    auto hello = handshakePacket(PacketType::hello, kProtocolVersion, 0);
    a.send(now, kServerAddress, hello.data(), hello.size());
    server.update(now);
    r = readReply(a);
    ASSERT_TRUE(r.type == PacketType::challenge);
    EXPECT_TRUE(r.size <= kMaxPacketBytes);
    const uint64_t cookieA = r.cookie;
    answer = handshakePacket(PacketType::answer, kProtocolVersion, cookieA, keys.publicKey);
    b.send(now, kServerAddress, answer.data(), answer.size());
    server.update(now);
    r = readReply(b);
    EXPECT_EQ(r.reason, std::string("bad cookie"));

    // A's cookie after two windows: stale.
    now += 25;
    a.send(now, kServerAddress, answer.data(), answer.size());
    server.update(now);
    r = readReply(a);
    EXPECT_EQ(r.reason, std::string("bad cookie"));
    EXPECT_EQ(server.connectionCount(), size_t(0));

    // A fresh cookie from A works: a welcome with the server's public key,
    // but no connection until the token comes sealed.
    a.send(now, kServerAddress, hello.data(), hello.size());
    server.update(now);
    r = readReply(a);
    const uint64_t cookie = r.cookie;
    answer = handshakePacket(PacketType::answer, kProtocolVersion, cookie, keys.publicKey);
    a.send(now, kServerAddress, answer.data(), answer.size());
    server.update(now);
    r = readReply(a);
    ASSERT_TRUE(r.type == PacketType::welcome);
    EXPECT_TRUE(r.connection != 0);
    EXPECT_EQ(server.connectionCount(), size_t(0));

    // The token, sealed by hand with the keys both ends derive.
    crypto::SessionKeys session;
    ASSERT_TRUE(crypto::deriveKeys(keys.secret, r.serverPublic, keys.publicKey, r.serverPublic, cookie, session));
    PacketHeader h;
    h.type = PacketType::token;
    h.connection = r.connection;
    BitWriter w;
    writeHeader(w, h);
    w.writeU8(static_cast<uint8_t>(kToken.size()));
    w.writeBytes(kToken.data(), kToken.size());
    std::vector<uint8_t> sealed;
    crypto::seal(session.clientToServer, crypto::Direction::clientToServer, 0, w.bytes().data(), kHeaderBytes,
                 w.byteCount(), sealed);
    a.send(now, kServerAddress, sealed.data(), sealed.size());
    server.update(now);
    EXPECT_EQ(server.connectionCount(), size_t(1));
    const Connection* c = server.connectionForTests(r.connection);
    ASSERT_TRUE(c);
    EXPECT_TRUE(c->keys().clientToServer == session.clientToServer);
    EXPECT_TRUE(c->keys().serverToClient == session.serverToClient);
}

TEST(handshake_helloKeepsNoStateAndShortHellosGetNothing) {
    MemoryNet net;
    MemoryPort serverPort(net, kServerAddress);
    ServerEndpoint server(serverPort, serverConfig());
    auto hello = handshakePacket(PacketType::hello, kProtocolVersion, 0);
    for (int i = 0; i < 1000; i++) {
        MemoryPort p(net, Address::ipv4(10, 1, uint8_t(i >> 8), uint8_t(i), 9000));
        p.send(1, kServerAddress, hello.data(), hello.size());
    }
    server.update(1);
    EXPECT_EQ(server.connectionCount(), size_t(0));
    EXPECT_EQ(serverPort.sent, uint64_t(1000));  // a challenge each, nothing kept

    // A hello not padded to a full packet could make the server a
    // amplifier: no reply.
    MemoryPort p(net, Address::ipv4(10, 2, 0, 1, 9000));
    p.send(2, kServerAddress, hello.data(), 100);
    server.update(2);
    EXPECT_TRUE(!readReply(p).any);
}

TEST(handshake_timesOutWithNoServer) {
    MemoryNet net;
    MemoryPort clientPort(net, kClientAddress);
    ClientEndpoint client(clientPort, clientConfig());
    double now = 0;
    client.connect(now);
    std::vector<Event> events;
    while (now < 9.9) {
        now += 1.0 / 60;
        client.update(now);
        client.flush(now);
    }
    EXPECT_EQ(client.state(), ClientEndpoint::State::hello);
    EXPECT_TRUE(clientPort.sent >= 36);  // a hello every 0.25 s (16 ticks)
    while (now < 10.1) {
        now += 1.0 / 60;
        client.update(now);
        client.flush(now);
    }
    EXPECT_EQ(client.state(), ClientEndpoint::State::disconnected);
    Event e;
    ASSERT_TRUE(client.poll(e));
    EXPECT_EQ(e.type, Event::Type::disconnected);
    EXPECT_EQ(e.reason, std::string("timed out"));
}

TEST(handshake_idleConnectionStaysUpOnKeepAlives) {
    Rig rig(roughLag());
    ASSERT_TRUE(rig.connect());
    const uint64_t before = rig.server.stats(rig.serverConnection)->packetsSent;
    for (int i = 0; i < 60 * 60; i++) rig.step();
    EXPECT_EQ(rig.client.state(), ClientEndpoint::State::connected);
    EXPECT_EQ(rig.server.connectionCount(), size_t(1));
    // At least a packet a second each way.
    EXPECT_TRUE(rig.server.stats(rig.serverConnection)->packetsSent - before >= 60);
    EXPECT_TRUE(rig.client.stats()->packetsSent >= 60);
    EXPECT_TRUE(!Rig::find(rig.clientEvents, Event::Type::disconnected));
}

TEST(handshake_silenceTimesOutAfterTenSeconds) {
    Rig rig;
    ASSERT_TRUE(rig.connect());
    // The server goes quiet: the client hears nothing.
    const double silentFrom = rig.now;
    while (rig.client.state() == ClientEndpoint::State::connected && rig.now < silentFrom + 20) {
        rig.now += 1.0 / 60;
        rig.client.update(rig.now);
        rig.client.flush(rig.now);
    }
    EXPECT_NEAR(rig.now - silentFrom, 10.0, 0.6);
    Event e;
    ASSERT_TRUE(rig.client.poll(e));
    ASSERT_TRUE(e.type == Event::Type::disconnected);
    EXPECT_EQ(e.reason, std::string("timed out"));

    // And the server, left alone, times the client out.
    for (int i = 0; i < 60 * 11; i++) {
        rig.now += 1.0 / 60;
        rig.server.update(rig.now);
    }
    EXPECT_EQ(rig.server.connectionCount(), size_t(0));
    Event s;
    bool timedOut = false;
    while (rig.server.poll(s)) timedOut = timedOut || (s.type == Event::Type::disconnected && s.reason == "timed out");
    EXPECT_TRUE(timedOut);
}

TEST(handshake_disconnectTellsTheOtherSideWhy) {
    Rig rig(roughLag());
    ASSERT_TRUE(rig.connect());
    rig.client.disconnect(rig.now, "the player quit");
    for (int i = 0; i < 30; i++) rig.step();
    const Event* e = Rig::find(rig.serverEvents, Event::Type::disconnected);
    ASSERT_TRUE(e);
    EXPECT_EQ(e->reason, std::string("the player quit"));
    EXPECT_EQ(rig.server.connectionCount(), size_t(0));

    Rig rig2(roughLag());
    ASSERT_TRUE(rig2.connect());
    rig2.server.disconnect(rig2.serverConnection, rig2.now, "the server stopped");
    for (int i = 0; i < 30; i++) rig2.step();
    const Event* c = Rig::find(rig2.clientEvents, Event::Type::disconnected);
    ASSERT_TRUE(c);
    EXPECT_EQ(c->reason, std::string("the server stopped"));
    EXPECT_EQ(rig2.client.state(), ClientEndpoint::State::disconnected);
}
