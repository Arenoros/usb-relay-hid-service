#include "ws_server.h"

namespace mplc {
    
    void WSServer::OnConnected(socket_t sock, sockaddr_in nsi) {
        printf("Connected: %s:%d\n", inet_ntoa(nsi.sin_addr), ntohs(nsi.sin_port));
        pool.Add(sock, nsi);
    }

}  // namespace mplc
