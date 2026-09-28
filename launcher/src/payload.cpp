#include "payload.h"

#include <fstream>

#include "embedded.h"
#include "subprocess.h"
#include "texts.h"

namespace {

constexpr const char* kRevisionFile = ".launcher-revision";
constexpr const char* kArchiveFile = ".launcher-payload.tar.gz";

std::string tar_program() {
#ifdef _WIN32
    std::string system = environment_value("SystemRoot");
    return utf8_of(path_from(system.empty() ? "C:/Windows" : system) / "System32" / "tar.exe");
#else
    return find_program("tar");
#endif
}

}

bool payload_available() {
    return !blob("payload").empty();
}

std::string stored_revision(const std::filesystem::path& root) {
    std::ifstream file(root / kRevisionFile, std::ios::binary);
    std::string revision;
    std::getline(file, revision);
    return revision;
}

bool extract_payload(const std::filesystem::path& root, std::string& message) {
    if (stored_revision(root) == source_revision()) {
        return true;
    }
    std::string_view data = blob("payload");
    std::filesystem::path archive = root / kArchiveFile;
    std::error_code error;
    std::filesystem::create_directories(root, error);
    {
        std::ofstream file(archive, std::ios::binary | std::ios::trunc);
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!file) {
            message = format(texts().extract_failed, utf8_of(root));
            return false;
        }
    }
    Process tar;
    std::string failure;
    bool started = tar.start(tar_program(), {"-xzf", kArchiveFile}, root, false, failure);
    char buffer[1024];
    while (started && tar.read(buffer, sizeof(buffer))) {
    }
    bool extracted = started && tar.wait() == 0;
    std::filesystem::remove(archive, error);
    if (!extracted) {
        message = format(texts().extract_failed, utf8_of(root)) + (failure.empty() ? "" : " " + failure);
        return false;
    }
    std::ofstream revision(root / kRevisionFile, std::ios::binary | std::ios::trunc);
    revision << source_revision() << '\n';
    message = format(texts().extracted_files, utf8_of(root));
    return true;
}
