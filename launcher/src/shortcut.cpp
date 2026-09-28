#include "shortcut.h"

#include "subprocess.h"

#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif

bool create_desktop_shortcut(const std::filesystem::path& target, const std::filesystem::path& folder, const std::string& name, std::string& error) {
#ifdef _WIN32
    HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    PWSTR desktop = nullptr;
    IShellLinkW* link = nullptr;
    IPersistFile* file = nullptr;
    bool ok = SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktop)) &&
              SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&link))) &&
              SUCCEEDED(link->SetPath(target.c_str())) && SUCCEEDED(link->SetWorkingDirectory(folder.c_str())) &&
              SUCCEEDED(link->SetIconLocation(target.c_str(), 0)) && SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file)));
    if (ok) {
        std::filesystem::path shortcut = std::filesystem::path(desktop) / path_from(name + ".lnk");
        ok = SUCCEEDED(file->Save(shortcut.c_str(), TRUE));
    }
    if (!ok) {
        error = "Windows " + std::to_string(static_cast<unsigned long>(GetLastError()));
    }
    if (file) {
        file->Release();
    }
    if (link) {
        link->Release();
    }
    if (desktop) {
        CoTaskMemFree(desktop);
    }
    if (SUCCEEDED(initialized)) {
        CoUninitialize();
    }
    return ok;
#else
    (void)target;
    (void)folder;
    (void)name;
    error = "not supported";
    return false;
#endif
}
