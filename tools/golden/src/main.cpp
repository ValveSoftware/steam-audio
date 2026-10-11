// Writes golden .npy files produced by the reference Steam Audio implementation.
//
// Usage: golden <output dir> [module ...]
// With no modules given, every registered generator runs. Output goes to <output dir>/<module>/<case>.npy.

#include <cstdio>
#include <exception>
#include <set>

#include "generators.h"

namespace golden {

std::vector<Registration>& registry()
{
    static std::vector<Registration> generators;
    return generators;
}

}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <output dir> [module ...]\n", argv[0]);
        return 2;
    }

    const std::filesystem::path outDir = argv[1];
    const std::set<std::string> only(argv + 2, argv + argc);

    int ran = 0;
    for (const auto& reg : golden::registry())
    {
        if (!only.empty() && !only.count(reg.module))
            continue;

        std::printf("%s\n", reg.module);
        try
        {
            reg.generate(outDir / reg.module);
        }
        catch (const std::exception& e)
        {
            std::fprintf(stderr, "error in %s: %s\n", reg.module, e.what());
            return 1;
        }
        ++ran;
    }

    if (ran == 0)
    {
        std::fprintf(stderr, "no generators matched\n");
        return 1;
    }
    return 0;
}
