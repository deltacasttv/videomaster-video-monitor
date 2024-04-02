/*
 * SPDX-FileCopyrightText: Copyright (c) DELTACAST.TV. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at * * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "version.h"
#include "device.hpp"
#include "shared_resources.hpp"
#include "rx_stream.hpp"
#include "windowed_renderer.hpp"

#include "VideoMasterAPIHelper/api.hpp"
#include "VideoMasterAPIHelper/resource_manager.hpp"

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
    if (!device)
    {
        std::cout << "Error opening the device " << device_id << std::endl;
        return -1;
    }

    std::cout << *device << std::endl;

    while (!shared_resources.stop_is_requested)
    {
        shared_resources.reset();
        std::cout << "Waiting for incoming signal" << std::endl;
        if (!device->wait_for_incoming_signal(rx_stream_id, shared_resources.stop_is_requested))
        {
            std::cout << "Error waiting for incoming signal" << std::endl;
            return -1;
        }

        std::cout << "Opening RX stream " << rx_stream_id << "" << std::endl;
        std::unique_ptr<Deltacast::RxStream> rx_stream;

        try
        {
            rx_stream = std::make_unique<Deltacast::RxStream>(*device, "RX Stream", rx_stream_id);
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
            return -1;
        }

        std::cout << "Configuring RX stream" << std::endl;
        if (!rx_stream->configure(shared_resources.sdi_video_info))
            return -1;

        std::cout << "Getting incoming signal information" << std::endl;
        auto current_video_format = shared_resources.sdi_video_info.get_video_format(rx_stream->handle()).value();
        std::cout << current_video_format << std::endl;

        auto window_refresh_interval = 10ms;
        WindowedRenderer renderer("Live Content", current_video_format.width / 2,
                            current_video_format.height / 2, window_refresh_interval.count(),
                            shared_resources.stop_is_requested);
        std::cout << "Initializing live content rendering window" << std::endl;
        renderer.init(current_video_format.width, current_video_format.height, Deltacast::VideoViewer::InputFormat::ycbcr_422_8);

        std::cout << std::endl;

        std::cout << "Starting RX stream" << std::endl;
        if (!rx_stream->start())
            return -1;
        
        while (!shared_resources.stop_is_requested && !shared_resources.incoming_signal_changed)
        {
            if (!device->wait_for_incoming_signal(rx_stream_id, shared_resources.stop_is_requested))
            {
                std::this_thread::sleep_for(100ms);
                continue;
            }
            
            auto detected_video_format = shared_resources.sdi_video_info.get_video_format(rx_stream->handle()).value();
            if (detected_video_format != current_video_format)
            {
                shared_resources.incoming_signal_changed = true;
                continue;
            }
            if (!rx_stream->lock_slot())
               continue;

            auto optional_buffer = rx_stream->get_buffer();
            if (!optional_buffer)
                return -1;
            auto [buffer, buffer_size] = optional_buffer.value();

            renderer.render_buffer(buffer, buffer_size);

            if (!rx_stream->unlock_slot())
                return -1;

            std::cout << "\rSlot count: " << rx_stream->slot_count() << " (dropped " << rx_stream->dropped_slot_count() << " frames)          ";
        }
        std::cout << std::endl;
    }
    return 0;
}