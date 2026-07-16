#pragma once

#include "breeze_api.h"
#include <cstdio>
#include <cstdarg>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace breeze {

class Logger {
public:
    static void Init(LogLevel level, bool to_android, const std::string& tag) {
        level_      = level;
        to_android_ = to_android;
        tag_        = tag;
    }

    static void SetLevel(LogLevel level) { level_ = level; }
    static LogLevel GetLevel() { return level_; }

    static void Verbose(const char* fmt, ...) __attribute__((format(printf, 1, 2))) {
        if (level_ > LogLevel::Verbose) return;
        va_list ap; va_start(ap, fmt);
        Log(LogLevel::Verbose, fmt, ap);
        va_end(ap);
    }

    static void Debug(const char* fmt, ...) __attribute__((format(printf, 1, 2))) {
        if (level_ > LogLevel::Debug) return;
        va_list ap; va_start(ap, fmt);
        Log(LogLevel::Debug, fmt, ap);
        va_end(ap);
    }

    static void Info(const char* fmt, ...) __attribute__((format(printf, 1, 2))) {
        if (level_ > LogLevel::Info) return;
        va_list ap; va_start(ap, fmt);
        Log(LogLevel::Info, fmt, ap);
        va_end(ap);
    }

    static void Warn(const char* fmt, ...) __attribute__((format(printf, 1, 2))) {
        if (level_ > LogLevel::Warn) return;
        va_list ap; va_start(ap, fmt);
        Log(LogLevel::Warn, fmt, ap);
        va_end(ap);
    }

    static void Error(const char* fmt, ...) __attribute__((format(printf, 1, 2))) {
        if (level_ > LogLevel::Error) return;
        va_list ap; va_start(ap, fmt);
        Log(LogLevel::Error, fmt, ap);
        va_end(ap);
    }

private:
    static void Log(LogLevel level, const char* fmt, va_list ap) {
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, ap);

#ifdef __ANDROID__
        if (to_android_) {
            static int android_levels[] = {
                ANDROID_LOG_VERBOSE, ANDROID_LOG_DEBUG, ANDROID_LOG_INFO,
                ANDROID_LOG_WARN,    ANDROID_LOG_ERROR, ANDROID_LOG_SILENT
            };
            __android_log_print(android_levels[static_cast<int>(level)],
                                tag_.c_str(), "%s", buf);
            return;
        }
#endif
        static const char* level_names[] = {
            "V", "D", "I", "W", "E", "S"
        };
        std::printf("[%s][%s] %s\n", tag_.c_str(), level_names[static_cast<int>(level)], buf);
    }

    static inline LogLevel   level_      = LogLevel::Info;
    static inline bool       to_android_ = true;
    static inline std::string tag_       = "BreezeAPI";
};

} // namespace breeze
