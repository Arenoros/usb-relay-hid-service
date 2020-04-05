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


int main() {

    mplc::InitializeSockets();
    assert(test_Accept());
    mplc::WSServer server(4444);
    server.Start();
    WSACleanup();
    return 0;
}
