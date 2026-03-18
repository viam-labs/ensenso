#pragma once

#include <memory>
#include <string>
#include <vector>

#include <viam/sdk/components/camera.hpp>
#include <viam/sdk/config/resource.hpp>
#include <viam/sdk/resource/resource.hpp>
#include <viam/sdk/spatialmath/geometry.hpp>

#include "nxLib.h"

// Use Viam SDK namespace types
using viam::sdk::Camera;
using viam::sdk::ProtoStruct;
using viam::sdk::GeometryConfig;

namespace viam {
namespace camera {
namespace ensenso {

/**
 * @brief Viam Camera component implementation for IDS Ensenso cameras
 *
 * This class implements the Viam Camera API using the Ensenso nxLib SDK.
 * It provides access to stereo images, depth maps, and point clouds from
 * Ensenso 3D cameras.
 */
class EnsensoCamera : public Camera {
public:
    /**
     * @brief Construct a new Ensenso Camera object
     *
     * @param name Component name from Viam config
     * @param attrs Configuration attributes
     */
    EnsensoCamera(const std::string& name, const ProtoStruct& attrs);

    /**
     * @brief Destroy the Ensenso Camera object
     */
    ~EnsensoCamera() override;

    // Viam Camera API implementation

    /**
     * @brief Get multiple images from different sources
     *
     * @param filter_source_names Names of sources to receive images from
     * @param extra Additional parameters
     * @return image_collection Collection of named images (color, depth, etc.)
     */
    Camera::image_collection get_images(std::vector<std::string> filter_source_names,
                                        const ProtoStruct& extra) override;

    /**
     * @brief Get point cloud data
     *
     * @param mime_type Requested encoding (e.g., "pointcloud/pcd")
     * @param extra Additional parameters
     * @return point_cloud Point cloud data
     */
    Camera::point_cloud get_point_cloud(std::string mime_type, const ProtoStruct& extra) override;

    /**
     * @brief Get camera properties (intrinsics)
     *
     * @return properties Camera properties including resolution and intrinsic matrix
     */
    Camera::properties get_properties() override;

    /**
     * @brief Get geometries associated with the camera
     *
     * @param extra Additional parameters
     * @return Vector of GeometryConfig
     */
    std::vector<GeometryConfig> get_geometries(const ProtoStruct& extra) override;

    /**
     * @brief Reconfigure the camera with new attributes
     *
     * @param attrs New configuration attributes
     */
    void reconfigure(const ProtoStruct& attrs);

    /**
     * @brief Execute custom commands
     *
     * @param command Command map
     * @return ProtoStruct Response data
     */
    ProtoStruct do_command(const ProtoStruct& command) override;

private:
    // Configuration attributes
    std::string serial_number_;
    int width_px_;
    int height_px_;
    bool enable_depth_;
    bool enable_point_cloud_;

    // nxLib objects
    NxLibItem camera_node_;
    bool camera_open_;
    bool owns_nxlib_;  // Track if this instance initialized nxLib

    // Private methods
    void open_camera();
    void close_camera();
    void parse_attributes(const ProtoStruct& attrs);
    void capture_images();
    Camera::raw_image get_color_image(const std::string& mime_type);
    Camera::raw_image get_depth_image(const std::string& mime_type);
    void compute_point_cloud();
};

/**
 * @brief Factory function to create EnsensoCamera instances
 */
std::shared_ptr<Camera> create_ensenso_camera(
    const std::string& name,
    const ProtoStruct& attrs
);

}  // namespace ensenso
}  // namespace camera
}  // namespace viam
