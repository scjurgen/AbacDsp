#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#ifdef __APPLE__
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include <intrin.h>
#include <windows.h>
#endif

namespace AbacDsp::Perf
{

/// @brief CPU model/clock/memory as reported by the OS, for the report header.
struct ProcessorInfo
{
    std::string model{"Unknown"};
    double frequencyGHz{0.0};
    size_t memoryMB{0};
};

[[nodiscard]] inline ProcessorInfo getProcessorInfo()
{
    ProcessorInfo info{};

#ifdef __APPLE__
    std::array<char, 1024> buffer{};
    size_t size = buffer.size();
    if (sysctlbyname("machdep.cpu.brand_string", buffer.data(), &size, nullptr, 0) == 0)
    {
        info.model = buffer.data();
    }

    uint64_t freq = 0;
    size = sizeof(freq);
    if (sysctlbyname("hw.cpufrequency", &freq, &size, nullptr, 0) == 0)
    {
        info.frequencyGHz = static_cast<double>(freq) / 1e9;
    }

    int64_t memSize = 0;
    size = sizeof(memSize);
    if (sysctlbyname("hw.memsize", &memSize, &size, nullptr, 0) == 0)
    {
        info.memoryMB = static_cast<size_t>(memSize / (1024 * 1024));
    }
#elif defined(_WIN32)
    std::array<int, 4> cpuInfo{};
    std::array<char, 0x40> brand{};
    __cpuid(cpuInfo.data(), 0x80000002);
    std::memcpy(brand.data(), cpuInfo.data(), sizeof(cpuInfo));
    __cpuid(cpuInfo.data(), 0x80000003);
    std::memcpy(brand.data() + 16, cpuInfo.data(), sizeof(cpuInfo));
    __cpuid(cpuInfo.data(), 0x80000004);
    std::memcpy(brand.data() + 32, cpuInfo.data(), sizeof(cpuInfo));
    info.model = brand.data();

    MEMORYSTATUSEX memInfo{};
    memInfo.dwLength = sizeof(memInfo);
    GlobalMemoryStatusEx(&memInfo);
    info.memoryMB = static_cast<size_t>(memInfo.ullTotalPhys / (1024 * 1024));
#endif

    return info;
}

/// @brief Current git branch and short commit hash, for correlating a report with the code it measured.
struct GitInfo
{
    std::string branch;
    std::string commit;
};

[[nodiscard]] inline std::string runGitCommand(const std::string& command)
{
    const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe)
    {
        return "";
    }
    std::array<char, 256> buffer{};
    std::string result;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr)
    {
        result += buffer.data();
    }
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
    {
        result.pop_back();
    }
    return result;
}

[[nodiscard]] inline GitInfo getGitInfo()
{
    return {runGitCommand("git rev-parse --abbrev-ref HEAD 2>/dev/null"),
            runGitCommand("git rev-parse --short HEAD 2>/dev/null")};
}

}
