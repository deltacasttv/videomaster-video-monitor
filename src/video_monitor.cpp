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

#include "video_monitor.hpp"
#include "helper.hpp"
#include "version.hpp"
#include "windowed_renderer.hpp"

#include <VideoMasterCppApi/api.hpp>
#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/exception.hpp>

using namespace std::chrono_literals;

namespace Deltacast::VideoMonitor
{
    VideoMonitorApp::VideoMonitorApp(SharedResources& shared_resources)
        : m_app{ "Identify an incoming signal and display it on the screen" },
          m_shared_resources{ shared_resources }
    {
        m_app.add_option("-d,--device", m_device_id, "ID of the device to use");
        m_app.add_option("-i,--input", m_stream_id, "ID of the input connector to use");
    }

    int VideoMonitorApp::run(int argc, char** argv)
    {

        CLI11_PARSE(m_app, argc, argv);

        std::cout << "VideoMaster video-monitor (" << VERSTRING << ")" << std::endl;

        try
        {
            std::cout << "VideoMaster API version: " << Deltacast::Wrapper::api_version()
                      << std::endl;
            std::cout << "Discovered " << Deltacast::Wrapper::Board::count() << " devices"
                      << std::endl;

            if (m_device_id >= Deltacast::Wrapper::Board::count())
            {
                std::cout << "Invalid device ID" << std::endl;
                return -1;
            }

            std::cout << "Opening device " << m_device_id << std::endl;
            auto board = Deltacast::Wrapper::Board::open(
                m_device_id, [this](Deltacast::Wrapper::Board& board)
                { Deltacast::VideoMonitor::Helper::enable_loopback(board, m_stream_id); });

            std::cout << board << std::endl;

            Deltacast::VideoMonitor::Helper::disable_loopback(board, m_stream_id);

            while (!m_shared_resources.stop_is_requested)
            {
                m_shared_resources.reset();

                std::cout << "Opening RX" << m_stream_id << " stream..." << std::endl;
                auto rx_tech_stream = Deltacast::VideoMonitor::Helper::open_stream(
                    board, Deltacast::VideoMonitor::Helper::rx_index_to_streamtype(m_stream_id));
                auto& rx_stream = Deltacast::VideoMonitor::Helper::to_base_stream(rx_tech_stream);

                std::cout << "Waiting for signal..." << std::endl;
                if (!Deltacast::VideoMonitor::Helper::wait_for_input(
                        board.rx(m_stream_id), m_shared_resources.stop_is_requested))
                {
                    std::cerr << "Application has been stopped before any input was received."
                              << std::endl;
                    return -1;
                }

                auto signal_information = Deltacast::VideoMonitor::Helper::detect_information(
                    rx_tech_stream);
                auto video_characteristics =
                    Deltacast::VideoMonitor::Helper::get_video_characteristics(signal_information);
                std::cout << "Detected:" << std::endl;
                Deltacast::VideoMonitor::Helper::print_information(signal_information, "\t");

                rx_stream.buffer_queue().set_depth(8);
                rx_stream.set_buffer_packing(VHD_BUFPACK_VIDEO_YUV422_8);
                Deltacast::VideoMonitor::Helper::configure_stream(rx_tech_stream,
                                                                  signal_information);

                auto             window_refresh_interval = 10ms;
                WindowedRenderer renderer("Live Content", video_characteristics.width / 2,
                                          video_characteristics.height / 2,
                                          window_refresh_interval.count(),
                                          m_shared_resources.stop_is_requested);
                std::cout << "Initializing live content rendering window..." << std::endl;
                renderer.init(video_characteristics.width, video_characteristics.height,
                              Deltacast::VideoViewer::InputFormat::ycbcr_422_8);

                std::cout << std::endl;

                std::cout << "Starting RX stream..." << std::endl;
                rx_stream.start();

                while (!m_shared_resources.stop_is_requested &&
                       !m_shared_resources.incoming_signal_changed)
                {
                    if (!Deltacast::VideoMonitor::Helper::wait_for_input(
                            board.rx(m_stream_id), m_shared_resources.stop_is_requested))
                    {
                        std::this_thread::sleep_for(100ms);
                        continue;
                    }

                    if (Deltacast::VideoMonitor::Helper::detect_information(rx_tech_stream) !=
                        signal_information)
                    {
                        m_shared_resources.incoming_signal_changed = true;
                        continue;
                    }

                    {
                        auto slot = rx_stream.pop_slot();
                        auto [buffer, buffer_size] = slot->video().buffer();

                        renderer.render_buffer(buffer, buffer_size);
                    }

                    std::cout << "Slots count: " << rx_stream.buffer_queue().slots_count()
                              << " (dropped: " << rx_stream.buffer_queue().slots_dropped() << ")"
                              << "\r";
                }

                std::cout << std::endl;
            }
        }
        catch (const Deltacast::Wrapper::ApiException& e)
        {
            std::cerr << e.what() << std::endl;
            std::cerr << e.logs() << std::endl;
            return -1;
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            return -1;
        }
        return 0;
    }
}  // namespace Deltacast::VideoMonitor