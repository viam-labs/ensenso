#include "nxlib_context.hpp"

namespace viam {
namespace camera {
namespace ensenso {

// Static member definitions
std::weak_ptr<NxLibContext> NxLibContext::instance_;
std::mutex NxLibContext::mutex_;

}  // namespace ensenso
}  // namespace camera
}  // namespace viam
