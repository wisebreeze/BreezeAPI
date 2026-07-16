#include "logger.h"

// Logger implementation is header-only (inline static).
// This TU ensures the header is compiled and included in the shared library.
namespace breeze {
// Force template instantiation
static auto _logger_force = &Logger::Info;
} // namespace breeze
