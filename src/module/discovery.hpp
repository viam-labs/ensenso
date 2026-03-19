#pragma once

#include <memory>
#include <string>
#include <vector>

#include <viam/sdk/services/discovery.hpp>
#include <viam/sdk/resource/resource.hpp>
#include <viam/sdk/config/resource.hpp>

#include "nxLib.h"
#include "nxlib_context.hpp"

using viam::sdk::Discovery;
using viam::sdk::ResourceConfig;

namespace viam {
namespace camera {
namespace ensenso {
namespace discovery {

/**
 * @brief Discovery service for Ensenso cameras
 *
 * Automatically detects connected Ensenso cameras and provides
 * configuration suggestions for the Viam robot.
 */
class EnsensoDiscovery : public Discovery {
public:
    static inline viam::sdk::Model model{"viam", "ensenso", "discovery"};

    EnsensoDiscovery(viam::sdk::Dependencies dependencies, ResourceConfig configuration)
        : Discovery(configuration.name()) {
        // Get shared nxLib context (initializes nxLib if needed)
        try {
            nxlib_context_ = NxLibContext::get_instance(true);
        } catch (const std::exception& ex) {
            VIAM_SDK_LOG(error) << "Failed to initialize nxLib: " << ex.what();
            throw;
        }
    }

    /**
     * @brief Discover connected Ensenso cameras
     *
     * @param extra Additional parameters
     * @return std::vector<ResourceConfig> List of discovered camera configurations
     */
    std::vector<viam::sdk::ResourceConfig> discover_resources(const viam::sdk::ProtoStruct& extra) override {
        std::vector<viam::sdk::ResourceConfig> configs;

        try {
            // nxLib is already initialized via shared context
            if (!nxlib_context_ || !nxlib_context_->is_initialized()) {
                VIAM_SDK_LOG(error) << "[discover_resources] nxLib not initialized";
                return configs;
            }

            VIAM_SDK_LOG(info) << "[discover_resources] Starting Ensenso camera discovery";

            // Get list of cameras
            NxLibItem root;
            NxLibItem cameras = root[itmCameras][itmBySerialNo];

            int camera_count = cameras.count();
            VIAM_SDK_LOG(info) << "[discover_resources] Found " << camera_count << " Ensenso camera(s)";

            // Iterate through each camera
            for (int i = 0; i < camera_count; i++) {
                try {
                    NxLibItem camera_item = cameras[i];
                    std::string serial = camera_item.name();
                    NxLibItem camera = camera_item;

                    // Skip non-camera items (like ByEepromId, BySerialNo nodes)
                    if (serial == "ByEepromId" || serial == "BySerialNo") {
                        continue;
                    }

                    // Get camera type
                    std::string camera_type = "Unknown";
                    if (camera[itmType].exists()) {
                        camera_type = camera[itmType].asString();
                    }

                    // Get model name
                    std::string model_name = "Ensenso";
                    if (camera[itmModelName].exists()) {
                        model_name = camera[itmModelName].asString();
                    }

                    // Set attributes with camera-specific configuration
                    viam::sdk::ProtoStruct attributes;
                    attributes["serial_number"] = serial;
                    attributes["width_px"] = 1280.0;  // Default resolution
                    attributes["height_px"] = 1024.0;
                    attributes["enable_depth"] = true;
                    attributes["enable_point_cloud"] = true;

                    // Add optional camera info
                    if (camera[itmVersion].exists()) {
                        attributes["firmware_version"] = camera[itmVersion].asString();
                    }
                    if (camera[itmInterface].exists()) {
                        attributes["interface"] = camera[itmInterface].asString();
                    }
                    attributes["camera_type"] = camera_type;
                    attributes["model_name"] = model_name;

                    // Create a descriptive name
                    std::string config_name = "ensenso-" + serial;

                    // Create ResourceConfig
                    viam::sdk::ResourceConfig config(
                        "camera",                       // API type
                        config_name,                    // Name
                        "viam",                         // Namespace
                        attributes,                     // Attributes
                        "rdk:component:camera",         // API string
                        viam::sdk::Model{"viam", "camera", "ensenso"},  // Model
                        viam::sdk::LinkConfig{},        // Link config
                        viam::sdk::log_level::info      // Log level
                    );

                    configs.push_back(config);

                    VIAM_SDK_LOG(info) << "[discover_resources] Discovered: " << model_name
                                      << " (S/N: " << serial << ", Type: " << camera_type << ")";

                } catch (const NxLibException& ex) {
                    VIAM_SDK_LOG(warn) << "[discover_resources] Error processing camera: "
                                       << ex.getErrorText();
                    continue;
                }
            }

            VIAM_SDK_LOG(info) << "[discover_resources] Discovery complete. Found "
                              << configs.size() << " camera(s)";

        } catch (const NxLibException& ex) {
            VIAM_SDK_LOG(error) << "[discover_resources] NxLib error during discovery: "
                               << ex.getErrorText();
        } catch (const std::exception& ex) {
            VIAM_SDK_LOG(error) << "[discover_resources] Error during discovery: " << ex.what();
        }

        return configs;
    }

    /**
     * @brief Execute custom commands
     */
    viam::sdk::ProtoStruct do_command(const viam::sdk::ProtoStruct& command) override {
        viam::sdk::ProtoStruct result;
        result["status"] = "ok";
        return result;
    }

    ~EnsensoDiscovery() override {
        // nxLib context will be finalized when last reference is released
    }

private:
    std::shared_ptr<NxLibContext> nxlib_context_;
};

}  // namespace discovery
}  // namespace ensenso
}  // namespace camera
}  // namespace viam
