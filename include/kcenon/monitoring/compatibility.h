#ifndef MONITORING_SYSTEM_COMPATIBILITY_H
#define MONITORING_SYSTEM_COMPATIBILITY_H

// Forward declare the canonical namespace
namespace kcenon { namespace monitoring {} }

// Backward compatibility namespace alias
// Legacy code can use monitoring_system:: which maps to kcenon::monitoring::
// This alias will be deprecated in future versions
namespace monitoring_system = ::kcenon::monitoring;

// Additional legacy alias
namespace monitoring_module = ::kcenon::monitoring;

#endif // MONITORING_SYSTEM_COMPATIBILITY_H
