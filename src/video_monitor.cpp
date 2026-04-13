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

#include <CLI/CLI.hpp>
#include <VideoMasterCppApi/api.hpp>
#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/exception.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

namespace Deltacast::VideoMonitor
{
    namespace
    {
        constexpr auto log_pattern = "[%Y-%b-%d %T.%e] [%l] %v";
        constexpr auto log_file_name = "video_monitor.log";

    }  // namespace
    VideoMonitorApp::VideoMonitorApp(SharedResources& shared_resources)
        : m_app{ "Identify an incoming signal and display it on the screen" },
          m_shared_resources{ shared_resources }
    {
        init_cli();
    }

    bool VideoMonitorApp::check_device_id()
    {
        if (m_device_id >= Deltacast::Wrapper::Board::count())
        {
            spdlog::error("Invalid device ID");
            return false;
        }
        return true;
    }

    int VideoMonitorApp::run(int argc, char** argv)
    {
        CLI11_PARSE(m_app, argc, argv);

        init_log();

        spdlog::trace("VideoMaster video-monitor ({})", VERSTRING);

        spdlog::debug("VideoMaster API version: {}", Deltacast::Wrapper::api_version());
        spdlog::trace("Discovered {} devices", Deltacast::Wrapper::Board::count());

        if (!check_device_id())
            return static_cast<int>(ExitCode::FailureUnexpected);

        spdlog::debug("Opening device {}", m_device_id);
        auto board = Deltacast::Wrapper::Board::open(
            m_device_id, [this](Deltacast::Wrapper::Board& board)
            { Deltacast::VideoMonitor::Helper::enable_loopback(board, m_stream_id); });
        spdlog::trace("Opened device {}", m_device_id);

        if (m_sdp_file_path.has_value())
        {
            spdlog::debug("SDP file has been provided, board should be an IP board");
            if (board.has_ip())
            {
                spdlog::trace("Board has IP capabilities");
            }
            else
            {
                spdlog::error("Board does not have IP capability");
                return static_cast<int>(ExitCode::FailureUnexpected);
            }
        }
        else
        {
            Deltacast::VideoMonitor::Helper::disable_loopback(board, m_stream_id);
        }

        while (!m_shared_resources.stop_is_requested)
        {
            m_shared_resources.reset();

            spdlog::debug("Opening RX{} stream...", m_stream_id);
            auto rx_tech_stream = Deltacast::VideoMonitor::Helper::open_stream(
                board, Deltacast::VideoMonitor::Helper::rx_index_to_streamtype(m_stream_id));
            auto& rx_stream = Deltacast::VideoMonitor::Helper::to_base_stream(rx_tech_stream);

            spdlog::trace("Waiting for signal...");
            if (!Deltacast::VideoMonitor::Helper::wait_for_input(
                    board.rx(m_stream_id), m_shared_resources.stop_is_requested))
            {
                spdlog::error("Application has been stopped before any input was received.");
                return static_cast<int>(ExitCode::StopRequestedBeforeSignal);
            }

            auto signal_information = Deltacast::VideoMonitor::Helper::detect_information(
                rx_tech_stream);
            auto video_characteristics = Deltacast::VideoMonitor::Helper::get_video_characteristics(
                signal_information);
            spdlog::info("Detected: {}",
                         Deltacast::VideoMonitor::Helper::get_information_string(signal_information,
                                                                                 "\t"));

            rx_stream.buffer_queue().set_depth(8);
            rx_stream.set_buffer_packing(VHD_BUFPACK_VIDEO_YUV422_8);
            Deltacast::VideoMonitor::Helper::configure_stream(rx_tech_stream, signal_information);

            auto             window_refresh_interval = 10ms;
            WindowedRenderer renderer("Live Content", video_characteristics.width / 2,
                                      video_characteristics.height / 2,
                                      window_refresh_interval.count(),
                                      m_shared_resources.stop_is_requested);
            spdlog::trace("Initializing live content rendering window...");
            renderer.init(video_characteristics.width, video_characteristics.height,
                          Deltacast::VideoViewer::InputFormat::ycbcr_422_8);

            spdlog::trace("Starting RX stream...");
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

                spdlog::trace("Slots count: {} (dropped: {})",
                              rx_stream.buffer_queue().slots_count(),
                              rx_stream.buffer_queue().slots_dropped());
            }

            // Check if renderer thread had an exception
            if (auto renderer_exception = renderer.get_thread_exception())
            {
                spdlog::error("Renderer thread encountered an error, rethrowing");
                std::rethrow_exception(renderer_exception);
            }
        }
        return static_cast<int>(ExitCode::Success);
    }

    void VideoMonitorApp::init_cli()
    {
        m_app.set_version_flag("-v,--version", VERSTRING);

        m_app
            .add_option("--log-level,-l", m_log_level,
                        "Log level: trace, debug, info, warn, error, critical, off")
            ->check(CLI::IsMember({ "trace", "debug", "info", "warn", "error", "critical", "off" }))
            ->capture_default_str()
            ->default_str("info");
        m_app.add_option("--log-directory", m_log_directory, "Directory for the log file")
            ->check(CLI::ExistingDirectory)
            ->default_str(".")
            ->capture_default_str();

        auto* common_option_group = m_app.add_option_group("Common options");
        common_option_group->add_option("-d,--device", m_device_id, "ID of the device to use")
            ->check(CLI::NonNegativeNumber);
        common_option_group
            ->add_option("-i,--input", m_stream_id, "ID of the input connector to use")
            ->check(CLI::NonNegativeNumber);

        auto* ip_board_option_group = m_app.add_option_group("IP board options");
        ip_board_option_group
            ->add_option("--sdp-file,-s", m_sdp_file_path, "Path to save the SDP file for IP input")
            ->check(CLI::ExistingFile)
            ->capture_default_str();
    }

    void VideoMonitorApp::init_log()
    {
        auto log_level = spdlog::level::from_str(m_log_level);
        auto log_file = (m_log_directory / log_file_name).string();
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
        file_sink->set_pattern(log_pattern);
        if (log_level < spdlog::level::info)
            console_sink->set_pattern(log_pattern);
        else
            console_sink->set_pattern("%v");
        spdlog::sinks_init_list sinks = { console_sink, file_sink };
        auto                    logger = std::make_shared<spdlog::logger>("multi_sink", sinks);
        logger->set_level(log_level);
        spdlog::set_default_logger(logger);
    }
}  // namespace Deltacast::VideoMonitor