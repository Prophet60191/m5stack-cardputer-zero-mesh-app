/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#pragma once

namespace app {

// CMake project version is also used by the Debian package.
#ifndef APP_VERSION
#define APP_VERSION "0.1.1"
#endif
inline constexpr const char* kAppVersion = "v" APP_VERSION;
inline constexpr const char* kRepoUrl = "https://github.com/Prophet60191";

}  // namespace app
