#include "ensenso_camera.hpp"
#include "nxlib_context.hpp"

#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace {
// Callback for stb to write into a std::vector<unsigned char>
void stb_write_to_vector(void* ctx, void* data, int size) {
    auto* buf = static_cast<std::vector<unsigned char>*>(ctx);
    const auto* ptr = static_cast<unsigned char*>(data);
    buf->insert(buf->end(), ptr, ptr + size);
}
}  // namespace

#include <viam/sdk/common/exception.hpp>
#include <viam/sdk/common/proto_value.hpp>
#include <viam/sdk/common/utils.hpp>

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
      camera_type_("Stereo"),
      width_px_(1280),
      height_px_(1024),
      enable_depth_(true),
      enable_point_cloud_(true),
      point_cloud_stride_(2),
      camera_open_(false) {
    VIAM_RESOURCE_LOG(info) << "[constructor] Starting Ensenso camera initialization for resource: " << name;

    parse_attributes(attrs);

    VIAM_RESOURCE_LOG(info) << "[constructor] Configuration: serial=" << serial_number_ << ", resolution=" << width_px_ << "x" << height_px_
                            << ", enable_depth=" << enable_depth_ << ", enable_point_cloud=" << enable_point_cloud_;

    // Get shared nxLib context — initialize without waiting for camera enumeration
    // (cameras enumerate in the background; open_camera() retries until they appear)
    try {
        VIAM_RESOURCE_LOG(debug) << "[constructor] Getting shared nxLib context";
        nxlib_context_ = NxLibContext::get_instance(false);
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

    // Parse camera type
    if (attrs.count("camera_type")) {
        camera_type_ = attrs.at("camera_type").get_unchecked<std::string>();
    }

    // Parse point cloud stride (downsample factor, default 2)
    if (attrs.count("point_cloud_stride")) {
        int s = static_cast<int>(attrs.at("point_cloud_stride").get_unchecked<double>());
        point_cloud_stride_ = std::max(1, s);
    }

    // Parse feature flags — monocular cameras can't produce depth or point clouds
    bool is_stereo = (camera_type_ == "Stereo");
    if (attrs.count("enable_depth")) {
        enable_depth_ = attrs.at("enable_depth").get_unchecked<bool>() && is_stereo;
    } else {
        enable_depth_ = is_stereo;
    }
    if (attrs.count("enable_point_cloud")) {
        enable_point_cloud_ = attrs.at("enable_point_cloud").get_unchecked<bool>() && is_stereo;
    } else {
        enable_point_cloud_ = is_stereo;
    }
}

void EnsensoCamera::open_camera() {
    try {
        VIAM_RESOURCE_LOG(info) << "[open_camera] Starting camera open procedure";

        // nxLib enumerates cameras in the background after nxLibInitialize(false).
        // Poll until the requested camera appears (up to 30s).
        constexpr int max_wait_s = 30;
        constexpr int poll_interval_ms = 500;
        NxLibItem cameras = NxLibItem()[itmCameras];

        for (int waited_ms = 0;; waited_ms += poll_interval_ms) {
            cameras = NxLibItem()[itmCameras];
            int count = cameras.count();

            bool found = !serial_number_.empty() ? cameras[serial_number_].exists() : count > 0;

            if (found)
                break;

            if (waited_ms >= max_wait_s * 1000) {
                std::string msg = serial_number_.empty()
                                      ? "No Ensenso cameras found after " + std::to_string(max_wait_s) + "s"
                                      : "Camera '" + serial_number_ + "' not found after " + std::to_string(max_wait_s) + "s";
                VIAM_RESOURCE_LOG(error) << "[open_camera] " << msg;
                throw Exception(msg);
            }

            if (waited_ms == 0) {
                VIAM_RESOURCE_LOG(info) << "[open_camera] Waiting for camera enumeration...";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
        }

        int available_count = cameras.count();
        VIAM_RESOURCE_LOG(info) << "[open_camera] Found " << available_count << " cameras in nxLib tree";

        // Select target camera
        if (!serial_number_.empty()) {
            VIAM_RESOURCE_LOG(info) << "[open_camera] Looking for camera with serial: " << serial_number_;
            camera_node_ = cameras[serial_number_];
            VIAM_RESOURCE_LOG(info) << "[open_camera] Found camera node for serial: " << serial_number_;
        } else {
            // Open first available camera
            VIAM_RESOURCE_LOG(info) << "[open_camera] No serial specified, using first available camera";
            serial_number_ = cameras[0].name();
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

        // Configure capture parameters. Manual exposure + explicit projector on
        // so the active-stereo pattern is actually visible in the stereo pair.
        VIAM_RESOURCE_LOG(debug) << "[open_camera] Configuring capture parameters";
        try {
            camera_node_[itmParameters][itmCapture][itmAutoBlackLevel] = true;
            camera_node_[itmParameters][itmCapture][itmAutoGain] = false;
            camera_node_[itmParameters][itmCapture][itmAutoExposure] = false;
            camera_node_[itmParameters][itmCapture][itmExposure] = 8.0;
            camera_node_[itmParameters][itmCapture][itmGain] = 4.0;
            camera_node_[itmParameters][itmCapture][itmProjector] = true;
            camera_node_[itmParameters][itmCapture][itmFrontLight] = false;
            VIAM_RESOURCE_LOG(info) << "[open_camera] Capture parameters set: exposure=8ms gain=4 projector=on frontlight=off";
        } catch (const NxLibException& ex) {
            VIAM_RESOURCE_LOG(warn) << "[open_camera] Some capture parameters not supported: " << ex.getErrorText();
        }

        // Try to open linked color camera for texture (XYZRGB point clouds)
        if (camera_type_ == "Stereo") {
            std::string potential_color = serial_number_ + "-Color";
            NxLibItem all_cameras = NxLibItem()[itmCameras];
            if (all_cameras[potential_color].exists()) {
                VIAM_RESOURCE_LOG(info) << "[open_camera] Found linked color camera: " << potential_color;
                try {
                    NxLibCommand open_color(cmdOpen);
                    open_color.parameters()[itmCameras] = potential_color;
                    open_color.execute();
                    color_serial_ = potential_color;
                    // Enable texture capture so cmdCapture also grabs the color camera
                    camera_node_[itmParameters][itmCapture][itmTexture][itmEnabled] = true;
                    VIAM_RESOURCE_LOG(info) << "[open_camera] Texture capture enabled with color camera: " << color_serial_;
                } catch (const NxLibException& ex) {
                    VIAM_RESOURCE_LOG(warn) << "[open_camera] Could not open color camera or enable texture: " << ex.getErrorText();
                    color_serial_.clear();
                }
            } else {
                VIAM_RESOURCE_LOG(info) << "[open_camera] No linked color camera found (" << potential_color << ")";
            }
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
        // Close linked color camera first
        if (!color_serial_.empty()) {
            try {
                NxLibCommand close_color(cmdClose);
                close_color.parameters()[itmCameras] = color_serial_;
                close_color.execute();
                VIAM_RESOURCE_LOG(info) << "[close_camera] Color camera closed: " << color_serial_;
            } catch (const NxLibException& ex) {
                VIAM_RESOURCE_LOG(warn) << "[close_camera] Error closing color camera: " << ex.getErrorText();
            }
            color_serial_.clear();
        }
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
        NxLibCommand capture(cmdCapture);
        if (!color_serial_.empty()) {
            capture.parameters()[itmCameras][0] = serial_number_;
            capture.parameters()[itmCameras][1] = color_serial_;
        } else {
            capture.parameters()[itmCameras] = serial_number_;
        }
        VIAM_RESOURCE_LOG(debug) << "[capture_images] Executing cmdCapture";
        capture.execute();
        VIAM_RESOURCE_LOG(debug) << "[capture_images] Capture complete";

        // Rectify stereo camera images
        if (camera_type_ == "Stereo") {
            VIAM_RESOURCE_LOG(debug) << "[capture_images] Executing cmdRectifyImages (stereo)";
            NxLibCommand rectify(cmdRectifyImages);
            rectify.parameters()[itmCameras] = serial_number_;
            rectify.execute();
            VIAM_RESOURCE_LOG(debug) << "[capture_images] Stereo rectification complete";

            // Also rectify the linked color camera to remove fisheye distortion
            if (!color_serial_.empty()) {
                VIAM_RESOURCE_LOG(debug) << "[capture_images] Executing cmdRectifyImages (color)";
                NxLibCommand rectify_color(cmdRectifyImages);
                rectify_color.parameters()[itmCameras] = color_serial_;
                rectify_color.execute();
                VIAM_RESOURCE_LOG(debug) << "[capture_images] Color rectification complete";
            }
        }
    } catch (const NxLibException& ex) {
        VIAM_RESOURCE_LOG(error) << "[capture_images] nxLib exception: " << ex.getErrorText();
        check_nxlib_error(ex);
    }
}

Camera::image_collection EnsensoCamera::get_images(std::vector<std::string> filter_source_names, const ProtoStruct& extra) {
    std::lock_guard<std::mutex> lock(camera_mutex_);
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
    if (return_all || std::find(filter_source_names.begin(), filter_source_names.end(), "color") != filter_source_names.end()) {
        VIAM_RESOURCE_LOG(debug) << "[get_images] Attempting to get color image";
        try {
            Camera::raw_image color_img = get_color_image("");
            color_img.source_name = "color";
            images.images.push_back(color_img);
            VIAM_RESOURCE_LOG(info) << "[get_images] Added color image (" << color_img.bytes.size() << " bytes)";
        } catch (const std::exception& ex) {
            VIAM_RESOURCE_LOG(warn) << "[get_images] Failed to get color image: " << ex.what();
        }
    }

    // Add depth image if enabled
    if (enable_depth_ &&
        (return_all || std::find(filter_source_names.begin(), filter_source_names.end(), "depth") != filter_source_names.end())) {
        VIAM_RESOURCE_LOG(debug) << "[get_images] Attempting to get depth image (enabled=" << enable_depth_ << ")";
        try {
            compute_point_cloud();  // Need to compute disparity map first
            Camera::raw_image depth_img = get_depth_image("");
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
    VIAM_RESOURCE_LOG(debug) << "[get_color_image] Fetching color image";
    try {
        // Stereo cameras: prefer rectified left image, fall back to raw left.
        // Monocular cameras: image is stored directly at Raw (no Left/Right children).
        NxLibItem leftImg;
        if (camera_type_ == "Stereo" && !color_serial_.empty()) {
            // Use linked color camera's rectified image (removes fisheye distortion)
            VIAM_RESOURCE_LOG(debug) << "[get_color_image] Reading from color camera: " << color_serial_;
            NxLibItem color_node = NxLibItem()[itmCameras][color_serial_];
            leftImg = color_node[itmImages][itmRectified];
            if (!leftImg.exists()) {
                VIAM_RESOURCE_LOG(warn) << "[get_color_image] Color rectified not found, falling back to raw";
                leftImg = color_node[itmImages][itmRaw];
            }
        } else if (camera_type_ == "Stereo") {
            leftImg = camera_node_[itmImages][itmRectified][itmLeft];
            if (!leftImg.exists()) {
                VIAM_RESOURCE_LOG(warn) << "[get_color_image] Rectified/Left not found, trying Raw/Left";
                leftImg = camera_node_[itmImages][itmRaw][itmLeft];
            }
        } else {
            leftImg = camera_node_[itmImages][itmRaw];
        }

        if (!leftImg.exists()) {
            VIAM_RESOURCE_LOG(error) << "[get_color_image] No image found. Images node JSON: " << camera_node_[itmImages].asJson(true);
            throw Exception("Image not available");
        }

        int width, height, channels, bytesPerElement;
        double timestamp;
        leftImg.getBinaryDataInfo(&width, &height, &channels, &bytesPerElement, nullptr, &timestamp);

        VIAM_RESOURCE_LOG(debug) << "[get_color_image] Image info: " << width << "x" << height << ", channels=" << channels
                                 << ", bpe=" << bytesPerElement;

        std::vector<unsigned char> raw;
        leftImg.getBinaryData(raw, nullptr);

        Camera::raw_image result;

        if (bytesPerElement == 1) {
            // 8-bit data: encode directly as JPEG
            std::vector<unsigned char> jpeg;
            stbi_write_jpg_to_func(stb_write_to_vector, &jpeg, width, height, channels, raw.data(), 85);
            result.mime_type = "image/jpeg";
            result.bytes = std::move(jpeg);
        } else {
            // Unexpected format: return raw with a descriptive mime type so the
            // caller at least gets data and can diagnose
            VIAM_RESOURCE_LOG(warn) << "[get_color_image] Unexpected bpe=" << bytesPerElement << ", returning raw bytes";
            result.mime_type = "image/jpeg";
            result.bytes = std::move(raw);
        }

        VIAM_RESOURCE_LOG(info) << "[get_color_image] Successfully created color image (" << result.bytes.size() << " bytes)";
        return result;

    } catch (const NxLibException& ex) {
        VIAM_RESOURCE_LOG(error) << "[get_color_image] nxLib exception: " << ex.getErrorText();
        check_nxlib_error(ex);
        return Camera::raw_image();  // Unreachable
    }
}

Camera::raw_image EnsensoCamera::get_depth_image(const std::string& mime_type) {
    VIAM_RESOURCE_LOG(debug) << "[get_depth_image] Fetching depth image";
    try {
        NxLibItem disparityMap = camera_node_[itmImages][itmDisparityMap];

        if (!disparityMap.exists()) {
            VIAM_RESOURCE_LOG(error) << "[get_depth_image] Disparity map does not exist. Images node JSON: "
                                     << camera_node_[itmImages].asJson(true);
            throw Exception("Disparity map not available");
        }

        int width, height, channels, bytesPerElement;
        disparityMap.getBinaryDataInfo(&width, &height, &channels, &bytesPerElement, nullptr, nullptr);

        VIAM_RESOURCE_LOG(debug) << "[get_depth_image] Disparity map info: " << width << "x" << height << ", channels=" << channels
                                 << ", bpe=" << bytesPerElement;

        std::vector<unsigned char> raw;
        disparityMap.getBinaryData(raw, nullptr);

        // Normalize disparity values to 8-bit grayscale for visualization
        int num_pixels = width * height;
        std::vector<unsigned char> gray(num_pixels, 0);

        if (bytesPerElement == 2) {
            // DisparityMap is signed 16-bit, scaled x16 for subpixel resolution.
            // Invalid pixels are marked with 0x8000 (INT16_MIN).
            static constexpr int16_t INVALID = static_cast<int16_t>(0x8000);
            const int16_t* src = reinterpret_cast<const int16_t*>(raw.data());
            int16_t min_val = INT16_MAX, max_val = INT16_MIN;
            for (int i = 0; i < num_pixels; i++) {
                if (src[i] != INVALID) {
                    min_val = std::min(min_val, src[i]);
                    max_val = std::max(max_val, src[i]);
                }
            }
            float range = (max_val > min_val) ? static_cast<float>(max_val - min_val) : 1.0f;
            for (int i = 0; i < num_pixels; i++) {
                gray[i] = (src[i] == INVALID) ? 0 : static_cast<unsigned char>(255.0f * (src[i] - min_val) / range);
            }
        } else if (bytesPerElement == 4) {
            const float* src = reinterpret_cast<const float*>(raw.data());
            float min_val = FLT_MAX, max_val = -FLT_MAX;
            for (int i = 0; i < num_pixels; i++) {
                if (!std::isnan(src[i]) && src[i] > 0.0f) {
                    min_val = std::min(min_val, src[i]);
                    max_val = std::max(max_val, src[i]);
                }
            }
            float range = (max_val > min_val) ? (max_val - min_val) : 1.0f;
            for (int i = 0; i < num_pixels; i++) {
                gray[i] = (std::isnan(src[i]) || src[i] <= 0.0f) ? 0 : static_cast<unsigned char>(255.0f * (src[i] - min_val) / range);
            }
        } else {
            VIAM_RESOURCE_LOG(warn) << "[get_depth_image] Unexpected bpe=" << bytesPerElement;
        }

        std::vector<unsigned char> jpeg;
        stbi_write_jpg_to_func(stb_write_to_vector, &jpeg, width, height, 1, gray.data(), 85);

        Camera::raw_image result;
        result.mime_type = "image/jpeg";
        result.bytes = std::move(jpeg);

        VIAM_RESOURCE_LOG(info) << "[get_depth_image] Successfully created depth image (" << result.bytes.size() << " bytes)";
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
    std::lock_guard<std::mutex> lock(camera_mutex_);
    VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Called with mime_type=" << mime_type;

    if (!enable_point_cloud_) {
        VIAM_RESOURCE_LOG(warn) << "[get_point_cloud] Point cloud generation is disabled";
        throw Exception("Point cloud generation is disabled");
    }

    try {
        // Capture stereo + color cameras together (explicit list ensures color is captured)
        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Step 1: cmdCapture";
        NxLibCommand capture(cmdCapture);
        if (!color_serial_.empty()) {
            capture.parameters()[itmCameras][0] = serial_number_;
            capture.parameters()[itmCameras][1] = color_serial_;
        } else {
            capture.parameters()[itmCameras] = serial_number_;
        }
        capture.execute();
        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Step 1 done";

        // Rectify
        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Step 2: cmdRectifyImages";
        NxLibCommand rectify(cmdRectifyImages);
        rectify.parameters()[itmCameras] = serial_number_;
        rectify.execute();
        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Step 2 done";

        // Compute disparity map
        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Step 3: cmdComputeDisparityMap";
        compute_point_cloud();
        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Step 3 done";

        // Convert disparity map to 3D point cloud
        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Step 4: cmdComputePointMap";
        NxLibCommand computePointMap(cmdComputePointMap);
        computePointMap.parameters()[itmCameras] = serial_number_;
        computePointMap.execute();
        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Step 4 done";

        NxLibItem pointMap = camera_node_[itmImages][itmPointMap];

        if (!pointMap.exists()) {
            VIAM_RESOURCE_LOG(error) << "[get_point_cloud] Point map does not exist. Images node JSON: "
                                     << camera_node_[itmImages].asJson(true);
            throw Exception("Point map not available");
        }

        int width, height, channels, bytesPerElement;
        std::vector<float> buffer;

        pointMap.getBinaryDataInfo(&width, &height, &channels, &bytesPerElement, nullptr, nullptr);

        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Point map: " << width << "x" << height << " channels=" << channels
                                << " bpe=" << bytesPerElement;

        pointMap.getBinaryData(buffer, nullptr);
        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Retrieved " << buffer.size() << " floats";

        Camera::point_cloud result;
        result.mime_type = mime_type;

        // Try to compute color texture if linked color camera is available
        std::vector<unsigned char> texture_data;
        int tex_width = 0, tex_height = 0, tex_channels = 0;
        bool has_texture = false;

        if (!color_serial_.empty()) {
            try {
                VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Step 5: cmdComputeTexture (color=" << color_serial_ << ")";
                NxLibCommand computeTexture(cmdComputeTexture);
                computeTexture.parameters()[itmCameras] = serial_number_;
                computeTexture.parameters()[itmTexture] = color_serial_;
                computeTexture.execute();
                VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Step 5 done";

                NxLibItem textureImg = camera_node_[itmImages][itmRectifiedTexture][itmLeft];
                if (textureImg.exists()) {
                    int bpe;
                    textureImg.getBinaryDataInfo(&tex_width, &tex_height, &tex_channels, &bpe, nullptr, nullptr);
                    textureImg.getBinaryData(texture_data, nullptr);
                    has_texture = (tex_width == width && tex_height == height);
                    VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Texture: " << tex_width << "x" << tex_height
                                            << ", channels=" << tex_channels << ", has_texture=" << has_texture;
                }
            } catch (const NxLibException& ex) {
                VIAM_RESOURCE_LOG(warn) << "[get_point_cloud] Could not compute texture: " << ex.getErrorText();
            }
        }

        // Filter valid points and convert mm -> meters, optionally with color
        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Converting to binary PCD format (color=" << has_texture
                                << ", stride=" << point_cloud_stride_ << ")";

        // Each valid point: x,y,z (3 floats) + optionally rgb (1 float packed)
        int floats_per_point = has_texture ? 4 : 3;
        std::vector<float> valid_points_data;
        valid_points_data.reserve((buffer.size() / 3 / (point_cloud_stride_ * point_cloud_stride_)) * floats_per_point);
        int invalid_points = 0;

        for (int row = 0; row < height; row += point_cloud_stride_) {
            for (int col = 0; col < width; col += point_cloud_stride_) {
                int pi = row * width + col;
                size_t i = static_cast<size_t>(pi) * 3;
                if (i + 2 >= buffer.size())
                    break;
                float x = buffer[i], y = buffer[i + 1], z = buffer[i + 2];
                if (std::isnan(x) || std::isnan(y) || std::isnan(z)) {
                    invalid_points++;
                    continue;
                }
                valid_points_data.push_back(x / 1000.0f);
                valid_points_data.push_back(y / 1000.0f);
                valid_points_data.push_back(z / 1000.0f);

                if (has_texture) {
                    // Texture is BGRA (4 channels) or RGB/BGR (3 channels) at 8-bit
                    size_t tex_idx = static_cast<size_t>(pi) * tex_channels;
                    uint8_t r = 0, g = 0, b = 0;
                    if (tex_channels >= 3 && tex_idx + 2 < texture_data.size()) {
                        // nxLib stores rectified texture as RGBA
                        r = texture_data[tex_idx];
                        g = texture_data[tex_idx + 1];
                        b = texture_data[tex_idx + 2];
                    } else if (tex_channels == 1 && tex_idx < texture_data.size()) {
                        r = g = b = texture_data[tex_idx];
                    }
                    uint32_t rgb_packed = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
                    float rgb_float;
                    std::memcpy(&rgb_float, &rgb_packed, sizeof(float));
                    valid_points_data.push_back(rgb_float);
                }
            }  // col
        }  // row

        int valid_points = static_cast<int>(valid_points_data.size() / floats_per_point);
        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Point cloud: " << valid_points << " valid, " << invalid_points << " invalid";

        // Build binary PCD header
        std::ostringstream header;
        header << "VERSION 0.7\n";
        if (has_texture) {
            header << "FIELDS x y z rgb\n"
                   << "SIZE 4 4 4 4\n"
                   << "TYPE F F F F\n"
                   << "COUNT 1 1 1 1\n";
        } else {
            header << "FIELDS x y z\n"
                   << "SIZE 4 4 4\n"
                   << "TYPE F F F\n"
                   << "COUNT 1 1 1\n";
        }
        header << "WIDTH " << valid_points << "\n"
               << "HEIGHT 1\n"
               << "VIEWPOINT 0 0 0 1 0 0 0\n"
               << "POINTS " << valid_points << "\n"
               << "DATA binary\n";
        std::string header_str = header.str();

        std::vector<unsigned char> pcd;
        pcd.reserve(header_str.size() + valid_points_data.size() * sizeof(float));
        pcd.insert(pcd.end(), header_str.begin(), header_str.end());
        const auto* bytes = reinterpret_cast<const unsigned char*>(valid_points_data.data());
        pcd.insert(pcd.end(), bytes, bytes + valid_points_data.size() * sizeof(float));

        result.pc = std::move(pcd);

        VIAM_RESOURCE_LOG(info) << "[get_point_cloud] Successfully created point cloud (" << result.pc.size() << " bytes)";
        return result;

    } catch (const NxLibException& ex) {
        VIAM_RESOURCE_LOG(error) << "[get_point_cloud] nxLib exception: " << ex.getErrorText();
        check_nxlib_error(ex);
        return Camera::point_cloud();  // Unreachable
    }
}

Camera::properties EnsensoCamera::get_properties() {
    std::lock_guard<std::mutex> lock(camera_mutex_);
    VIAM_RESOURCE_LOG(debug) << "[get_properties] Retrieving camera properties";
    Camera::properties props{};
    props.supports_pcd = enable_point_cloud_;
    props.intrinsic_parameters.width_px = width_px_;
    props.intrinsic_parameters.height_px = height_px_;
    // On-demand capture; no fixed frame rate.
    props.frame_rate = 0.f;

    // Let NxLibException propagate on failure. A silent fallback to zero
    // intrinsics would cause downstream consumers (motion, vision, cartographer)
    // to divide by zero and produce garbage.
    NxLibItem calib = camera_node_[itmCalibration][itmMonocular][itmLeft];
    double fx = calib[itmCamera][0][0].asDouble();
    double fy = calib[itmCamera][1][1].asDouble();
    double cx = calib[itmCamera][0][2].asDouble();
    double cy = calib[itmCamera][1][2].asDouble();

    props.intrinsic_parameters.focal_x_px = fx;
    props.intrinsic_parameters.focal_y_px = fy;
    props.intrinsic_parameters.center_x_px = cx;
    props.intrinsic_parameters.center_y_px = cy;

    VIAM_RESOURCE_LOG(info) << "[get_properties] fx=" << fx << " fy=" << fy << " cx=" << cx << " cy=" << cy << " " << width_px_ << "x"
                            << height_px_ << " supports_pcd=" << props.supports_pcd;
    return props;
}

std::vector<GeometryConfig> EnsensoCamera::get_geometries(const ProtoStruct& extra) {
    VIAM_RESOURCE_LOG(debug) << "[get_geometries] Called";
    // Return empty vector - no specific geometries for now
    return {};
}

void EnsensoCamera::reconfigure(const ProtoStruct& attrs) {
    std::lock_guard<std::mutex> lock(camera_mutex_);
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
std::shared_ptr<Camera> create_ensenso_camera(const std::string& name, const ProtoStruct& attrs) {
    return std::make_shared<EnsensoCamera>(name, attrs);
}

}  // namespace ensenso
}  // namespace camera
}  // namespace viam
