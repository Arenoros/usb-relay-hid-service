#include "usb_relay_hw.h"
#include "usb_relay_device.h"
#include <string>
#include <vector>

#pragma comment(lib, "usb_relay_device.lib")

struct UsbDevice {
    std::string serial_number;
    std::string path;
    intptr_t type;
    UsbDevice(pusb_relay_device_info_t dev) {
        serial_number = dev->serial_number;
        path = dev->device_path;
        type = dev->type;
    }
    UsbDevice& operator=(pusb_relay_device_info_t dev) {
        serial_number = dev->serial_number;
        path = dev->device_path;
        type = dev->type;
        return *this;
    }
};

class UsbDevices {
    pusb_relay_device_info_t g_enumlist;
    std::vector<UsbDevice> list;
    void scan() {
        if(g_enumlist) {
            usb_relay_device_free_enumerate(g_enumlist);
            g_enumlist = nullptr;
        }

        g_enumlist = usb_relay_device_enumerate();
        pusb_relay_device_info_t q = g_enumlist;
        while(q) {
            list.push_back(q);
            q = q->next;
        }
    }
    void open_device(int pos) {
        UsbDevice& dev = list[pos];
        intptr_t dh = usb_relay_device_open_with_serial_number(dev.serial_number.c_str(), (int)dev.serial_number.size());
    }
};
