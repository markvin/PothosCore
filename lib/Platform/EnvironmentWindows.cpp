// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Platform/Environment.hpp"
#include <Pothos/Exception.hpp>
#include <cstdlib>
#include <format>
#include <map>
#include <thread>
#include <vector>
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

static std::wstring toWide(const std::string &utf8)
{
    if (utf8.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), out.data(), n);
    return out;
}

static std::string toUtf8(const std::wstring &wide)
{
    if (wide.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), out.data(), n, nullptr, nullptr);
    return out;
}

//environment strings are UTF-8 in Pothos: use the wide CRT functions,
//which update the CRT environment and the block that child processes inherit
std::optional<std::string> Pothos::Platform::getEnv(const std::string &name)
{
    wchar_t *value = nullptr;
    size_t len = 0;
    if (_wdupenv_s(&value, &len, toWide(name).c_str()) != 0 or value == nullptr) return std::nullopt;
    std::string result = toUtf8(value);
    std::free(value);
    return result;
}

void Pothos::Platform::setEnv(const std::string &name, const std::string &value)
{
    if (_wputenv_s(toWide(name).c_str(), toWide(value).c_str()) != 0)
        throw Pothos::SystemException("Pothos::Platform::setEnv("+name+")", "_wputenv_s failed");
}

void Pothos::Platform::unsetEnv(const std::string &name)
{
    //an empty value removes the variable
    if (_wputenv_s(toWide(name).c_str(), L"") != 0)
        throw Pothos::SystemException("Pothos::Platform::unsetEnv("+name+")", "_wputenv_s failed");
}

std::string Pothos::Platform::osName(void)
{
    return "Windows NT";
}

std::string Pothos::Platform::osVersion(void)
{
    //RtlGetVersion reports the real version, unlike GetVersionEx without a manifest
    using RtlGetVersionFcn = LONG (WINAPI *)(PRTL_OSVERSIONINFOW);
    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFcn>(
        reinterpret_cast<void *>(GetProcAddress(ntdll, "RtlGetVersion")));
    RTL_OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (rtlGetVersion == nullptr or rtlGetVersion(&info) != 0) return "unknown";
    return std::format("{}.{} (Build {})", info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber);
}

std::string Pothos::Platform::osArchitecture(void)
{
    SYSTEM_INFO info{};
    GetNativeSystemInfo(&info);
    switch (info.wProcessorArchitecture)
    {
    case PROCESSOR_ARCHITECTURE_AMD64: return "AMD64";
    case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64";
    case PROCESSOR_ARCHITECTURE_ARM: return "ARM";
    case PROCESSOR_ARCHITECTURE_IA64: return "IA64";
    case PROCESSOR_ARCHITECTURE_INTEL: return "IA32";
    default: return "unknown";
    }
}

std::string Pothos::Platform::nodeName(void)
{
    wchar_t name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (not GetComputerNameW(name, &size)) return "unknown";
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, name, int(size), nullptr, 0, nullptr, nullptr);
    std::string out(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, 0, name, int(size), out.data(), bytes, nullptr, nullptr);
    return out;
}

std::string Pothos::Platform::nodeId(void)
{
    //hardware addresses of non-loopback adapters, ordered by adapter name
    std::map<std::string, std::string> macs;
    ULONG size = 16*1024;
    std::vector<unsigned char> buff(size);
    auto *list = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buff.data());
    ULONG ret = GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, list, &size);
    if (ret == ERROR_BUFFER_OVERFLOW)
    {
        buff.resize(size);
        list = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buff.data());
        ret = GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, list, &size);
    }
    if (ret == NO_ERROR) for (auto *a = list; a != nullptr; a = a->Next)
    {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK or a->PhysicalAddressLength != 6) continue;
        const auto *addr = a->PhysicalAddress;
        bool allZero = true;
        for (size_t i = 0; i < 6; i++) if (addr[i] != 0) allZero = false;
        if (allZero) continue;
        macs.emplace(a->AdapterName, std::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]));
    }
    if (macs.empty()) return nodeName();
    return macs.begin()->second;
}

unsigned Pothos::Platform::processorCount(void)
{
    const auto n = std::thread::hardware_concurrency();
    return n == 0 ? 1 : n;
}

int Pothos::Platform::currentProcessId(void)
{
    return static_cast<int>(GetCurrentProcessId());
}
