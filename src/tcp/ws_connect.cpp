#pragma once
#include <regex>
#include "sha1.h"
#include "ws_connect.h"
#include <sstream>

namespace mplc {

    WSConnect::WSConnect(socket_t sock, sockaddr_in addr)
        : TcpSocket(sock), prev(WSFrame::Continue), state(Handshake), frame(recv_buf, MPLC_RECV_BUF_SIZE), addr(addr) {}

    void WSConnect::NewPayloadPart() {
        frame.decode();
        switch(prev) {
        case WSFrame::Text:
            OnText((const char*)frame.payload, frame.len, frame.fin);
            break;
        case WSFrame::Binary:
            OnBinary(frame.payload, frame.len, frame.fin);
            break;
        case WSFrame::Close:
            OnClose(frame);
            break;
        case WSFrame::Ping:
            OnPing(frame);
            break;
        case WSFrame::Pong:
            OnPong(frame);
            break;
        default:
            break;
        }
    }
    void WSConnect::NewFrame() {
        while(frame.has_frame()) {
            frame.next();
            prev = WSFrame::TOpcode(frame.opcode ? frame.opcode : prev);
            NewPayloadPart();
        }
    }
    std::string WSConnect::BaseHandshake() {
        std::string Key = headers["Sec-WebSocket-Key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        uint8_t hash[20] = {0};
        SHA1::make(Key, hash);
        Key = to_base64(hash);
        // clang-format off
        std::string handshake =
            headers["HTTP"]+" 101 Web Socket Protocol Handshake\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: "+Key+"\r\n"
            "WebSocket-Origin: http://"+headers["Host"]+"\r\n"
            "WebSocket-Location: ws://"+headers["Host"]+headers["GET"]+"\r\n";
        // clang-format on
        return handshake;
    }
    void WSConnect::ParsHeaders(const std::string& header) {
        size_t l_start = 0;
        auto l_end = header.find("\r\n");
        if(l_end == std::string::npos) return;
        const std::regex re("GET (.*) (.*)");
        std::match_results<std::string::const_iterator> match;
        if(std::regex_search(header.begin() + l_start, header.begin() + l_end, match, re)) {
            headers["GET"] = match[1];
            headers["HTTP"] = match[2];
            l_start = l_end + 2;
            l_end = header.find("\r\n", l_start);
        }
        const std::regex kv("(.*): (.*)");
        while(l_end != std::string::npos &&
              std::regex_search(header.begin() + l_start, header.begin() + l_end, match, kv)) {
            headers[match[1]] = match[2];
            l_start = l_end + 2;
            l_end = header.find("\r\n", l_start);
        }
    }

    void WSConnect::Disconnect() {
        Close();
        state = Closed;
    }

    int WSConnect::Read() {
        int n = Recv(recv_buf, sizeof(recv_buf));
        if(n == -1) {
            OnError(GetError());
            return n;
        }
        switch(state) {
        case Handshake:
            OnHttpHeader((char*)recv_buf, n);
            break;
        case Connected: {
            // If 'load' return less then n then buffer contains part of prev frame
            if(frame.load(n) < n) NewPayloadPart();
            // if in buff has more frames read it's all
            if(frame.has_frame()) NewFrame();
        } break;
        case Closed:
            return -1;
        }
        return n;
    }
    
    void WSConnect::OnHttpHeader(char* data, int size) {
        handshake.append(data, size);
        size_t pos = (size_t)size + 3 > handshake.size() ? 0 : handshake.size() - size - 3;
        if((pos = handshake.find("\r\n\r\n", pos)) != std::string::npos) {
            handshake.resize(pos + 2);
            ParsHeaders(handshake);
            SendHandshake();
            handshake.clear();
            state = Connected;
        } else if(handshake.size() > 4 * TcpSocket::MTU) {  // Handshake limit
            Close();
        }
    }
    void WSConnect::OnText(const char* payload, int size, bool fin) { print_text(payload); }
    void WSConnect::OnBinary(const uint8_t* payload, int size, bool fin) { print_bin(payload, size); }
    void WSConnect::OnClose(WSFrame& frame) {
        SendClose(frame);
        state = Closed;
    }
    void WSConnect::OnPing(WSFrame& frame) { SendPong(frame); }
    void WSConnect::OnPong(WSFrame&) {
        // client response on ping, update timer
    }
    void WSConnect::OnDisconnect() { printf("OnDiconect: %d\n", ec); }
    void WSConnect::OnError(error_code ec) { printf("OnError: %d\n", ec); }

    void WSConnect::SendHandshake() {
        std::string handshake = BaseHandshake() + "\r\n";
        PushData(handshake.begin(), handshake.end());
        SendData();
    }
    void WSConnect::SendBinary(const uint8_t* payload, int size, bool fin) {
        WSFrame frame;
        frame.fin = fin;
        frame.payload_len = size;
        frame.payload = payload;
        frame.opcode = WSFrame::Binary;
        frame.send_to(*this);
        SendData();
    }
    void WSConnect::SendText(const std::string& data, bool fin) {
        WSFrame frame;
        frame.fin = fin;
        frame.payload_len = data.size();
        frame.payload = (const uint8_t*)data.c_str();
        frame.opcode = WSFrame::Text;
        frame.send_to(*this);
        SendData();
    }
    void WSConnect::SendPing() {
        WSFrame frame;
        frame.fin = true;
        frame.opcode = WSFrame::Ping;
        frame.send_to(*this);
        SendData();
    }
    void WSConnect::SendPong(WSFrame& frame) {
        frame.opcode = WSFrame::Pong;
        frame.send_to(*this);
        SendData();
    }

    void WSConnect::SendClose(WSFrame& frame) {
        frame.send_to(*this);
        state = Closed;
        SendData();
    }
    std::string GenerateKey() {
        char rnd[16];
        for(int i = 0; i < 16; ++i) { rnd[i] = rand() & 0xFF; }
        return to_base64(rnd);
    }

    struct Url {
        enum State { sHost, sPort, sPath, sQuery } state;
        enum Type { unknown, wss, ws } type;
        std::string host;
        uint16_t port = 0;
        std::string path;
        std::string query;
        Url(): state(sHost), type(unknown), port(0) {}
        int from(const std::string& url) {
            if(url.substr(0, 6) == "wss://") {
                type = wss;
            } else if(url.substr(0, 5) == "ws://") {
                type = ws;
            }
            if(type == unknown) return -1;
            for(size_t i = (type == wss ? 6 : 5); i < url.size(); ++i) {
                switch(url[i]) {
                case ':':
                    if(state == sHost)
                        state = sPort;
                    else if(state == sQuery)
                        query += url[i];
                    else
                        return -1;
                    break;
                case '/':
                    if(state == sHost || state == sPort) {
                        state = sPath;
                        path += '/';
                    } else if(state == sPath) {
                        path += '/';
                    } else if(state == sQuery)
                        query += url[i];
                    else
                        return -1;
                    break;
                case '?':
                    if(state == sHost || state == sPort || state == sPath)
                        state = sQuery;
                    else
                        return -1;
                    break;
                default:
                    switch(state) {
                    case sHost:
                        host += url[i];
                        break;
                    case sPort:
                        if(url[i] >= '0' && url[i] <= '9') {
                            port *= 10;
                            port += url[i] - '0';
                        } else
                            return -1;
                        break;
                    case sPath:
                        path += url[i];
                        break;
                    case sQuery:
                        query += url[i];
                        break;
                    }
                }
            }
            if(path.empty()) path = "/";
            return 0;
        }
    };

    WSConnect* WSConnect::Open(const std::string& _url) {
        WSConnect* con;
        Url url;
        if(url.from(_url) != 0) return nullptr;
        con = new WSConnect();
        if(ResolveHost(url.host.c_str(), &con->addr) != 0) {
            delete con;
            return nullptr;
        }
        con->addr.sin_port = htons(url.port);
        if(con->Connect((struct sockaddr*)&con->addr) != 0) {
            delete con;
            return nullptr;
        }
        std::stringstream ss;
        ss << "GET " << url.path << " HTTP/1.1\r\n";
        ss << "Host: " << url.host;
        if(url.port) ss << ':' << url.port;
        ss << " \r\n";
        ss << "Upgrade: websocket\r\n";
        ss << "Connection: Upgrade\r\n";
        ss << "Sec-WebSocket-Key: " << GenerateKey() << "\r\n";
        // ss << "Origin: http://example.com\r\n";
        ss << "Sec-WebSocket-Protocol: chat, superchat\r\n";
        // ss << "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n";
        ss << "Sec-WebSocket-Version: 13\r\n";
        ss << "\r\n";
        std::string hendshake = ss.str();
        con->Send(hendshake.c_str(), hendshake.size());
        con->Read();
        return con;
    }
    WSConnect::~WSConnect() {}
}  // namespace mplc
