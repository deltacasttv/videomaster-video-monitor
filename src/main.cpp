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

#include <CLI/CLI.hpp>

#include <csignal>
#include <atomic>
#include <memory>

#include <VideoMasterCppApi/exception.hpp>
#include <VideoMasterCppApi/to_string.hpp>
#include <VideoMasterCppApi/api.hpp>
#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/stream/sdi/sdi_stream.hpp>
#include <VideoMasterCppApi/slot/sdi/sdi_slot.hpp>

#include "version.hpp"
#include "helper.hpp"
#include "shared_resources.hpp"
#include "windowed_renderer.hpp"

using namespace std::chrono_literals;
using namespace Deltacast::Wrapper;

Deltacast::SharedResources shared_resources;

void on_close(int /*signal*/)
{
    shared_resources.stop_is_requested = true;
}

int main(int argc, char** argv)
{
    CLI::App app{"Identify an incoming signal and display it on the screen"};
    
    int device_id = 0;
    app.add_option("-d,--device", device_id, "ID of the device to use");
    int rx_stream_id = 0;
    app.add_option("-i,--input", rx_stream_id, "ID of the input connector to use");
    CLI11_PARSE(app, argc, argv);

    signal(SIGINT, on_close);
    
    std::cout << "VideoMaster video-monitor (" << VERSTRING << ")" << std::endl;

    try
    {    
        std::cout << "VideoMaster API version: " << api_version() << std::endl;
        std::cout << "Discovered " << Board::count() << " devices" << std::endl;

        if (device_id >= Board::count())
        {
            std::cout << "Invalid device ID" << std::endl;
            return -1;
        }

        std::cout << "Opening device " << device_id << std::endl;
        auto board = Board::open(device_id);

        std::cout << board << std::endl;

        while (!shared_resources.stop_is_requested)
        {
            shared_resources.reset();

            std::cout << "Opening RX" << rx_stream_id << " stream..." << std::endl;
            auto rx_stream = board.sdi().open_stream(Application::Helper::rx_index_to_streamtype(rx_stream_id), VHD_SDI_STPROC_DISJOINED_VIDEO);

            std::cout << "Waiting for signal..." << std::endl;
            if (!Application::Helper::wait_for_input(board.rx(rx_stream_id), shared_resources.stop_is_requested))
            {
                std::cerr << "Application has been stopped before any input was received." << std::endl;
                return -1;
            }

            auto signal_information = Application::Helper::detect_information(rx_stream);
            auto video_characteristics = Application::Helper::get_video_characteristics(signal_information);
            std::cout << "Detected:" << std::endl;
            std::cout << "\t" << "Video standard: " << to_pretty_string(signal_information.video_standard) << std::endl;
            std::cout << "\t" << "Clock divisor: " << to_pretty_string(signal_information.clock_divisor) << std::endl;
            std::cout << "\t" << "Interface: " << to_pretty_string(signal_information.video_interface) << std::endl;

            rx_stream.buffer_queue().set_depth(8);
            rx_stream.set_buffer_packing(VHD_BUFPACK_VIDEO_YUV422_8);

            rx_stream.set_video_standard(signal_information.video_standard);
            rx_stream.set_interface(signal_information.video_interface);

            auto window_refresh_interval = 10ms;
            WindowedRenderer renderer("Live Content", video_characteristics.width / 2, video_characteristics.height / 2
                                                    , window_refresh_interval.count(), shared_resources.stop_is_requested);
            std::cout << "Initializing live content rendering window..." << std::endl;
            renderer.init(video_characteristics.width, video_characteristics.height, Deltacast::VideoViewer::InputFormat::ycbcr_422_8);

            std::cout << std::endl;

            std::cout << "Starting RX stream..." << std::endl;
            rx_stream.start();
            
            while (!shared_resources.stop_is_requested && !shared_resources.incoming_signal_changed)
            {
                if (!Application::Helper::wait_for_input(board.rx(rx_stream_id), shared_resources.stop_is_requested))
                {
                    std::this_thread::sleep_for(100ms);
                    continue;
                }
                
                if (Application::Helper::detect_information(rx_stream) != signal_information)
                {
                    shared_resources.incoming_signal_changed = true;
                    continue;
                }

                {
                    auto slot = rx_stream.pop_slot();
                    auto [ buffer, buffer_size ] = slot->video().buffer();
                    
                    renderer.render_buffer(buffer, buffer_size);
                }

                std::cout << "Slots count: " << rx_stream.buffer_queue().slots_count() 
                                             << " (dropped: " << rx_stream.buffer_queue().slots_dropped() << ")" << "\r";
            }

            std::cout << std::endl;
        }
    }
    catch (const ApiException& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << e.logs() << std::endl;
    }

    return 0;
}