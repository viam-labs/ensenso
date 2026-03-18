#include <memory>
#include <signal.h>
#include <vector>

#include <viam/sdk/module/service.hpp>
#include <viam/sdk/components/camera.hpp>
#include <viam/sdk/registry/registry.hpp>
#include <viam/sdk/resource/resource.hpp>

#include "ensenso_camera.hpp"

using namespace viam::sdk;

int main(int argc, char** argv) {
    // Set up signal handling for graceful shutdown
    signal(SIGTERM, [](int signum) {
        exit(0);
    });

    try {
        // Create model registration for Ensenso camera
        std::vector<std::shared_ptr<ModelRegistration>> registrations;

        auto ensenso_model = std::make_shared<ModelRegistration>(
            API::traits<Camera>::api(),
            Model{"viam", "camera", "ensenso"},
            [](Dependencies, ResourceConfig cfg) -> std::shared_ptr<Resource> {
                return std::make_shared<viam::camera::ensenso::EnsensoCamera>(
                    cfg.name(),
                    cfg.attributes()
                );
            }
        );

        registrations.push_back(ensenso_model);

        // Create the module service with registrations
        auto module = std::make_shared<ModuleService>(argc, argv, registrations);

        // Start the module service
        module->serve();

    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
