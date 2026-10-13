// Real UDP sockets on 127.0.0.1: the server binds port 0 and the system
// picks; a client connects through the lag simulator and sends reliable
// messages and a bulk transfer both ways.
#include "test.h"

#include "rig.h"

#include <chrono>
#include <thread>

using namespace nettest;

TEST(socket_loopbackConnectsAndCarriesAllChannels) {
    UdpSocket serverSocket, clientSocket;
    ASSERT_TRUE(serverSocket.open(Address::loopback(0)));
    ASSERT_TRUE(clientSocket.open(Address::loopback(0)));
    EXPECT_TRUE(serverSocket.localAddress().port != 0);
    EXPECT_TRUE(serverSocket.localAddress() == Address::loopback(serverSocket.localAddress().port));

    LagSimulator clientLag(clientSocket, 3);
    LagSettings lag;
    lag.delay = 0.01;
    lag.loss = 0.05;
    lag.reorder = 0.05;
    clientLag.setDefault(lag);

    ServerConfig sc = serverConfig();
    ASSERT_TRUE(secureRandom(sc.secret.data(), sc.secret.size()));
    sc.token.assign(16, 0);
    ASSERT_TRUE(secureRandom(sc.token.data(), sc.token.size()));
    ClientConfig cc = clientConfig();
    cc.server = serverSocket.localAddress();
    cc.token = sc.token;
    ServerEndpoint server(serverSocket, sc);
    ClientEndpoint client(clientLag, cc);

    std::vector<uint8_t> bulk(100 * 1024);
    for (size_t i = 0; i < bulk.size(); i++) bulk[i] = static_cast<uint8_t>(i * 7);
    std::vector<Event> serverEvents, clientEvents;
    ConnectionId id = 0;
    uint32_t sentUp = 0, sentDown = 0;
    bool bulkQueued = false;
    double now = 0;
    client.connect(now);
    // The clock runs at 5 ms a step while the real one sleeps 1 ms: the
    // packets cross the loopback in far less.
    for (int step = 0; step < 4000; step++) {
        now += 0.005;
        server.update(now);
        for (Event e; server.poll(e);) {
            if (e.type == Event::Type::connected) id = e.connection;
            serverEvents.push_back(std::move(e));
        }
        client.update(now);
        for (Event e; client.poll(e);) clientEvents.push_back(std::move(e));
        if (client.state() == ClientEndpoint::State::connected && id != 0) {
            if (sentUp < 200) {
                const auto m = u32Bytes(sentUp++);
                client.send(Channel::reliable, m.data(), m.size());
            }
            if (sentDown < 200) {
                const auto m = u32Bytes(sentDown++);
                server.send(id, Channel::reliable, m.data(), m.size());
            }
            if (!bulkQueued) bulkQueued = server.send(id, Channel::bulk, bulk.data(), bulk.size());
        }
        server.flush(now);
        client.flush(now);
        if (Rig::messages(serverEvents, Channel::reliable).size() == 200 &&
            Rig::messages(clientEvents, Channel::reliable).size() == 200 &&
            Rig::messages(clientEvents, Channel::bulk).size() == 1)
            break;
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
    ASSERT_TRUE(id != 0);
    const auto up = Rig::messages(serverEvents, Channel::reliable);
    const auto down = Rig::messages(clientEvents, Channel::reliable);
    const auto got = Rig::messages(clientEvents, Channel::bulk);
    ASSERT_TRUE(up.size() == 200 && down.size() == 200 && got.size() == 1);
    for (uint32_t i = 0; i < 200; i++) {
        EXPECT_EQ(bytesU32(up[i]), i);
        EXPECT_EQ(bytesU32(down[i]), i);
    }
    EXPECT_TRUE(got[0] == bulk);

    client.disconnect(now, "done");
    bool gone = false;
    for (int step = 0; step < 200 && !gone; step++) {
        now += 0.005;
        clientLag.pump(now);  // the disconnect waits in the simulator
        server.update(now);
        for (Event e; server.poll(e);) gone = gone || (e.type == Event::Type::disconnected && e.reason == "done");
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
    EXPECT_TRUE(gone);
}

TEST(socket_bindingATakenPortFails) {
    UdpSocket a, b;
    ASSERT_TRUE(a.open(Address::loopback(0)));
    EXPECT_TRUE(!b.open(a.localAddress()));
    EXPECT_TRUE(!b.error().empty());
    EXPECT_TRUE(!b.isOpen());
}
