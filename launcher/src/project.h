#pragma once

#include <filesystem>
#include <string>

struct Project {
    std::filesystem::path root;
    bool packaged = false;

    std::filesystem::path game_folder() const;
    std::filesystem::path extracted_folder() const;
    std::filesystem::path nand_folder() const;
    std::filesystem::path backups_folder() const;
    std::filesystem::path executable() const;
    std::string settings_file() const;
    bool extracted() const;
    bool built() const;
    bool outdated() const;
    double needed_gigabytes() const;
    double free_gigabytes() const;
};

constexpr const char* kGame = "wiiparty";
constexpr const char* kInstallFolderName = "WiiPartyRecomp";

bool find_checkout(std::filesystem::path& root);
std::filesystem::path default_install_folder();
