#include "version.h"
#include "api_helper/api.hpp"
#include "api_helper/resource_manager.hpp"
#include "device.hpp"

#include <CLI/CLI.hpp>

#include <csignal>
#include <atomic>

bool stop_is_requested = false;

void on_close(int /*signal*/)
{
    stop_is_requested = true;    
}

int main(int argc, char** argv)
{
    int device_id = 0;
    int rx_stream_id = 0;

    CLI::App app{"Identify an incoming signal and display it on the screen"};
    
    app.add_option("-d,--device", device_id, "ID of the device to use");
    app.add_option("-i,--input", rx_stream_id, "ID of the input connector to use");
    CLI11_PARSE(app, argc, argv);

    signal(SIGINT, on_close);
    
    std::cout << "InputViewer (" << VERSTRING << ")" << std::endl;
    
    std::cout << std::endl;

    std::cout << "VideoMaster: " << Deltacast::Helper::get_api_version() << std::endl;
    std::cout << "Discovered " << Deltacast::Helper::get_number_of_devices() << " devices" << std::endl;

    std::cout << "Opening device " << device_id << std::endl;
    auto device = Deltacast::Device::create(device_id);
    if (!device) {
        std::cout << "Erreur opening the device " << device_id << std::endl;
        return -1;
    }
}