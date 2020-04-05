#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <stddef.h>
#include <vector>
#include <thread>
#include <map>
#include <regex>

#include "platform_conf.h"

#include <cassert>
#include "sha1.h"
#include "ws_server.h"

bool test_Accept() {
    std::string Key = "dGhlIHNhbXBsZSBub25jZQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    uint8_t data[20];
    mplc::SHA1::make(Key, data);
    Key = mplc::to_base64(data);
    return Key == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
}

void on_error(SOCKET sock) {
    printf("Error: %d\n", WSAGetLastError());
    closesocket(sock);
}
class RemoteConnect : public mplc::WSConnect {
public:
    RemoteConnect(mplc::socket_t& connect, sockaddr_in nsi): WSConnect(connect, nsi) {}
    void OnText(const char* payload, int size, bool fin) override {
        std::string data(payload, size);
        std::cout << data;
    }
};
class CmdServer : public mplc::TCPServer<RemoteConnect> {
public:
    void OnConnected(mplc::socket_t connect, sockaddr_in nsi) override {
        printf("new user: %s\n", inet_ntoa(nsi.sin_addr));
        pool.Add(connect, nsi);
    }
    CmdServer(uint16_t port, const char* ip = nullptr): TCPServer(port, ip) {}
};
void test() {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    mplc::WSConnect* con = mplc::WSConnect::Open("ws://127.0.0.1:4444/chat?test=query");

    con->SendText("Helo from myself");
    delete con;
}

int main() {
    mplc::InitializeSockets();
    assert(test_Accept());
    //std::thread th(test);
    CmdServer server(4444);
    server.Start();
    WSACleanup();
    return 0;
}

//class RemoteCmd : public  mplc::WSConnect {
//    RemoteCmd(){}
//    void SendText(const std::string& data, bool fin) override;
//};

