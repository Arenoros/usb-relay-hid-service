#pragma once
#include "tcp_server.hpp"
#include "ws_connect.h"

namespace mplc {
    class WSServer : public TCPServer<WSConnect> {
    public:
        void OnConnected(socket_t sock, sockaddr_in addr) override;
        WSServer() {}
    };

    std::list<std::string> messages;
    struct UserConn : WSConnect {
        UserConn(socket_t sock, sockaddr_in& addr): WSConnect(sock, addr) {}
        void OnText(const char* payload, int size, bool fin) override {
            printf("new message from: %s\n", inet_ntoa(addr.sin_addr));
            print_text(payload);
            messages.push_back(payload);
            for(auto& msg: messages) { SendText(msg); }
        }
        void SendHandshake() override {
            std::string handshake = BaseHandshake()+"\r\n";
            PushData(handshake.begin(), handshake.end());
        }
    };
    class Chat : public WSServer {
    public:
        void OnConnected(socket_t sock, sockaddr_in addr) override {
            printf("new user: %s\n", inet_ntoa(addr.sin_addr));
            pool.Add(sock, addr);
        }
        //Chat(uint16_t port, const char* ip = nullptr): WSServer(port, ip) {}
    };
}  // namespace mplc
