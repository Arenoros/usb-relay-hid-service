#pragma once

#include <list>
#include "platform_conf.h"
#include "tcp_socket.hpp"

namespace mplc {
    template<class ConnType = TcpSocket>
    struct ConnectionsPool {
        typedef std::list<ConnType*> ConLst;
        typedef typename std::list<ConnType*>::iterator con_iterator;
        static const size_t MAX_CONNECTIONS = FD_SETSIZE;
        static void AddToSet(const ConnType& sock, fd_set& set) { FD_SET(sock.raw(), &set); }
        static void RemFromSet(const ConnType& sock, fd_set& set) { FD_CLR(sock.raw(), &set); }
        static bool isContains(const ConnType& sock, const fd_set& set) { return FD_ISSET(sock.raw(), &set); }
        ConnectionsPool(): stop(false), th(&ConnectionsPool::worker, this) {
            FD_ZERO(&read_set);
            max = INVALID_SOCKET;
            ec = 0;
        }
        virtual ~ConnectionsPool() {
            stop = true;
            FD_ZERO(&read_set);
            th.join();
            for(con_iterator it = connections.begin(); it != connections.end(); ++it) {
                (*it)->Close();
                delete *it;
            }
            connections.clear();
        }
        void DeleteConnection(const con_iterator& it) {
            if(it == connections.end()) return;
            if((*it)->raw() == max) { max = INVALID_SOCKET; }
            RemFromSet(**it, read_set);
            (*it)->Close();
            delete *it;
            connections.erase(it);
        }
        template<class... ArgsT>
        void Add(ArgsT... args) {
            lock_guard<mutex> lock(con_mtx);
            ConnType* sock = new ConnType(args...);
            AddToSet(*sock, read_set);
            connections.push_back(sock);
            if(max < sock->raw()) max = sock->raw();
        }
        // void Add(ConnType* sock) {
        //
        //
        //}
        size_t size() {
            lock_guard<mutex> lock(con_mtx);
            return connections.size();
        }

        SOCKET GetMax() {
            if(max != INVALID_SOCKET) return max;
            max = 0;
            lock_guard<mutex> lock(con_mtx);
            for(con_iterator it = connections.begin(); it != connections.end(); ++it) {
                const ConnType& sock = **it;
                if(sock.raw() > max) max = sock.raw();
            }
            return max;
        }
        virtual void ReadSockets(const fd_set& rdst) {
            lock_guard<mutex> lock(con_mtx);
            con_iterator it = connections.begin();
            while(it != connections.end()) {
                ConnType& sock = **it;
                if(!sock || (isContains(sock, rdst) && sock.OnRead() == -1)) {
                    DeleteConnection(it++);
                } else {
                    ++it;
                }
            }
        }
        virtual void WriteSockets(const fd_set& wrst) {
            lock_guard<mutex> lock(con_mtx);
            con_iterator it = connections.begin();
            while(it != connections.end()) {
                ConnType& sock = **it;
                if(!sock || (isContains(sock, wrst) && sock.SendData() == -1)) {
                    DeleteConnection(it++);
                } else {
                    ++it;
                }
            }
        }

    protected:
        SOCKET max;
        fd_set read_set;
        ConLst connections;
        bool stop;
        error_code ec;
        thread th;
        mutex con_mtx;
        void worker() {
            while(!stop) {
                struct timeval tv;
                if(connections.empty()) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                // Ждем событий 1 сек
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                fd_set rdst, wrst;
                {
                    lock_guard<mutex> lock(con_mtx);
                    std::memcpy(&rdst, &read_set, sizeof(fd_set));
                }
                FD_ZERO(&wrst);
                for(con_iterator it = connections.begin(); it != connections.end(); ++it) {
                    ConnType& sock = **it;
                    if(sock.hasData()) { AddToSet(sock, wrst); }
                }
                SOCKET rv = select(GetMax() + 1, &rdst, nullptr, nullptr, &tv);
                if(rv == 0) continue;
                if(!IsValidSock(rv)) {
                    ec = GetLastSockError();
                    continue;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                ReadSockets(rdst);
                // WriteSockets(wrst);
            }
        }
    };
}  // namespace mplc
