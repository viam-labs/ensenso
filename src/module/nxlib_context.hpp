#pragma once

#include <memory>
#include <mutex>
#include "nxLib.h"

namespace viam {
namespace camera {
namespace ensenso {

/**
 * @brief Shared nxLib context manager
 *
 * Ensures nxLib is initialized only once and properly cleaned up.
 * Thread-safe singleton pattern for managing nxLib lifecycle.
 */
class NxLibContext {
public:
    /**
     * @brief Get the singleton instance
     * @param wait_for_cameras If true, waits for initial camera enumeration
     * @return Shared pointer to the context
     */
    static std::shared_ptr<NxLibContext> get_instance(bool wait_for_cameras = true) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!instance_) {
            instance_ = std::shared_ptr<NxLibContext>(new NxLibContext(wait_for_cameras));
        }

        return instance_;
    }

    /**
     * @brief Check if nxLib is initialized
     */
    bool is_initialized() const {
        return initialized_;
    }

    /**
     * @brief Destructor - finalizes nxLib
     */
    ~NxLibContext() {
        if (initialized_) {
            try {
                nxLibFinalize();
            } catch (...) {
                // Suppress exceptions in destructor
            }
        }
    }

private:
    /**
     * @brief Private constructor - use get_instance() instead
     */
    NxLibContext(bool wait_for_cameras) : initialized_(false) {
        try {
            nxLibInitialize(wait_for_cameras);
            initialized_ = true;
        } catch (const NxLibException& ex) {
            throw std::runtime_error(std::string("Failed to initialize nxLib: ") + ex.getErrorText());
        }
    }

    // Prevent copying
    NxLibContext(const NxLibContext&) = delete;
    NxLibContext& operator=(const NxLibContext&) = delete;

    bool initialized_;
    static std::shared_ptr<NxLibContext> instance_;
    static std::mutex mutex_;
};

}  // namespace ensenso
}  // namespace camera
}  // namespace viam
