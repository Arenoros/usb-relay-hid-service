#pragma once

#define PLATFORM_WINDOWS 1
#define PLATFORM_MAC 2
#define PLATFORM_UNIX 3
#define MPLC_RECV_BUF_SIZE 1024 * 10

#if defined(_WIN32)
#    define NOMINMAX
#    define PLATFORM PLATFORM_WINDOWS
#elif defined(__APPLE__)
#    define PLATFORM PLATFORM_MAC
#else
#    define PLATFORM PLATFORM_UNIX
#endif

#ifndef MPLC_WS_API
#    if PLATFORM == PLATFORM_WINDOWS
#        ifdef MPLC_WS_API_EXPORTS
#            define MPLC_WS_API __declspec(dllexport)
#        else
#            define MPLC_WS_API __declspec(dllimport)
#        endif
#    else
#        define MPLC_WS_API
#    endif
#endif

#if PLATFORM == PLATFORM_WINDOWS
#    define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#    if defined(WINCE)
#        include <Winsock2.h>
#        pragma comment(lib, "ws2.lib")
#    elif _MSC_VER < 1300
#        include <winsock.h>
#        pragma comment(lib, "wsock32.lib")
#    else
#        ifdef FD_SETSIZE
#            undef FD_SETSIZE
#            define FD_SETSIZE 1024
#        endif
#        include <Winsock2.h>
#        include <ws2tcpip.h>
#        pragma comment(lib, "ws2_32.lib")
#    endif
#    define bzero(a, b) memset((a), 0x0, (b))
#elif PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_UNIX
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <fcntl.h>
typedef int SOCKET;
#endif
#include <cstdint>

typedef int error_code;
namespace mplc {

    inline bool InitializeSockets() {
#if PLATFORM == PLATFORM_WINDOWS
        WSADATA WsaData;
        return WSAStartup(MAKEWORD(2, 2), &WsaData) == NO_ERROR;
#else
        return true;
#endif
    }

    inline void ShutdownSockets() {
#if PLATFORM == PLATFORM_WINDOWS
        WSACleanup();
#endif
    }
    inline error_code GetSockError(SOCKET s) {
        int error = 0;
        int len = sizeof(error);
        int ret = getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&error, &len);
        if(ret != 0) return ret;
        return error;
    }
    inline error_code GetLastSockError() {
#if PLATFORM == PLATFORM_WINDOWS
        return WSAGetLastError();
#else
        return errno;
#endif
    }

    inline bool IsValidSock(SOCKET sock) { return sock != INVALID_SOCKET; }
    inline error_code CloseSock(SOCKET sock) {
#if PLATFORM == PLATFORM_WINDOWS
        return closesocket(sock);
#else
#    if defined(SHUT_RDWR)
        if(shutdown(sock, SHUT_RDWR) == -1) return GetLastSockError();
#    endif
        if(close(sock) == -1) return GetLastSockError();
        return 0;
#endif
    }
    inline error_code SetNoBlockSock(SOCKET sock) {
#if PLATFORM != PLATFORM_WINDOWS
        int flags = fcntl(sock, F_GETFL);
        if(flags == -1) {
            PRINT2("%ld: Error open TCP socket (%d)!", RGetTime_ms(), RGetLastError());
            CR;
        }
        flags = flags | O_NONBLOCK;
        if(fcntl(sock, F_SETFL, flags) == -1) return GetLastSockError();
        return 0;
#else
        u_long on_sock = 1;
        return ioctlsocket(sock, FIONBIO, &on_sock);
#endif
    }

}  // namespace mplc

#include <mutex>

namespace mplc {
    using std::lock_guard;
    using std::mutex;
    using std::thread;
    typedef SOCKET socket_t;
}  // namespace mplc
