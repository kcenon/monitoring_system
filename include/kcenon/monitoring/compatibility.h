#ifndef MONITORING_SYSTEM_COMPATIBILITY_H
#define MONITORING_SYSTEM_COMPATIBILITY_H

// Forward declare the legacy namespace so aliases compile without pulling
// the full monitoring headers.
namespace monitoring_system {}

// Backward compatibility namespace aliases
// These will be removed in future versions
namespace kcenon {
namespace monitoring = ::monitoring_system;
} // namespace kcenon

namespace monitoring_module = ::monitoring_system;

#endif // MONITORING_SYSTEM_COMPATIBILITY_H
