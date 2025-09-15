#ifndef KCENON_MONITORING_COMPATIBILITY_H
#define KCENON_MONITORING_COMPATIBILITY_H

// Backward compatibility namespace aliases
// These will be removed in future versions

namespace kcenon::monitoring {
    namespace core {}
    namespace interfaces {}
    namespace collectors {}
    namespace exporters {}
    namespace impl {}
    namespace utils {}
}

// Old namespace alias (deprecated)
namespace kcenon::monitoring = kcenon::monitoring;

#endif // KCENON_MONITORING_COMPATIBILITY_H
