#pragma once
#include "platform_conf.h"
#include <vector>
#include <algorithm>

#define MAX_TRANSFER_UNIT 1350

namespace mplc {
    // struct BaseConnection {
    //    virtual void OnDisconnect() = 0;
    //    virtual int Read() = 0;
    //    virtual ~BaseConnection() {}
    //    // virtual int Write() = 0;
    //    // virtual const TcpSocket& Socket() const = 0;
    //};

    class TcpSocket {
    protected:
        socket_t sock_fd;
        error_code ec;
        void SetError() { ec = GetSockError(sock_fd); }
        std::vector<uint8_t> write_buf;
        uint8_t recv_buf[MPLC_RECV_BUF_SIZE];
        mplc::mutex buf_mtx;
        // typedef std::unique_ptr<BaseConnection> Connection;
        // Connection con;
    public:
        static const size_t MTU = MAX_TRANSFER_UNIT;
        virtual void OnDisconnect(){};
        virtual int Read() { return Recv(recv_buf, sizeof(recv_buf)); }
        template<class It>
        void PushData(const It& begin, const It& end) {
            mplc::lock_guard<mplc::mutex> lock(buf_mtx);
            write_buf.insert(write_buf.end(), begin, end);
        }
        int PushData(const uint8_t* data, int size) {
            if(size <= 0) return 0;
            mplc::lock_guard<mplc::mutex> lock(buf_mtx);
            write_buf.insert(write_buf.end(), data, data + size);
            return size;
        }
        int SendData() {
            mplc::lock_guard<mplc::mutex> lock(buf_mtx);
            int rc = Send(&write_buf[0], write_buf.size());
            write_buf.clear();
            return rc;
        }
        int OnRead() { return Read(); }
        // template<class TCon>
        // void SetConnection(BaseConnection* new_con) {
        //    con.reset(new_con);//(BaseConnection*)new TCon(*this, nsi);
        //}
        bool hasData() const { return !write_buf.empty(); }
        socket_t raw() const { return sock_fd; }
        TcpSocket() { SetSockDiecriptor(0); }
        TcpSocket(socket_t s) { SetSockDiecriptor(s); }

        TcpSocket& operator=(socket_t s) {
            SetSockDiecriptor(s);
            return *this;
        }
        void SetSockDiecriptor(socket_t s) {
            sock_fd = s;
            ec = 0;
        }
        error_code Open() {
            Close();
            SetSockDiecriptor(socket(AF_INET, SOCK_STREAM, 0));
            return GetSockError(sock_fd);
        }
        error_code SetOption(int opt, const char* val, int size) const {
            return setsockopt(sock_fd, SOL_SOCKET, opt, val, size);
        }
        static error_code ResolveHost(const char* host, struct sockaddr_in* addr) {
            struct hostent* ent;
            static const char dotdigits[] = "0123456789.";
            if(strspn(host, dotdigits) == strlen(host)) {
                /* given by IPv4 address */
                addr->sin_family = AF_INET;
                addr->sin_addr.s_addr = inet_addr(host);
            } else {
                // debug("resolving host by name: %s\n", host);
                ent = gethostbyname(host);
                if(ent) {
                    memcpy(&addr->sin_addr, ent->h_addr, ent->h_length);
                    addr->sin_family = ent->h_addrtype;
                    // debug("resolved: %s (%s)\n", host, inet_ntoa(addr->sin_addr));
                } else {
                    // debug("failed to resolve locally.\n");
                    return h_errno; /* failed */
                }
            }
            return 0; /* good */
        }
        error_code Bind(uint16_t port, const char* intface = nullptr) {
            struct sockaddr_in si;
            si.sin_port = htons(port);
            si.sin_addr.s_addr = intface ? inet_addr(intface) : INADDR_ANY;
            si.sin_family = AF_INET;
            if(bind(sock_fd, (struct sockaddr*)(&si), sizeof(si)) != 0) { return ec = GetLastSockError(); }
            return 0;
        }
        error_code Connect(struct sockaddr* addr) {
            Open();
            if(connect(sock_fd, addr, sizeof(struct sockaddr)) == -1) { ec = GetLastSockError(); }
            return ec;
        }
        error_code Connect(const char* host, uint16_t port) {
            struct sockaddr_in saddr;
            ec = ResolveHost(host, &saddr);
            if(ec) return ec;
            saddr.sin_port = htons(port);
            return Connect((struct sockaddr*)&saddr);
        }
        error_code GetError() const {
            if(isValid()) return GetSockError(sock_fd);
            return GetLastSockError();
        }
        error_code UnBlock() { return SetNoBlockSock(sock_fd); }
        bool isValid() const { return sock_fd != 0 && IsValidSock(sock_fd); }
        error_code Close() {
            ec = 0;
            if(isValid()) {
                OnDisconnect();
                ec = CloseSock(sock_fd);
            }
            sock_fd = 0;
            return ec;
        }
        // bool IsOpen() const { return IsValidSock(sock_fd) && ; }

        int Recv(void* buf, int size) const {
            char* ptr = (char*)buf;
            return recv(sock_fd, ptr, size, 0);
        }

        int TrySend(const void* resp, int len) {
            const char* ptr = (const char*)resp;
            return send(sock_fd, ptr, len, 0);
        }

        int Send(const void* resp, int len) {
            int limit = MTU;
            int n = 0;
            const char* ptr = (const char*)resp;
            while((n = send(sock_fd, ptr, std::min(limit, len), 0)) > 0) {
                ptr += n;
                len -= n;
            }
            return n > 0 ? len : n;
        }
    };

}  // namespace mplc
