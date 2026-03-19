#include "nxlib_context.hpp"

namespace viam {
namespace camera {
namespace ensenso {

// Static member definitions
std::shared_ptr<NxLibContext> NxLibContext::instance_ = nullptr;
std::mutex NxLibContext::mutex_;

}  // namespace ensenso
}  // namespace camera
}  // namespace viam
