#pragma once
#include <string>
#include <map>
#include <vector>

#include "platform_conf.h"
#include "ws_frame.h"
#include "tcp_socket.hpp"

namespace mplc {

    class WSConnect : public TcpSocket {
        friend class WSServer;
        WSFrame::TOpcode prev;
        std::string handshake;
        enum Status { Closed, Connected, Handshake };
        Status state;
        WSFrame frame;
        virtual void NewFrame();
        virtual void NewPayloadPart();

        WSConnect(): prev(WSFrame::Continue), state(Closed) { memset(&addr, 0, sizeof(sockaddr_in)); }

    protected:
        // TcpSocket& sock;
        struct sockaddr_in addr;
        // error_code ec;
        std::map<std::string, std::string> headers;
        std::string text;
        std::vector<uint8_t> binary;
        void OnHttpHeader(char* data, int size);
        std::string BaseHandshake();
        // error_code ReadHttpHeader(std::string& http);

    public:
        operator bool() const override { return state != Closed; }
        // const TcpSocket& Socket() const override { return sock; }
        WSConnect(socket_t sock, sockaddr_in addr);
        virtual ~WSConnect();
        virtual int Read() override;
        virtual void OnDisconnect() override;
        static WSConnect* Open(const std::string& url);
        virtual void ParsHeaders(const std::string& header);
        virtual void OnText(const char* payload, int size, bool fin);
        virtual void OnBinary(const uint8_t* payload, int size, bool fin);
        virtual void OnPing(WSFrame& frame);
        virtual void OnPong(WSFrame& frame);
        virtual void OnClose(WSFrame& frame);
        virtual void OnError(error_code ec);

        virtual void Disconnect();
        virtual void SendHandshake();
        virtual void SendText(const std::string& data, bool fin = true);
        virtual void SendBinary(const uint8_t* payload, int size, bool fin = true);
        virtual void SendPing();
        virtual void SendPong(WSFrame& frame);
        virtual void SendClose(WSFrame& frame);
    };

    class WSStream {};
}  // namespace mplc
