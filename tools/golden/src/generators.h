#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace golden {

using Generator = std::function<void(const std::filesystem::path& outDir)>;

struct Registration
{
    const char* module;
    Generator generate;
};

std::vector<Registration>& registry();

struct Register
{
    Register(const char* module, Generator generate) { registry().push_back({module, std::move(generate)}); }
};

}
