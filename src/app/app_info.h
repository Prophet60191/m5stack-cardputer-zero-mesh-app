/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#pragma once

namespace app {

// CMake project version is also used by the Debian package.
#ifndef APP_VERSION
#error "APP_VERSION must come from app-builder.json through the build system"
#endif
inline constexpr const char* kAppVersion = "v" APP_VERSION;
inline constexpr const char* kRepoUrl = "https://github.com/Prophet60191/m5stack-cardputer-zero-mesh-app";

}  // namespace app
