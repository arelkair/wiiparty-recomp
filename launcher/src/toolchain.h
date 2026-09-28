#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "runner.h"

struct ToolStatus {
    std::string program;
    std::string version;
    bool usable = false;
};

class Toolchain {
public:
    explicit Toolchain(std::filesystem::path folder);

    void add_to_path() const;
    std::vector<ToolStatus> inspect() const;
    std::vector<Step> preparation(const std::vector<ToolStatus>& tools) const;
    std::string python() const;

private:
    std::filesystem::path folder_;
};
