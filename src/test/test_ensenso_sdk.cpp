/**
 * Simple test to verify Ensenso SDK integration
 * This can be built and run independently to test SDK access
 */

#include <iostream>
#include <string>

#include "nxLib.h"

int main() {
    std::cout << "Testing Ensenso SDK integration..." << std::endl;
    std::cout << "NxLib version: " << NXLIB_VERSION_MAJOR << "." << NXLIB_VERSION_MINOR << "." << NXLIB_VERSION_BUILD << std::endl;

    try {
        // Initialize nxLib
        std::cout << "\nInitializing nxLib..." << std::endl;
        nxLibInitialize(true);
        std::cout << "✓ nxLib initialized successfully" << std::endl;

        // List available cameras
        NxLibItem root;
        NxLibItem cameras = root[itmCameras][itmBySerialNo];

        // Get the count of cameras
        std::vector<std::string> serials;
        for (int i = 0; i < cameras.count(); i++) {
            serials.push_back(cameras[i].name());
        }

        std::cout << "\n✓ Found " << serials.size() << " camera(s)" << std::endl;

        if (!serials.empty()) {
            std::cout << "\nAvailable cameras:" << std::endl;
            for (size_t i = 0; i < serials.size(); i++) {
                const std::string& serial = serials[i];
                NxLibItem camera = cameras[serial];

                std::string type = camera[itmType].exists() ? camera[itmType].asString() : "Unknown";

                std::string model = camera[itmModelName].exists() ? camera[itmModelName].asString() : "Unknown";

                std::cout << "  [" << i << "] Serial: " << serial << ", Type: " << type << ", Model: " << model << std::endl;
            }
        } else {
            std::cout << "\n⚠ No cameras detected. Please connect an Ensenso camera." << std::endl;
        }

        // Clean up
        std::cout << "\nFinalizing nxLib..." << std::endl;
        nxLibFinalize();
        std::cout << "✓ nxLib finalized successfully" << std::endl;

        std::cout << "\n✅ All tests passed!" << std::endl;
        return 0;

    } catch (const NxLibException& ex) {
        std::cerr << "❌ NxLib error " << ex.getErrorCode() << ": " << ex.getErrorText() << std::endl;
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "❌ Error: " << ex.what() << std::endl;
        return 1;
    }
}
