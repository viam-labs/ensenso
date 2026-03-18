#include "ensenso_camera.hpp"

#include <cmath>
#include <stdexcept>
#include <sstream>
#include <vector>

#include <viam/sdk/common/exception.hpp>
#include <viam/sdk/common/utils.hpp>
#include <viam/sdk/common/proto_value.hpp>

using viam::sdk::Exception;
using viam::sdk::ProtoValue;

namespace viam {
namespace camera {
namespace ensenso {

// Helper function to convert nxLib exceptions to Viam exceptions
static void check_nxlib_error(const NxLibException& ex) {
    std::ostringstream oss;
    oss << "nxLib error " << ex.getErrorCode() << ": " << ex.getErrorText();
    throw Exception("nxLib error: " + oss.str());
}

EnsensoCamera::EnsensoCamera(const std::string& name, const ProtoStruct& attrs)
    : Camera(name),
      width_px_(1280),
      height_px_(1024),
      enable_depth_(true),
      enable_point_cloud_(true),
      camera_open_(false),
      owns_nxlib_(false) {

    parse_attributes(attrs);

    // Initialize nxLib (will do nothing if already initialized)
    // Note: nxLib uses reference counting internally
    try {
        nxLibInitialize(true);  // Wait for initial camera enumeration
        owns_nxlib_ = true;
    } catch (const NxLibException& ex) {
        check_nxlib_error(ex);
    }

    open_camera();
}

EnsensoCamera::~EnsensoCamera() {
    try {
        close_camera();

        // Finalize nxLib if we initialized it
        if (owns_nxlib_) {
            nxLibFinalize();
            owns_nxlib_ = false;
        }
    } catch (...) {
        // Suppress exceptions in destructor
    }
}

void EnsensoCamera::parse_attributes(const ProtoStruct& attrs) {
    // Parse serial number (optional - if not provided, use first available camera)
    if (attrs.count("serial_number")) {
        serial_number_ = attrs.at("serial_number").get_unchecked<std::string>();
    }

    // Parse resolution
    if (attrs.count("width_px")) {
        width_px_ = static_cast<int>(attrs.at("width_px").get_unchecked<double>());
    }
    if (attrs.count("height_px")) {
        height_px_ = static_cast<int>(attrs.at("height_px").get_unchecked<double>());
    }

    // Parse feature flags
    if (attrs.count("enable_depth")) {
        enable_depth_ = attrs.at("enable_depth").get_unchecked<bool>();
    }
    if (attrs.count("enable_point_cloud")) {
        enable_point_cloud_ = attrs.at("enable_point_cloud").get_unchecked<bool>();
    }
}

void EnsensoCamera::open_camera() {
    try {
        // Access the cameras tree
        NxLibItem cameras = NxLibItem()[itmCameras][itmBySerialNo];

        // If serial number specified, open specific camera
        if (!serial_number_.empty()) {
            if (!cameras[serial_number_].exists()) {
                throw Exception("Camera with serial number '" + serial_number_ + "' not found");
            }
            camera_node_ = cameras[serial_number_];
        } else {
            // Open first available camera
            NxLibItem root;
            NxLibItem cameraList = root[itmCameras][itmBySerialNo];

            // Get list of available cameras
            std::vector<std::string> serials;
            for (int i = 0; i < cameraList.count(); i++) {
                serials.push_back(cameraList[i].name());
            }

            if (serials.empty()) {
                throw Exception("No Ensenso cameras found");
            }

            serial_number_ = serials[0];
            camera_node_ = cameras[serial_number_];
        }

        // Open the camera
        NxLibCommand open(cmdOpen);
        open.parameters()[itmCameras] = serial_number_;
        open.execute();

        camera_open_ = true;

        // Configure capture parameters for better image quality
        try {
            camera_node_[itmParameters][itmCapture][itmAutoBlackLevel] = true;
            camera_node_[itmParameters][itmCapture][itmAutoGain] = true;
            camera_node_[itmParameters][itmCapture][itmAutoExposure] = true;
        } catch (...) {
            // Some camera types might not support all parameters
        }

    } catch (const NxLibException& ex) {
        check_nxlib_error(ex);
    }
}

void EnsensoCamera::close_camera() {
    if (camera_open_) {
        try {
            NxLibCommand close(cmdClose);
            close.parameters()[itmCameras] = serial_number_;
            close.execute();
            camera_open_ = false;
        } catch (const NxLibException& ex) {
            // Log but don't throw in cleanup
        }
    }
}

void EnsensoCamera::capture_images() {
    try {
        // Capture raw images
        NxLibCommand capture(cmdCapture);
        capture.parameters()[itmCameras] = serial_number_;
        capture.execute();

        // Rectify images for better quality
        NxLibCommand rectify(cmdRectifyImages);
        rectify.parameters()[itmCameras] = serial_number_;
        rectify.execute();
    } catch (const NxLibException& ex) {
        check_nxlib_error(ex);
    }
}

Camera::image_collection EnsensoCamera::get_images(std::vector<std::string> filter_source_names,
                                                    const ProtoStruct& extra) {
    Camera::image_collection images;

    capture_images();

    bool return_all = filter_source_names.empty();

    // Add color/rectified image
    if (return_all ||  std::find(filter_source_names.begin(), filter_source_names.end(), "color") != filter_source_names.end()) {
        try {
            Camera::raw_image color_img = get_color_image("image/jpeg");
            color_img.source_name = "color";
            images.images.push_back(color_img);
        } catch (...) {
            // Skip if not available
        }
    }

    // Add depth image if enabled
    if (enable_depth_ && (return_all || std::find(filter_source_names.begin(), filter_source_names.end(), "depth") != filter_source_names.end())) {
        try {
            compute_point_cloud();  // Need to compute disparity map first
            Camera::raw_image depth_img = get_depth_image("image/png");
            depth_img.source_name = "depth";
            images.images.push_back(depth_img);
        } catch (...) {
            // Skip if not available
        }
    }

    return images;
}

Camera::raw_image EnsensoCamera::get_color_image(const std::string& mime_type) {
    try {
        // Get rectified images from the camera
        NxLibItem leftImg = camera_node_[itmImages][itmRectified][itmLeft];

        int width, height, channels, bytesPerElement;
        double timestamp;

        leftImg.getBinaryDataInfo(&width, &height, &channels,
                                  &bytesPerElement, nullptr, &timestamp);

        std::vector<unsigned char> buffer;
        leftImg.getBinaryData(buffer, nullptr);

        Camera::raw_image result;
        result.mime_type = mime_type;
        result.bytes = std::move(buffer);

        return result;

    } catch (const NxLibException& ex) {
        check_nxlib_error(ex);
        return Camera::raw_image();  // Unreachable
    }
}

Camera::raw_image EnsensoCamera::get_depth_image(const std::string& mime_type) {
    try {
        NxLibItem disparityMap = camera_node_[itmImages][itmDisparityMap];

        int width, height, channels, bytesPerElement;
        std::vector<unsigned char> buffer;

        disparityMap.getBinaryDataInfo(&width, &height, &channels,
                                       &bytesPerElement, nullptr, nullptr);
        disparityMap.getBinaryData(buffer, nullptr);

        Camera::raw_image result;
        result.mime_type = mime_type;
        result.bytes = std::move(buffer);

        return result;

    } catch (const NxLibException& ex) {
        check_nxlib_error(ex);
        return Camera::raw_image();  // Unreachable
    }
}

void EnsensoCamera::compute_point_cloud() {
    try {
        NxLibCommand computeDisparityMap(cmdComputeDisparityMap);
        computeDisparityMap.parameters()[itmCameras] = serial_number_;
        computeDisparityMap.execute();
    } catch (const NxLibException& ex) {
        check_nxlib_error(ex);
    }
}

Camera::point_cloud EnsensoCamera::get_point_cloud(std::string mime_type, const ProtoStruct& extra) {
    if (!enable_point_cloud_) {
        throw Exception("Point cloud generation is disabled");
    }

    try {
        // Capture images (without rectification for point cloud)
        NxLibCommand capture(cmdCapture);
        capture.parameters()[itmCameras] = serial_number_;
        capture.execute();

        // Compute disparity map
        compute_point_cloud();

        // Convert disparity map to 3D point cloud
        NxLibCommand computePointMap(cmdComputePointMap);
        computePointMap.parameters()[itmCameras] = serial_number_;
        computePointMap.execute();

        NxLibItem pointMap = camera_node_[itmImages][itmPointMap];

        int width, height, channels, bytesPerElement;
        std::vector<float> buffer;

        pointMap.getBinaryDataInfo(&width, &height, &channels,
                                   &bytesPerElement, nullptr, nullptr);
        pointMap.getBinaryData(buffer, nullptr);

        Camera::point_cloud result;
        result.mime_type = mime_type;

        // Convert from nxLib format (XYZ interleaved) to Viam PCD format
        // Filter out NaN values (invalid points)
        std::ostringstream pcd_data;
        int valid_points = 0;

        for (size_t i = 0; i < buffer.size(); i += 3) {
            float x = buffer[i];
            float y = buffer[i + 1];
            float z = buffer[i + 2];

            // Skip NaN points (invalid depth)
            if (std::isnan(x) || std::isnan(y) || std::isnan(z)) {
                continue;
            }

            // Convert from mm to meters
            pcd_data << (x / 1000.0) << " " << (y / 1000.0) << " " << (z / 1000.0) << "\n";
            valid_points++;
        }

        // Convert string to bytes for result.pc
        std::string pcd_str = pcd_data.str();
        result.pc = std::vector<unsigned char>(pcd_str.begin(), pcd_str.end());

        return result;

    } catch (const NxLibException& ex) {
        check_nxlib_error(ex);
        return Camera::point_cloud();  // Unreachable
    }
}

Camera::properties EnsensoCamera::get_properties() {
    Camera::properties props;
    props.supports_pcd = enable_point_cloud_;
    props.intrinsic_parameters.width_px = width_px_;
    props.intrinsic_parameters.height_px = height_px_;

    try {
        // Get camera calibration parameters
        NxLibItem calib = camera_node_[itmCalibration][itmMonocular][itmLeft];

        double fx = calib[itmCamera][0][0].asDouble();
        double fy = calib[itmCamera][1][1].asDouble();
        double cx = calib[itmCamera][0][2].asDouble();
        double cy = calib[itmCamera][1][2].asDouble();

        props.intrinsic_parameters.focal_x_px = fx;
        props.intrinsic_parameters.focal_y_px = fy;
        props.intrinsic_parameters.center_x_px = cx;
        props.intrinsic_parameters.center_y_px = cy;

    } catch (const NxLibException& ex) {
        // If calibration not available, use defaults
    }

    return props;
}

std::vector<GeometryConfig> EnsensoCamera::get_geometries(const ProtoStruct& extra) {
    // Return empty vector - no specific geometries for now
    return {};
}

void EnsensoCamera::reconfigure(const ProtoStruct& attrs) {
    try {
        close_camera();
        parse_attributes(attrs);
        open_camera();
    } catch (const std::exception& ex) {
        throw Exception(std::string("Failed to reconfigure camera: ") + ex.what());
    }
}

ProtoStruct EnsensoCamera::do_command(const ProtoStruct& command) {
    // Handle custom commands here if needed
    ProtoStruct result;
    result.emplace("status", ProtoValue("ok"));
    return result;
}

// Factory function
std::shared_ptr<Camera> create_ensenso_camera(
    const std::string& name,
    const ProtoStruct& attrs) {
    return std::make_shared<EnsensoCamera>(name, attrs);
}

}  // namespace ensenso
}  // namespace camera
}  // namespace viam
