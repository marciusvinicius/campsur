#pragma once

// Stub logging: no dependency on raylib. Redefine ENGINE_LOG as needed (e.g. fprintf, spdlog).
enum EngineLogLevel { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR };
#define ENGINE_LOG(level, ...) ((void)(level))
