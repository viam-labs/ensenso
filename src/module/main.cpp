#include <memory>
#include <signal.h>
#include <vector>

#include <viam/sdk/common/instance.hpp>
#include <viam/sdk/module/service.hpp>
#include <viam/sdk/components/camera.hpp>
#include <viam/sdk/services/discovery.hpp>
#include <viam/sdk/registry/registry.hpp>
#include <viam/sdk/resource/resource.hpp>

#include "ensenso_camera.hpp"
#include "discovery.hpp"

using namespace viam::sdk;

int main(int argc, char** argv) {
    // Every Viam C++ SDK program must have one and only one Instance object
    // which is created before any other C++ SDK objects and stays alive until
    // all Viam C++ SDK objects are destroyed.
    viam::sdk::Instance inst;

    // Set up signal handling for graceful shutdown
    signal(SIGTERM, [](int signum) {
        exit(0);
    });

    try {
        // Create model registrations
        std::vector<std::shared_ptr<ModelRegistration>> registrations;

        // Register Ensenso camera component
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

        // Register Ensenso discovery service
        auto discovery_model = std::make_shared<ModelRegistration>(
            API::traits<Discovery>::api(),
            viam::camera::ensenso::discovery::EnsensoDiscovery::model,
            [](Dependencies deps, ResourceConfig cfg) -> std::shared_ptr<Resource> {
                return std::make_shared<viam::camera::ensenso::discovery::EnsensoDiscovery>(
                    deps,
                    cfg
                );
            }
        );
        registrations.push_back(discovery_model);

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
