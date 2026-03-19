#include "ensenso_camera.hpp"
#include "nxlib_context.hpp"

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
      camera_open_(false) {

    VIAM_RESOURCE_LOG(info) << "[constructor] Starting Ensenso camera initialization for resource: " << name;

    parse_attributes(attrs);

    VIAM_RESOURCE_LOG(info) << "[constructor] Configuration: serial=" << serial_number_
                           << ", resolution=" << width_px_ << "x" << height_px_
                           << ", enable_depth=" << enable_depth_
                           << ", enable_point_cloud=" << enable_point_cloud_;

    // Get shared nxLib context (initializes nxLib if needed)
    try {
        VIAM_RESOURCE_LOG(debug) << "[constructor] Getting shared nxLib context";
        nxlib_context_ = NxLibContext::get_instance(true);
        VIAM_RESOURCE_LOG(info) << "[constructor] nxLib context obtained successfully";
    } catch (const std::exception& ex) {
        VIAM_RESOURCE_LOG(error) << "[constructor] Failed to initialize nxLib: " << ex.what();
        throw Exception("Failed to initialize nxLib: " + std::string(ex.what()));
    }

    open_camera();
    VIAM_RESOURCE_LOG(info) << "[constructor] Ensenso camera initialization complete";
}

EnsensoCamera::~EnsensoCamera() {
    VIAM_RESOURCE_LOG(info) << "[destructor] Starting cleanup for camera: " << serial_number_;
    try {
        close_camera();
        // nxLib context will be finalized when last reference is released
        VIAM_RESOURCE_LOG(info) << "[destructor] Cleanup complete";
    } catch (...) {
        VIAM_RESOURCE_LOG(error) << "[destructor] Exception during cleanup";
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
        VIAM_RESOURCE_LOG(info) << "[open_camera] Starting camera open procedure";

        // Access the cameras tree
        NxLibItem cameras = NxLibItem()[itmCameras][itmBySerialNo];
        int available_count = cameras.count();
        VIAM_RESOURCE_LOG(info) << "[open_camera] Found " << available_count << " cameras in nxLib tree";

        // If serial number specified, open specific camera
        if (!serial_number_.empty()) {
            VIAM_RESOURCE_LOG(info) << "[open_camera] Looking for camera with serial: " << serial_number_;
            if (!cameras[serial_number_].exists()) {
                VIAM_RESOURCE_LOG(error) << "[open_camera] Camera with serial '" << serial_number_ << "' not found";
                throw Exception("Camera with serial number '" + serial_number_ + "' not found");
            }
            camera_node_ = cameras[serial_number_];
            VIAM_RESOURCE_LOG(info) << "[open_camera] Found camera node for serial: " << serial_number_;
        } else {
            // Open first available camera
            VIAM_RESOURCE_LOG(info) << "[open_camera] No serial specified, searching for first available camera";
            NxLibItem root;
            NxLibItem cameraList = root[itmCameras][itmBySerialNo];

            // Get list of available cameras
            std::vector<std::string> serials;
            for (int i = 0; i < cameraList.count(); i++) {
                std::string s = cameraList[i].name();
                serials.push_back(s);
                VIAM_RESOURCE_LOG(debug) << "[open_camera] Available camera " << i << ": " << s;
            }

            if (serials.empty()) {
                VIAM_RESOURCE_LOG(error) << "[open_camera] No Ensenso cameras found";
                throw Exception("No Ensenso cameras found");
            }

            serial_number_ = serials[0];
            camera_node_ = cameras[serial_number_];
            VIAM_RESOURCE_LOG(info) << "[open_camera] Selected first available camera: " << serial_number_;
        }

        // Open the camera
        VIAM_RESOURCE_LOG(info) << "[open_camera] Executing cmdOpen for camera: " << serial_number_;
        NxLibCommand open(cmdOpen);
        open.parameters()[itmCameras] = serial_number_;
        open.execute();

        camera_open_ = true;
        VIAM_RESOURCE_LOG(info) << "[open_camera] Camera opened successfully: " << serial_number_;

        // Configure capture parameters for better image quality
        VIAM_RESOURCE_LOG(debug) << "[open_camera] Configuring capture parameters";
        try {
            camera_node_[itmParameters][itmCapture][itmAutoBlackLevel] = true;
            camera_node_[itmParameters][itmCapture][itmAutoGain] = true;
            camera_node_[itmParameters][itmCapture][itmAutoExposure] = true;
            VIAM_RESOURCE_LOG(info) << "[open_camera] Capture parameters configured (auto black level, gain, exposure)";
        } catch (const NxLibException& ex) {
            VIAM_RESOURCE_LOG(warn) << "[open_camera] Some capture parameters not supported: " << ex.getErrorText();
        }

        VIAM_RESOURCE_LOG(info) << "[open_camera] Camera setup complete";

    } catch (const NxLibException& ex) {
        VIAM_RESOURCE_LOG(error) << "[open_camera] nxLib exception: " << ex.getErrorText();
        check_nxlib_error(ex);
    }
}

void EnsensoCamera::close_camera() {
    if (camera_open_) {
        VIAM_RESOURCE_LOG(info) << "[close_camera] Closing camera: " << serial_number_;
        try {
            NxLibCommand close(cmdClose);
            close.parameters()[itmCameras] = serial_number_;
            close.execute();
            camera_open_ = false;
            VIAM_RESOURCE_LOG(info) << "[close_camera] Camera closed successfully";
        } catch (const NxLibException& ex) {
            VIAM_RESOURCE_LOG(error) << "[close_camera] Error closing camera: " << ex.getErrorText();
        }
    }
}

void EnsensoCamera::capture_images() {
    VIAM_RESOURCE_LOG(debug) << "[capture_images] Starting image capture for camera: " << serial_number_;
    try {
        // Capture raw images
        NxLibCommand capture(cmdCapture);
        capture.parameters()[itmCameras] = serial_number_;
        VIAM_RESOURCE_LOG(debug) << "[capture_images] Executing cmdCapture";
        capture.execute();
        VIAM_RESOURCE_LOG(debug) << "[capture_images] Capture complete";

        // Rectify images for better quality
        VIAM_RESOURCE_LOG(debug) << "[capture_images] Executing cmdRectifyImages";
        NxLibCommand rectify(cmdRectifyImages);
        rectify.parameters()[itmCameras] = serial_number_;
        rectify.execute();
        VIAM_RESOURCE_LOG(debug) << "[capture_images] Rectification complete";
    } catch (const NxLibException& ex) {
        VIAM_RESOURCE_LOG(error) << "[capture_images] nxLib exception: " << ex.getErrorText();
        check_nxlib_error(ex);
    }
}

Camera::image_collection EnsensoCamera::get_images(std::vector<std::string> filter_source_names,
                                                    const ProtoStruct& extra) {
    VIAM_RESOURCE_LOG(info) << "[get_images] Called with " << filter_source_names.size() << " filter names";
    for (const auto& name : filter_source_names) {
        VIAM_RESOURCE_LOG(debug) << "[get_images] Filter: " << name;
    }

    Camera::image_collection images;

    VIAM_RESOURCE_LOG(debug) << "[get_images] Starting image capture";
    capture_images();
    VIAM_RESOURCE_LOG(debug) << "[get_images] Image capture completed";

    bool return_all = filter_source_names.empty();
    VIAM_RESOURCE_LOG(debug) << "[get_images] return_all=" << return_all;

    // Add color/rectified image
    if (return_all ||  std::find(filter_source_names.begin(), filter_source_names.end(), "color") != filter_source_names.end()) {
        VIAM_RESOURCE_LOG(debug) << "[get_images] Attempting to get color image";
        try {
            Camera::raw_image color_img = get_color_image("image/jpeg");
            color_img.source_name = "color";
            images.images.push_back(color_img);
            VIAM_RESOURCE_LOG(info) << "[get_images] Added color image (" << color_img.bytes.size() << " bytes)";
        } catch (const std::exception& ex) {
            VIAM_RESOURCE_LOG(warn) << "[get_images] Failed to get color image: " << ex.what();
        }
    }

    // Add depth image if enabled
    if (enable_depth_ && (return_all || std::find(filter_source_names.begin(), filter_source_names.end(), "depth") != filter_source_names.end())) {
        VIAM_RESOURCE_LOG(debug) << "[get_images] Attempting to get depth image (enabled=" << enable_depth_ << ")";
        try {
            compute_point_cloud();  // Need to compute disparity map first
            Camera::raw_image depth_img = get_depth_image("image/png");
            depth_img.source_name = "depth";
            images.images.push_back(depth_img);
            VIAM_RESOURCE_LOG(info) << "[get_images] Added depth image (" << depth_img.bytes.size() << " bytes)";
        } catch (const std::exception& ex) {
            VIAM_RESOURCE_LOG(warn) << "[get_images] Failed to get depth image: " << ex.what();
        }
    }

    VIAM_RESOURCE_LOG(info) << "[get_images] Returning " << images.images.size() << " image(s)";
    return images;
}

Camera::raw_image EnsensoCamera::get_color_image(const std::string& mime_type) {
    VIAM_RESOURCE_LOG(debug) << "[get_color_image] Fetching color image (mime=" << mime_type << ")";
    try {
        // Get rectified images from the camera
        NxLibItem leftImg = camera_node_[itmImages][itmRectified][itmLeft];

        if (!leftImg.exists()) {
            VIAM_RESOURCE_LOG(error) << "[get_color_image] Left rectified image does not exist";
            throw Exception("Left rectified image not available");
        }

        int width, height, channels, bytesPerElement;
        double timestamp;

        leftImg.getBinaryDataInfo(&width, &height, &channels,
                                  &bytesPerElement, nullptr, &timestamp);

        VIAM_RESOURCE_LOG(debug) << "[get_color_image] Image info: "
                                << width << "x" << height
                                << ", channels=" << channels
                                << ", bpe=" << bytesPerElement
                                << ", timestamp=" << timestamp;

        std::vector<unsigned char> buffer;
        leftImg.getBinaryData(buffer, nullptr);

        VIAM_RESOURCE_LOG(debug) << "[get_color_image] Retrieved " << buffer.size() << " bytes";

        Camera::raw_image result;
        result.mime_type = mime_type;
        result.bytes = std::move(buffer);

        VIAM_RESOURCE_LOG(info) << "[get_color_image] Successfully created color image";
        return result;

    } catch (const NxLibException& ex) {
        VIAM_RESOURCE_LOG(error) << "[get_color_image] nxLib exception: " << ex.getErrorText();
        check_nxlib_error(ex);
        return Camera::raw_image();  // Unreachable
    }
}

Camera::raw_image EnsensoCamera::get_depth_image(const std::string& mime_type) {
    VIAM_RESOURCE_LOG(debug) << "[get_depth_image] Fetching depth image (mime=" << mime_type << ")";
    try {
        NxLibItem disparityMap = camera_node_[itmImages][itmDisparityMap];

        if (!disparityMap.exists()) {
            VIAM_RESOURCE_LOG(error) << "[get_depth_image] Disparity map does not exist";
            throw Exception("Disparity map not available");
        }

        int width, height, channels, bytesPerElement;
        std::vector<unsigned char> buffer;

        disparityMap.getBinaryDataInfo(&width, &height, &channels,
                                       &bytesPerElement, nullptr, nullptr);

        VIAM_RESOURCE_LOG(debug) << "[get_depth_image] Disparity map info: "
                                << width << "x" << height
                                << ", channels=" << channels
                                << ", bpe=" << bytesPerElement;

        disparityMap.getBinaryData(buffer, nullptr);

        VIAM_RESOURCE_LOG(debug) << "[get_depth_image] Retrieved " << buffer.size() << " bytes";

        Camera::raw_image result;
        result.mime_type = mime_type;
        result.bytes = std::move(buffer);

        VIAM_RESOURCE_LOG(info) << "[get_depth_image] Successfully created depth image";
        return result;

    } catch (const NxLibException& ex) {
        VIAM_RESOURCE_LOG(error) << "[get_depth_image] nxLib exception: " << ex.getErrorText();
        check_nxlib_error(ex);
        return Camera::raw_image();  // Unreachable
    }
}

void EnsensoCamera::compute_point_cloud() {
    VIAM_RESOURCE_LOG(debug) << "[compute_point_cloud] Computing disparity map";
    try {
        NxLibCommand computeDisparityMap(cmdComputeDisparityMap);
        computeDisparityMap.parameters()[itmCameras] = serial_number_;
        computeDisparityMap.execute();
        VIAM_RESOURCE_LOG(debug) << "[compute_point_cloud] Disparity map computed successfully";
    } catch (const NxLibException& ex) {
        VIAM_RESOURCE_LOG(error) << "[compute_point_cloud] nxLib exception: " << ex.getErrorText();
        check_nxlib_error(ex);
    }
}

Camera::point_cloud EnsensoCamera::get_point_cloud(std::string mime_type, const ProtoStruct& extra) {
    VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Called with mime_type=" << mime_type;

    if (!enable_point_cloud_) {
        VIAM_RESOURCE_LOG(warn) << "[get_point_cloud] Point cloud generation is disabled";
        throw Exception("Point cloud generation is disabled");
    }

    try {
        // Capture images (without rectification for point cloud)
        VIAM_RESOURCE_LOG(debug) << "[get_point_cloud] Capturing images for point cloud";
        NxLibCommand capture(cmdCapture);
        capture.parameters()[itmCameras] = serial_number_;
        capture.execute();
        VIAM_RESOURCE_LOG(debug) << "[get_point_cloud] Capture complete";

        // Compute disparity map
        VIAM_RESOURCE_LOG(debug) << "[get_point_cloud] Computing disparity map";
        compute_point_cloud();

        // Convert disparity map to 3D point cloud
        VIAM_RESOURCE_LOG(debug) << "[get_point_cloud] Computing point map";
        NxLibCommand computePointMap(cmdComputePointMap);
        computePointMap.parameters()[itmCameras] = serial_number_;
        computePointMap.execute();
        VIAM_RESOURCE_LOG(debug) << "[get_point_cloud] Point map computed";

        NxLibItem pointMap = camera_node_[itmImages][itmPointMap];

        if (!pointMap.exists()) {
            VIAM_RESOURCE_LOG(error) << "[get_point_cloud] Point map does not exist";
            throw Exception("Point map not available");
        }

        int width, height, channels, bytesPerElement;
        std::vector<float> buffer;

        pointMap.getBinaryDataInfo(&width, &height, &channels,
                                   &bytesPerElement, nullptr, nullptr);

        VIAM_RESOURCE_LOG(debug) << "[get_point_cloud] Point map info: "
                                << width << "x" << height
                                << ", channels=" << channels
                                << ", bpe=" << bytesPerElement;

        pointMap.getBinaryData(buffer, nullptr);
        VIAM_RESOURCE_LOG(debug) << "[get_point_cloud] Retrieved " << buffer.size() << " floats";

        Camera::point_cloud result;
        result.mime_type = mime_type;

        // Convert from nxLib format (XYZ interleaved) to Viam PCD format
        // Filter out NaN values (invalid points)
        VIAM_RESOURCE_LOG(debug) << "[get_point_cloud] Converting to PCD format";
        std::ostringstream pcd_data;
        int valid_points = 0;
        int invalid_points = 0;

        for (size_t i = 0; i < buffer.size(); i += 3) {
            float x = buffer[i];
            float y = buffer[i + 1];
            float z = buffer[i + 2];

            // Skip NaN points (invalid depth)
            if (std::isnan(x) || std::isnan(y) || std::isnan(z)) {
                invalid_points++;
                continue;
            }

            // Convert from mm to meters
            pcd_data << (x / 1000.0) << " " << (y / 1000.0) << " " << (z / 1000.0) << "\n";
            valid_points++;
        }

        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Point cloud: " << valid_points << " valid, " << invalid_points << " invalid";

        // Convert string to bytes for result.pc
        std::string pcd_str = pcd_data.str();
        result.pc = std::vector<unsigned char>(pcd_str.begin(), pcd_str.end());

        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Successfully created point cloud (" << result.pc.size() << " bytes)";
        return result;

    } catch (const NxLibException& ex) {
        VIAM_RESOURCE_LOG(error) << "[get_point_cloud] nxLib exception: " << ex.getErrorText();
        check_nxlib_error(ex);
        return Camera::point_cloud();  // Unreachable
    }
}

Camera::properties EnsensoCamera::get_properties() {
    VIAM_RESOURCE_LOG(debug) << "[get_properties] Retrieving camera properties";
    Camera::properties props;
    props.supports_pcd = enable_point_cloud_;
    props.intrinsic_parameters.width_px = width_px_;
    props.intrinsic_parameters.height_px = height_px_;

    try {
        // Get camera calibration parameters
        VIAM_RESOURCE_LOG(debug) << "[get_properties] Retrieving calibration parameters";
        NxLibItem calib = camera_node_[itmCalibration][itmMonocular][itmLeft];

        double fx = calib[itmCamera][0][0].asDouble();
        double fy = calib[itmCamera][1][1].asDouble();
        double cx = calib[itmCamera][0][2].asDouble();
        double cy = calib[itmCamera][1][2].asDouble();

        props.intrinsic_parameters.focal_x_px = fx;
        props.intrinsic_parameters.focal_y_px = fy;
        props.intrinsic_parameters.center_x_px = cx;
        props.intrinsic_parameters.center_y_px = cy;

        VIAM_RESOURCE_LOG(info) << "[get_properties] Calibration: fx=" << fx << ", fy=" << fy << ", cx=" << cx << ", cy=" << cy;

    } catch (const NxLibException& ex) {
        VIAM_RESOURCE_LOG(warn) << "[get_properties] Calibration not available, using defaults: " << ex.getErrorText();
    }

    VIAM_RESOURCE_LOG(info) << "[get_properties] Properties: " << width_px_ << "x" << height_px_ << ", supports_pcd=" << props.supports_pcd;
    return props;
}

std::vector<GeometryConfig> EnsensoCamera::get_geometries(const ProtoStruct& extra) {
    VIAM_RESOURCE_LOG(debug) << "[get_geometries] Called";
    // Return empty vector - no specific geometries for now
    return {};
}

void EnsensoCamera::reconfigure(const ProtoStruct& attrs) {
    VIAM_RESOURCE_LOG(info) << "[reconfigure] Reconfiguring camera";
    try {
        close_camera();
        parse_attributes(attrs);
        open_camera();
        VIAM_RESOURCE_LOG(info) << "[reconfigure] Reconfiguration complete";
    } catch (const std::exception& ex) {
        VIAM_RESOURCE_LOG(error) << "[reconfigure] Failed: " << ex.what();
        throw Exception(std::string("Failed to reconfigure camera: ") + ex.what());
    }
}

ProtoStruct EnsensoCamera::do_command(const ProtoStruct& command) {
    VIAM_RESOURCE_LOG(debug) << "[do_command] Called with " << command.size() << " parameter(s)";
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
