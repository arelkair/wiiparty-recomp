#pragma once

#include <array>
#include <cstddef>

class PointerWrap {
public:
    template <typename T>
    void Do(T&) {}

    template <typename T>
    void DoArray(T*, size_t) {}

    template <typename T, size_t N>
    void DoArray(T (&)[N]) {}

    template <typename T, size_t N>
    void DoArray(std::array<T, N>&) {}

    bool IsReadMode() const { return false; }
};
