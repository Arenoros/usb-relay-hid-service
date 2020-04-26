#define _CRT_SECURE_NO_WARNINGS
#define RAPIDJSON_HAS_STDSTRING 1
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
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <thread_pool.h>
#include <filetime.h>
#include <usb_relay_device.h>

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
template<class... T>
void LogMsg(const char* format, T... args) {
    printf_s(format, args...);
}
using Writer = rapidjson::Writer<rapidjson::StringBuffer>;
void print_json(const rapidjson::Value& json) {
    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    json.Accept(writer);
    LogMsg(sb.GetString());
}

struct USBDevice {
    typedef std::shared_ptr<USBDevice> ptr;
    std::string name;
    int ch_count;
    intptr_t hndl;
    USBDevice(intptr_t hndl): hndl(hndl), ch_count(0) {}
    std::array<bool, 8> GetState(int* rc = nullptr) {
        std::array<bool, 8> state;
        state.fill(false);
        if(!hndl) {
            if(rc) *rc = 6;
            return state;
        }
        // Get and show state:
        // BUGBUG g_dev is not usb_relay_device_info* in the orig library! use my new f()  ?? use only info in the
        // list?
        struct usb_relay_device_info* di = (struct usb_relay_device_info*)hndl;
        ch_count = (unsigned char)di->type;
        if(ch_count > state.size() || ch_count == 0) {
            // oops?
            ch_count = state.size();
        }
        // g_chanNum = numChannels;
        unsigned st = 0;
        int ret = usb_relay_device_get_status(hndl, &st);
        if(ret != 0) {
            if(rc) *rc = 6;
            return state;
        }

        for(int i = 0; i < state.size(); i++) {
            bool val = st & (1 << i);
            if(((i + 1) > ch_count)) val = false;
            state[i] = val;
        }
        if(rc) *rc = 0;
        return state;
    }
    int SetChannel(int ch, bool val) const {
        if(!hndl) return 6;
        if(val) return usb_relay_device_open_one_relay_channel(hndl, ch + 1);
        return usb_relay_device_close_one_relay_channel(hndl, ch + 1);
    }
    static ptr open(const std::string& serial_num) {
        ptr dev;
        intptr_t dh = usb_relay_device_open_with_serial_number(serial_num.c_str(), (int)serial_num.size());
        if(dh) {
            dev = std::make_shared<USBDevice>(dh);
            dev->name = serial_num;
            struct usb_relay_device_info* di = (struct usb_relay_device_info*)dh;
            dev->ch_count = (unsigned char)di->type;
        }
        return dev;
    }
    void close() { usb_relay_device_close(hndl); }
    ~USBDevice() { close(); }
};
class USBMonitor {
    std::mutex mtx;
    mplc::AsyncTask rescan;
    pusb_relay_device_info_t g_enumlist;
    void ScanUsb() {
        std::lock_guard<std::mutex> lock(mtx);
        if(g_enumlist) {
            usb_relay_device_free_enumerate(g_enumlist);
            g_enumlist = nullptr;
        }
        g_enumlist = usb_relay_device_enumerate();
    }
    USBMonitor()
        : rescan(mplc::FileTime(2, mplc::FileTime::s), std::bind(&USBMonitor::ScanUsb, this)), g_enumlist(nullptr) {}
    std::map<std::string, USBDevice::ptr> opened;

public:
    void GetList(std::vector<std::string>& serials) {
        std::lock_guard<std::mutex> lock(mtx);
        pusb_relay_device_info_t q = g_enumlist;
        while(q) {
            serials.push_back(q->serial_number);
            q = q->next;
        }
    }
    USBDevice::ptr GetDevice(const std::string& serial_num) {
        auto& dev = opened[serial_num];
        if(!dev) dev = USBDevice::open(serial_num);
        return dev;
    }
    static USBMonitor& instance() {
        static USBMonitor monitor;
        return monitor;
    }
};

class RemoteConnect : public mplc::WSConnect {
    std::vector<char> buf;
    mplc::AsyncTask update;
    void UpdateState() {
        if(cur_dev) {
            rapidjson::StringBuffer sb;
            Writer writer(sb);
            writer.StartObject();

            writer.Key("method");
            writer.String("get");

            int rc = GetDeviceState(writer);

            writer.Key("code");
            writer.Int(rc);

            writer.EndObject();
            sb.Put('\0');
            SendText(sb.GetString());
        }
    }
    static const int MaxRelaysNum = 8;
    std::mutex mtx;
    USBMonitor& monitor;
    USBDevice::ptr cur_dev;

public:
    RemoteConnect(mplc::socket_t& connect, sockaddr_in nsi)
        : WSConnect(connect, nsi),
          update(mplc::FileTime(500, mplc::FileTime::ms), std::bind(&RemoteConnect::UpdateState, this)),
          monitor(USBMonitor::instance()) {}

    void OnText(const char* payload, int size, bool fin) override {
        buf.insert(buf.end(), payload, payload + size);
        if(fin && !buf.empty()) {
            buf.push_back(0);
            LogMsg("Request: %s\n", buf.data());
            rapidjson::StringBuffer sb;
            Writer writer(sb);
            writer.StartObject();
            rapidjson::Document request;
            request.ParseInsitu(buf.data());
            if(request.HasParseError()) {
                LogMsg("Error parsing json\n%s \nerror: %s (pos %ld) \n",
                       buf.data(),
                       rapidjson::GetParseError_En(request.GetParseError()),
                       request.GetErrorOffset());
                writer.Key("code");
                writer.Int(request.GetParseError());
            } else {
                int rc = OnJson(request, writer);
                writer.Key("code");
                writer.Int(rc);
            }
            writer.EndObject();
            sb.Put('\0');
            SendText(sb.GetString());
            buf.clear();
        }
    }
    int OnJson(const rapidjson::Value& request, Writer& writer) {
        if(request.HasMember("method") && request["method"].IsString()) {
            std::string method = request["method"].GetString();
            writer.Key("method");
            writer.String(method);
            if(method == "list") { return ListDevices(request, writer); }
            if(method == "get") { return GetDevice(request, writer); }
            if(method == "set") { return SetChannel(request, writer); }
        }
        return 8;
    }
    int SetChannel(const rapidjson::Value& request, Writer& writer) {
        // if(!request.HasMember("id") && !request["id"].IsString()) return 1;
        if(!request.HasMember("channel") && !request["channel"].IsInt()) return 2;
        if(!request.HasMember("value") && !request["value"].IsBool()) return 3;
        std::lock_guard<std::mutex> lock(mtx);
        if(!cur_dev) {
            writer.Key("error");
            writer.String("Error opening the device");
            return 6;
        }
        int ch = request["channel"].GetInt();
        bool val = request["value"].GetBool();
        int rc = cur_dev->SetChannel(ch, val);
        if(rc != 0) {
            writer.Key("error");
            writer.String("Error reading device state");
            return rc;
        }

        writer.Key("data");
        writer.StartObject();
        writer.Key("value");
        writer.Bool(val);
        writer.Key("channel");
        writer.Int(ch);
        writer.EndObject();
        return 0;
    }

    int GetDeviceState(Writer& writer) {
        std::lock_guard<std::mutex> lock(mtx);
        if(!cur_dev) {
            writer.Key("error");
            writer.String("Error opening the device");
            return 6;
        }
        int rc = 0;
        writer.Key("id");
        writer.String(cur_dev->name);
        auto state = cur_dev->GetState(&rc);
        if(rc != 0) {
            writer.Key("error");
            writer.String("Error reading device state");
            return rc;
        }

        writer.Key("data");
        writer.StartArray();
        for(auto val: state) writer.Bool(val);
        writer.EndArray();
        return 0;
    }

    int GetDevice(const rapidjson::Value& request, Writer& writer) {
        if(!request.HasMember("id")) return 4;
        const rapidjson::Value& id = request["id"];
        if(!id.IsString()) return 5;
        {
            std::lock_guard<std::mutex> lock(mtx);
            cur_dev = monitor.GetDevice(id.GetString());
        }
        return GetDeviceState(writer);
    }
    int ListDevices(const rapidjson::Value& request, Writer& writer) {
        std::vector<std::string> data;
        monitor.GetList(data);
        writer.Key("data");
        writer.StartArray();
        for(auto& id: data) { writer.String(id); }
        writer.EndArray();
        return 0;
    }
};
class CmdServer : public mplc::TCPServer<RemoteConnect> {
public:
    void OnConnected(mplc::socket_t connect, sockaddr_in nsi) override {
        printf("new user: %s\n", inet_ntoa(nsi.sin_addr));
        pool.Add(connect, nsi);
    }
    /*CmdServer(uint16_t port, const char* ip = nullptr): TCPServer(port, ip) {
        std::cout << "Server started: " << (ip ? ip : "0.0.0.0") << ':' << port << std::endl;
    }*/
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
    // std::thread th(test);
    CmdServer server;
    int rc = server.Bind(4444);
    if(rc != 0) {
        std::cout << "Can't bind socket to 0.0.0.0" << ':' << 4444 << std::endl
        << "Error code: " << rc << std::endl;
        return 1;
    }
    std::cout << "Server started: 0.0.0.0" << ':' << 4444 << std::endl;
    server.Start();
    WSACleanup();
    return 0;
}

// class RemoteCmd : public  mplc::WSConnect {
//    RemoteCmd(){}
//    void SendText(const std::string& data, bool fin) override;
//};
