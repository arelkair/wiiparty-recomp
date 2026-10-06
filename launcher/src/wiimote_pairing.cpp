// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wiimote_pairing.h"

#ifdef _WIN32

#include <windows.h>
#include <initguid.h>
#include <bthdef.h>
#include <bluetoothapis.h>

#include <cwchar>

extern "C" DWORD WINAPI BluetoothAuthenticateDevice(HWND parent, HANDLE radio, BLUETOOTH_DEVICE_INFO* device, PWSTR pass_key, ULONG length);

namespace {

constexpr DWORD kServiceEnable = 0x01;
constexpr int kRounds = 3;
constexpr UCHAR kInquiryLength = 2;

bool is_wiimote(const BLUETOOTH_DEVICE_INFO& device) {
    return std::wcsncmp(device.szName, L"Nintendo RVL-CNT-01", 19) == 0;
}

template <typename Visit>
void for_each_wiimote(bool inquiry, Visit visit) {
    BLUETOOTH_FIND_RADIO_PARAMS radio_params{};
    radio_params.dwSize = sizeof(radio_params);
    HANDLE radio = nullptr;
    HBLUETOOTH_RADIO_FIND radios = BluetoothFindFirstRadio(&radio_params, &radio);
    if (!radios) {
        return;
    }
    do {
        BLUETOOTH_RADIO_INFO radio_info{};
        radio_info.dwSize = sizeof(radio_info);
        if (BluetoothGetRadioInfo(radio, &radio_info) == ERROR_SUCCESS) {
            BLUETOOTH_DEVICE_SEARCH_PARAMS search{};
            search.dwSize = sizeof(search);
            search.fReturnAuthenticated = TRUE;
            search.fReturnRemembered = TRUE;
            search.fReturnUnknown = TRUE;
            search.fReturnConnected = TRUE;
            search.fIssueInquiry = inquiry;
            search.cTimeoutMultiplier = inquiry ? kInquiryLength : 0;
            search.hRadio = radio;
            BLUETOOTH_DEVICE_INFO device{};
            device.dwSize = sizeof(device);
            HBLUETOOTH_DEVICE_FIND devices = BluetoothFindFirstDevice(&search, &device);
            if (devices) {
                do {
                    if (is_wiimote(device)) {
                        visit(radio, radio_info, device);
                    }
                    device.dwSize = sizeof(device);
                } while (BluetoothFindNextDevice(devices, &device));
                BluetoothFindDeviceClose(devices);
            }
        }
        CloseHandle(radio);
    } while (BluetoothFindNextRadio(radios, &radio));
    BluetoothFindRadioClose(radios);
}

bool authenticate(HANDLE radio, const BLUETOOTH_RADIO_INFO& radio_info, BLUETOOTH_DEVICE_INFO& device) {
    WCHAR pass_key[6];
    for (int i = 0; i < 6; i++) {
        pass_key[i] = radio_info.address.rgBytes[i];
    }
    if (BluetoothAuthenticateDevice(nullptr, radio, &device, pass_key, 6) != ERROR_SUCCESS) {
        return false;
    }
    DWORD services = 0;
    DWORD result = BluetoothEnumerateInstalledServices(radio, &device, &services, nullptr);
    return result == ERROR_SUCCESS || result == ERROR_MORE_DATA;
}

}

bool wiimote_pairing_supported() {
    BLUETOOTH_FIND_RADIO_PARAMS radio_params{};
    radio_params.dwSize = sizeof(radio_params);
    HANDLE radio = nullptr;
    HBLUETOOTH_RADIO_FIND radios = BluetoothFindFirstRadio(&radio_params, &radio);
    if (!radios) {
        return false;
    }
    CloseHandle(radio);
    BluetoothFindRadioClose(radios);
    return true;
}

int pair_wiimotes() {
    for_each_wiimote(false, [](HANDLE, const BLUETOOTH_RADIO_INFO&, BLUETOOTH_DEVICE_INFO& device) {
        if (device.fRemembered && !device.fConnected && !device.fAuthenticated) {
            BluetoothRemoveDevice(&device.Address);
        }
    });
    int paired = 0;
    for (int round = 0; round < kRounds; round++) {
        for_each_wiimote(true, [&](HANDLE radio, const BLUETOOTH_RADIO_INFO& radio_info, BLUETOOTH_DEVICE_INFO& device) {
            if (device.fConnected) {
                return;
            }
            if (!device.fAuthenticated) {
                authenticate(radio, radio_info, device);
            }
            GUID service = HumanInterfaceDeviceServiceClass_UUID;
            if (BluetoothSetServiceState(radio, &device, &service, kServiceEnable) == ERROR_SUCCESS) {
                paired++;
            }
        });
    }
    return paired;
}

bool wiimote_access_supported() {
    return false;
}

bool grant_wiimote_access() {
    return false;
}

#else

#include <set>
#include <sstream>
#include <string>

#include "subprocess.h"

namespace {

constexpr const char* kRule =
    "KERNEL==\"hidraw*\", KERNELS==\"0005:057E:0306.*\", MODE=\"0666\"\n"
    "KERNEL==\"hidraw*\", KERNELS==\"0005:057E:0330.*\", MODE=\"0666\"\n";
constexpr const char* kRuleFile = "/etc/udev/rules.d/60-wiipartyrecomp-wiimote.rules";

std::string bluetoothctl(const std::vector<std::string>& arguments) {
    std::string program = find_program("bluetoothctl");
    return program.empty() ? std::string() : capture(program, arguments);
}

std::set<std::string> wiimote_addresses(const std::string& listing) {
    std::set<std::string> found;
    std::istringstream lines(listing);
    for (std::string line; std::getline(lines, line);) {
        size_t device = line.find("Device ");
        if (device == std::string::npos || line.find("Nintendo RVL-CNT-01") == std::string::npos) {
            continue;
        }
        std::string address = line.substr(device + 7, 17);
        if (address.size() == 17) {
            found.insert(address);
        }
    }
    return found;
}

}

bool wiimote_pairing_supported() {
    return !find_program("bluetoothctl").empty();
}

int pair_wiimotes() {
    std::string scan = bluetoothctl({"--timeout", "10", "scan", "on"});
    std::set<std::string> addresses = wiimote_addresses(scan + "\n" + bluetoothctl({"devices"}));
    int paired = 0;
    for (const std::string& address : addresses) {
        bluetoothctl({"pair", address});
        bluetoothctl({"trust", address});
        if (bluetoothctl({"connect", address}).find("Connection successful") != std::string::npos) {
            paired++;
        }
    }
    return paired;
}

bool wiimote_access_supported() {
    return !find_program("pkexec").empty();
}

bool grant_wiimote_access() {
    std::string pkexec = find_program("pkexec");
    if (pkexec.empty()) {
        return false;
    }
    std::string script = std::string("printf '%s' '") + kRule + "' > " + kRuleFile + " && udevadm control --reload-rules && udevadm trigger --subsystem-match=hidraw";
    Process process;
    std::string error;
    if (!process.start(pkexec, {"/bin/sh", "-c", script}, {}, false, error)) {
        return false;
    }
    char buffer[256];
    while (process.read(buffer, sizeof buffer) > 0) {
    }
    return process.wait() == 0;
}

#endif
