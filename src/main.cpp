#include "version.h"
#include "device.hpp"
#include "signal_information.hpp"
#include "shared_resources.hpp"
#include "rx_renderer.hpp"
#include "rx_stream.hpp"

#include "api_helper/api.hpp"
#include "api_helper/resource_manager.hpp"

#include <CLI/CLI.hpp>

#include <csignal>
#include <atomic>
#include <memory>

Deltacast::SharedResources shared_resources;

void on_close(int /*signal*/)
{
    shared_resources.stop_is_requested = true;
}

int main(int argc, char** argv)
{
    using namespace std::chrono_literals;
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
        std::cout << "Error opening the device " << device_id << std::endl;
        return -1;
    }

    while (!shared_resources.stop_is_requested) {
        shared_resources.reset();
        std::cout << "Waiting for incoming signal" << std::endl;
        if (!device->wait_for_incoming_signal(rx_stream_id, shared_resources.stop_is_requested)) {
            std::cout << "Error waiting for incoming signal" << std::endl;
            return -1;
        }
        std::cout << "Getting incoming signal information" << std::endl;

        shared_resources.signal_info = device->get_incoming_signal_information(rx_stream_id);

        std::cout << "Incoming signal information:" << std::endl;
        std::cout << "\t" << "video standard: " << Deltacast::Helper::enum_to_string(shared_resources.signal_info.video_standard) << std::endl;
        std::cout << "\t" << "clock divisor: " << Deltacast::Helper::enum_to_string(shared_resources.signal_info.clock_divisor) << std::endl;
        std::cout << "\t" << "interface: " << Deltacast::Helper::enum_to_string(shared_resources.signal_info.interface) << std::endl;

        Deltacast::DecodedSignalInformation decoded_signal_info = Deltacast::decode(shared_resources.signal_info);
        std::cout << "Decoded signal information:" << std::endl;
        std::cout << "\t" << "width: " << decoded_signal_info.width << std::endl;
        std::cout << "\t" << "height: " << decoded_signal_info.height << std::endl;
        std::cout << "\t" << "progressive: " << (decoded_signal_info.progressive ? "true" : "false") << std::endl;
        std::cout << "\t" << "framerate: " << decoded_signal_info.framerate << std::endl;

        std::cout << std::endl;

        auto window_refresh_interval = 10ms;
        RxRenderer renderer("Live Content", decoded_signal_info.width / 2,
                            decoded_signal_info.height / 2, window_refresh_interval.count(),
                            shared_resources.stop_is_requested);
        std::cout << "Initializing live content rendering window" << std::endl;
        // starts the rendering thread
        renderer.init(decoded_signal_info.width, decoded_signal_info.height, Deltacast::VideoViewer::InputFormat::ycbcr_422_8);

        std::cout << std::endl;

        std::cout << "Opening RX stream " << rx_stream_id << "" << std::endl;
        std::unique_ptr<Deltacast::RxStream> rx_stream;

        try {
            rx_stream = std::make_unique<Deltacast::RxStream>(*device, "RX Stream", rx_stream_id);
        } catch (const std::exception& e) {
            std::cout << e.what() << std::endl;
            return -1;
        }

        std::cout << "Configuring and starting RX stream" << std::endl;
        if (!rx_stream->configure(shared_resources.signal_info))
            return -1;
        if (!rx_stream->start())
            return -1;
        
        Deltacast::SignalInformation detected_signal_info;
        BYTE* buffer;
        ULONG buffer_size;
        // starts the get and set frame loop
        while (!shared_resources.stop_is_requested && !shared_resources.incoming_signal_changed) {
            // the incoming signal might have changed
            try {
                detected_signal_info = device->get_incoming_signal_information(rx_stream_id);
            }
            catch(const std::exception& e) {
                std::this_thread::sleep_for(100ms);
                continue;
            }
            if (!(detected_signal_info == shared_resources.signal_info)) {
                shared_resources.incoming_signal_changed = true;
                continue;
            }
            if (!rx_stream->lock_slot())
               continue;
            if (!rx_stream->get_buffer(buffer, buffer_size))
                return -1;
            renderer.render_buffer(buffer, buffer_size);
            if (!rx_stream->unlock_slot())
                return -1;
        }
    }
    return 0;
}