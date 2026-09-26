#pragma once

#include <QString>

inline constexpr const char* kInstallFolderName = "WiiPartyRecomp";

bool payload_available();
bool extract_payload(const QString& root, QString& message);
