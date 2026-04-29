#pragma once

#include <cstddef>
#include <new>

namespace orderbook::detail {

// Portable cache-line size constants.
//
// std::hardware_destructive_interference_size and
// std::hardware_constructive_interference_size are part of C++17 <new>, but
// GCC 12+ emits -Winterference-size when they are referenced directly (the
// values are tied to -march and can differ across TUs).  All TUs in this
// project share the same -march=native flag, so the values are consistent; we
// still centralise the suppression here so it happens in exactly one place.
//
// On compilers that do not define __cpp_lib_hardware_interference_size the
// constants fall back to 64, which is the de-facto cache-line size on x86-64
// and most AArch64 implementations.  Other architectures may use 32 or 128
// bytes, but 64 is a safe and conservative default for the platforms this
// project targets.

#ifdef __cpp_lib_hardware_interference_size
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winterference-size"
#endif
inline constexpr std::size_t destructive_interference_size =
    std::hardware_destructive_interference_size;
inline constexpr std::size_t constructive_interference_size =
    std::hardware_constructive_interference_size;
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#else
inline constexpr std::size_t destructive_interference_size = 64;
inline constexpr std::size_t constructive_interference_size = 64;
#endif

}  // namespace orderbook::detail
