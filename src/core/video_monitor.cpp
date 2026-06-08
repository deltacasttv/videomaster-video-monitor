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
#include "exceptions.hpp"
#include "input_session_factory.hpp"
#include "ip_cli_configuration_builder.hpp"
#include "shared_resources.hpp"
#include "version.hpp"
#include "windowed_renderer.hpp"

#include <CLI/CLI.hpp>
#include <VideoMasterCppApi/api.hpp>
#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/exception.hpp>
#include <chrono>
#include <exception>
#include <filesystem>
#include <memory>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <videoviewer/videoviewer.hpp>

using namespace std::chrono_literals;

namespace Deltacast::VideoMonitor
{
    namespace
    {
        constexpr auto log_pattern = "[%Y-%b-%d %T.%e] [%l] %v";
        constexpr auto log_file_name = "video_monitor.log";
        constexpr auto window_refresh_interval = 10ms;

    }  // namespace
    VideoMonitorApp::VideoMonitorApp(SharedResources& shared_resources)
        : m_app{ "Identify an incoming signal and display it on the screen" },
          m_shared_resources{ shared_resources }
    {
        init_cli();
    }

    auto VideoMonitorApp::check_device_id() const -> bool
    {
        if (m_device_id >= Deltacast::Wrapper::Board::count())
        {
            spdlog::error("Invalid device ID, no board found at index {}", m_device_id);
            return false;
        }
        return true;
    }

    auto VideoMonitorApp::check_stream_id() const -> bool
    {
        auto board = Deltacast::Wrapper::Board::open(m_device_id);
        if (m_stream_id >= board.number_of_rx())
        {
            spdlog::error("Invalid stream ID, RX should be between 0 and {} for device {}",
                          board.number_of_rx() - 1, m_device_id);
            return false;
        }
        return true;
    }

    auto VideoMonitorApp::run(int argc, char** argv) -> int
    {
        CLI11_PARSE(m_app, argc, argv);

        const auto ip_network_options = Deltacast::VideoMonitor::Core::IpNetworkCliOptions{
            m_ip_dhcp,     m_gateway,        m_ip_address,   m_ip_subnet,
            m_ip_sps_dhcp, m_ip_sps_address, m_ip_sps_subnet
        };
        const auto ip_media_options = Deltacast::VideoMonitor::Core::IpMediaCliOptions{
            { m_ip_main_destination, m_ip_main_udp_port, m_ip_main_payload_type,
              m_ip_main_source_ip, m_ip_main_source_filter_mode, m_ip_main_source_filter_sources },
            { m_ip_sps_destination, m_ip_sps_udp_port, m_ip_sps_payload_type, m_ip_sps_source_ip,
              m_ip_sps_source_filter_mode, m_ip_sps_source_filter_sources },
            { m_ip_video_width, m_ip_video_height, m_ip_bit_depth, m_ip_framerate_numerator,
              m_ip_framerate_denominator }
        };

        Deltacast::VideoMonitor::Core::validate_explicit_ip_media_options(m_sdp_file_path,
                                                                          ip_media_options);

        init_log();

        spdlog::info("VideoMaster video-monitor ({})", VERSTRING);

        spdlog::trace("VideoMaster API version: {}", Deltacast::Wrapper::api_version());
        spdlog::trace("Discovered {} devices", Deltacast::Wrapper::Board::count());

        if (!check_device_id())
        {
            return static_cast<int>(ExitCode::FailureUnexpected);
        }

        if (!check_stream_id())
        {
            return static_cast<int>(ExitCode::FailureUnexpected);
        }

        auto session = Deltacast::VideoMonitor::Session::InputSessionFactory::create_input_session(
            m_device_id, m_stream_id, m_sdp_file_path,
            Deltacast::VideoMonitor::Core::build_ip_network_configuration(ip_network_options),
            Deltacast::VideoMonitor::Core::build_ip_media_configuration(ip_media_options),
            m_shared_resources);

        return run_session_loop(std::move(session));
    }

    auto VideoMonitorApp::run_session_loop(
        std::unique_ptr<Deltacast::VideoMonitor::Session::InputSessionBase> session) -> int
    {
        spdlog::trace("Starting monitor loop on device {} input {}", m_device_id, m_stream_id);

        spdlog::debug("Opening device {}", m_device_id);
        session->open_board();
        spdlog::trace("Opened device {}", m_device_id);

        while (!m_shared_resources.stop_is_requested)
        {
            spdlog::trace("Preparing capture cycle for RX{}", m_stream_id);
            m_shared_resources.reset();

            spdlog::debug("Opening RX{} stream...", m_stream_id);
            session->prepare_video_stream();
            session->configure_video_stream();
            spdlog::trace("RX{} stream opened and configured", m_stream_id);

            const auto& video_characteristics = session->get_video_characteristics();
            spdlog::info("Rendering {}x{} stream (interlaced={}, framerate={})",
                         video_characteristics.width, video_characteristics.height,
                         static_cast<bool>(video_characteristics.interlaced),
                         video_characteristics.framerate);

            Deltacast::VideoMonitor::Renderer::WindowedRenderer renderer(
                { "Live Content", static_cast<int>(video_characteristics.width / 2),
                  static_cast<int>(video_characteristics.height / 2),
                  static_cast<int>(window_refresh_interval.count()) },
                m_shared_resources.stop_is_requested);
            spdlog::trace("Initializing live content rendering window...");
            renderer.init(static_cast<int>(video_characteristics.width),
                          static_cast<int>(video_characteristics.height),
                          Deltacast::VideoViewer::InputFormat::ycbcr_422_8);

            spdlog::trace("Starting RX stream...");
            session->start_video_stream();

            while (!m_shared_resources.stop_is_requested &&
                   !m_shared_resources.incoming_signal_changed)
            {
                if (session->video_input_has_changed())
                {
                    spdlog::warn("Incoming signal changed on RX{}, restarting capture cycle",
                                 m_stream_id);
                    m_shared_resources.incoming_signal_changed = true;
                    continue;
                }

                try
                {
                    auto [buffer, buffer_size] = session->get_video_buffer();

                    renderer.render_buffer(buffer, buffer_size);
                }
                catch (Deltacast::Wrapper::RecoverableApiException& ex)
                {
                    spdlog::warn("Recoverable error while capturing video buffer: {}", ex.what());
                }
                catch (...)
                {
                    spdlog::error("Unexpected error while capturing video buffer");
                    throw;
                }

                {
                    auto [slots_count, slots_dropped] = session->get_video_slots_statistics();
                    spdlog::trace("Slots count: {} (dropped: {})", slots_count, slots_dropped);
                }
            }

            // Check if renderer thread had an exception
            if (auto renderer_exception = renderer.get_thread_exception())
            {
                spdlog::error("Renderer thread encountered an error, rethrowing");
                std::rethrow_exception(renderer_exception);
            }

            if (m_shared_resources.stop_is_requested)
            {
                spdlog::info("Stop requested, leaving monitor loop");
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

        init_common_options();
        init_ip_board_options();
    }

    void VideoMonitorApp::init_ip_board_options()
    {
        auto* ip_board_option_group = m_app.add_option_group("IP board options");
        ip_board_option_group
            ->add_option("--sdp-file", m_sdp_file_path, "Path to save the SDP file for IP input")
            ->check(CLI::ExistingFile)
            ->capture_default_str();
        auto* ip_dhcp_option = ip_board_option_group->add_flag("--ip-dhcp", m_ip_dhcp,
                                                               "Configure IP port through DHCP");
        auto* ip_gateway_option = ip_board_option_group->add_option(
            "--ip-gateway", m_gateway,
            "Static IPv4 gateway for IP input port (primary and SPS if SPS options are used)");
        auto* ip_address_option = ip_board_option_group->add_option(
            "--ip-address", m_ip_address, "Static IPv4 address for IP input port");
        auto* ip_subnet_option = ip_board_option_group->add_option(
            "--ip-subnet", m_ip_subnet, "Static IPv4 subnet mask for IP input port");
        auto* ip_sps_dhcp_option = ip_board_option_group->add_flag(
            "--ip-sps-dhcp", m_ip_sps_dhcp, "Configure SPS IP port through DHCP");
        auto* ip_sps_address_option = ip_board_option_group->add_option(
            "--ip-sps-address", m_ip_sps_address, "Static IPv4 address for SPS IP port");
        auto* ip_sps_subnet_option = ip_board_option_group->add_option(
            "--ip-sps-subnet", m_ip_sps_subnet, "Static IPv4 subnet mask for SPS IP port");

        auto* ip_main_destination_option =
            ip_board_option_group
                ->add_option("--ip-main-destination", m_ip_main_destination,
                             "Main stream destination IP address for explicit IP media mode")
                ->capture_default_str();
        auto* ip_main_udp_port_option =
            ip_board_option_group
                ->add_option("--ip-main-udp-port", m_ip_main_udp_port,
                             "Main stream destination UDP port for explicit IP media mode")
                ->check(CLI::Range(1, 65535));  // NOLINT(readability-magic-numbers)
        auto* ip_main_payload_type_option =
            ip_board_option_group
                ->add_option("--ip-main-payload-type", m_ip_main_payload_type,
                             "Main stream RTP payload type for explicit IP media mode")
                ->check(CLI::Range(0, 127));  // NOLINT(readability-magic-numbers)
        auto* ip_main_source_ip_option =
            ip_board_option_group
                ->add_option("--ip-main-source-ip", m_ip_main_source_ip,
                             "Main stream source IP address for unicast filtering")
                ->capture_default_str();
        auto* ip_main_filter_mode_option =
            ip_board_option_group
                ->add_option("--ip-main-source-filter-mode", m_ip_main_source_filter_mode,
                             "Main stream source filter mode: include or exclude")
                ->check(CLI::IsMember({ "include", "exclude" }))
                ->capture_default_str();
        auto* ip_main_filter_sources_option =
            ip_board_option_group
                ->add_option("--ip-main-source-filter-sources", m_ip_main_source_filter_sources,
                             "Main stream source filter IP list (comma-separated)")
                ->delimiter(',')
                ->capture_default_str();

        auto* ip_sps_destination_option =
            ip_board_option_group
                ->add_option("--ip-sps-destination", m_ip_sps_destination,
                             "SPS stream destination IP address for explicit IP media mode")
                ->capture_default_str();
        auto* ip_sps_udp_port_option =
            ip_board_option_group
                ->add_option("--ip-sps-udp-port", m_ip_sps_udp_port,
                             "SPS stream destination UDP port for explicit IP media mode")
                ->check(CLI::Range(1, 65535));  // NOLINT(readability-magic-numbers)
        auto* ip_sps_payload_type_option =
            ip_board_option_group
                ->add_option("--ip-sps-payload-type", m_ip_sps_payload_type,
                             "SPS stream RTP payload type for explicit IP media mode")
                ->check(CLI::Range(0, 127));  // NOLINT(readability-magic-numbers)
        auto* ip_sps_source_ip_option =
            ip_board_option_group
                ->add_option("--ip-sps-source-ip", m_ip_sps_source_ip,
                             "SPS stream source IP address for unicast filtering")
                ->capture_default_str();
        auto* ip_sps_filter_mode_option =
            ip_board_option_group
                ->add_option("--ip-sps-source-filter-mode", m_ip_sps_source_filter_mode,
                             "SPS stream source filter mode: include or exclude")
                ->check(CLI::IsMember({ "include", "exclude" }))
                ->capture_default_str();
        auto* ip_sps_filter_sources_option =
            ip_board_option_group
                ->add_option("--ip-sps-source-filter-sources", m_ip_sps_source_filter_sources,
                             "SPS stream source filter IP list (comma-separated)")
                ->delimiter(',')
                ->capture_default_str();

        auto* ip_video_width_option =
            ip_board_option_group
                ->add_option("--ip-video-width", m_ip_video_width,
                             "IP stream video width for explicit IP media mode")
                ->check(CLI::PositiveNumber);
        auto* ip_video_height_option =
            ip_board_option_group
                ->add_option("--ip-video-height", m_ip_video_height,
                             "IP stream video height for explicit IP media mode")
                ->check(CLI::PositiveNumber);
        auto* ip_bit_depth_option =
            ip_board_option_group
                ->add_option("--ip-bit-depth", m_ip_bit_depth,
                             "IP stream bit depth for explicit IP media mode")
                ->check(CLI::PositiveNumber);
        auto* ip_framerate_num_option =
            ip_board_option_group
                ->add_option("--ip-framerate-num", m_ip_framerate_numerator,
                             "IP stream framerate numerator for explicit IP media mode")
                ->check(CLI::PositiveNumber);
        auto* ip_framerate_den_option =
            ip_board_option_group
                ->add_option("--ip-framerate-den", m_ip_framerate_denominator,
                             "IP stream framerate denominator for explicit IP media mode")
                ->check(CLI::PositiveNumber);

        ip_address_option->needs(ip_subnet_option);
        ip_subnet_option->needs(ip_address_option);

        ip_sps_address_option->needs(ip_sps_subnet_option);
        ip_sps_subnet_option->needs(ip_sps_address_option);

        ip_dhcp_option->excludes(ip_address_option);
        ip_dhcp_option->excludes(ip_subnet_option);
        ip_dhcp_option->excludes(ip_gateway_option);

        ip_sps_dhcp_option->excludes(ip_sps_address_option);
        ip_sps_dhcp_option->excludes(ip_sps_subnet_option);
        ip_sps_dhcp_option->excludes(ip_gateway_option);

        ip_main_destination_option->needs(ip_video_width_option);
        ip_main_destination_option->needs(ip_video_height_option);
        ip_main_destination_option->needs(ip_bit_depth_option);
        ip_main_destination_option->needs(ip_framerate_num_option);
        ip_main_destination_option->needs(ip_framerate_den_option);

        ip_main_udp_port_option->needs(ip_video_width_option);
        ip_main_udp_port_option->needs(ip_video_height_option);
        ip_main_udp_port_option->needs(ip_bit_depth_option);
        ip_main_udp_port_option->needs(ip_framerate_num_option);
        ip_main_udp_port_option->needs(ip_framerate_den_option);

        ip_main_payload_type_option->needs(ip_video_width_option);
        ip_main_payload_type_option->needs(ip_video_height_option);
        ip_main_payload_type_option->needs(ip_bit_depth_option);
        ip_main_payload_type_option->needs(ip_framerate_num_option);
        ip_main_payload_type_option->needs(ip_framerate_den_option);

        ip_sps_destination_option->needs(ip_video_width_option);
        ip_sps_destination_option->needs(ip_video_height_option);
        ip_sps_destination_option->needs(ip_bit_depth_option);
        ip_sps_destination_option->needs(ip_framerate_num_option);
        ip_sps_destination_option->needs(ip_framerate_den_option);

        ip_sps_udp_port_option->needs(ip_video_width_option);
        ip_sps_udp_port_option->needs(ip_video_height_option);
        ip_sps_udp_port_option->needs(ip_bit_depth_option);
        ip_sps_udp_port_option->needs(ip_framerate_num_option);
        ip_sps_udp_port_option->needs(ip_framerate_den_option);

        ip_sps_payload_type_option->needs(ip_video_width_option);
        ip_sps_payload_type_option->needs(ip_video_height_option);
        ip_sps_payload_type_option->needs(ip_bit_depth_option);
        ip_sps_payload_type_option->needs(ip_framerate_num_option);
        ip_sps_payload_type_option->needs(ip_framerate_den_option);

        ip_main_source_ip_option->needs(ip_video_width_option);
        ip_main_source_ip_option->needs(ip_video_height_option);
        ip_main_source_ip_option->needs(ip_bit_depth_option);
        ip_main_source_ip_option->needs(ip_framerate_num_option);
        ip_main_source_ip_option->needs(ip_framerate_den_option);

        ip_sps_source_ip_option->needs(ip_video_width_option);
        ip_sps_source_ip_option->needs(ip_video_height_option);
        ip_sps_source_ip_option->needs(ip_bit_depth_option);
        ip_sps_source_ip_option->needs(ip_framerate_num_option);
        ip_sps_source_ip_option->needs(ip_framerate_den_option);

        ip_main_filter_mode_option->needs(ip_main_filter_sources_option);
        ip_main_filter_mode_option->needs(ip_main_destination_option);
        ip_main_filter_sources_option->needs(ip_main_filter_mode_option);
        ip_main_filter_sources_option->needs(ip_main_destination_option);

        ip_sps_filter_mode_option->needs(ip_sps_filter_sources_option);
        ip_sps_filter_mode_option->needs(ip_sps_destination_option);
        ip_sps_filter_sources_option->needs(ip_sps_filter_mode_option);
        ip_sps_filter_sources_option->needs(ip_sps_destination_option);

        ip_video_width_option->needs(ip_video_height_option);
        ip_video_width_option->needs(ip_bit_depth_option);
        ip_video_width_option->needs(ip_framerate_num_option);
        ip_video_width_option->needs(ip_framerate_den_option);

        ip_video_height_option->needs(ip_video_width_option);
        ip_video_height_option->needs(ip_bit_depth_option);
        ip_video_height_option->needs(ip_framerate_num_option);
        ip_video_height_option->needs(ip_framerate_den_option);

        ip_bit_depth_option->needs(ip_video_width_option);
        ip_bit_depth_option->needs(ip_video_height_option);
        ip_bit_depth_option->needs(ip_framerate_num_option);
        ip_bit_depth_option->needs(ip_framerate_den_option);

        ip_framerate_num_option->needs(ip_video_width_option);
        ip_framerate_num_option->needs(ip_video_height_option);
        ip_framerate_num_option->needs(ip_bit_depth_option);
        ip_framerate_num_option->needs(ip_framerate_den_option);

        ip_framerate_den_option->needs(ip_video_width_option);
        ip_framerate_den_option->needs(ip_video_height_option);
        ip_framerate_den_option->needs(ip_bit_depth_option);
        ip_framerate_den_option->needs(ip_framerate_num_option);
    }

    void VideoMonitorApp::init_common_options()
    {
        auto* common_option_group = m_app.add_option_group("Common options");
        common_option_group->add_option("-d,--device", m_device_id, "ID of the device to use")
            ->check(CLI::NonNegativeNumber);
        common_option_group
            ->add_option("-i,--input", m_stream_id, "ID of the input connector to use")
            ->check(CLI::NonNegativeNumber);
    }

    void VideoMonitorApp::init_log()
    {
        auto log_level = spdlog::level::from_str(m_log_level);
        auto log_file = (m_log_directory / log_file_name).string();
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
        file_sink->set_pattern(log_pattern);
        if (log_level < spdlog::level::info)
        {
            console_sink->set_pattern(log_pattern);
        }
        else
        {
            console_sink->set_pattern("%v");
        }
        spdlog::sinks_init_list sinks = { console_sink, file_sink };
        auto                    logger = std::make_shared<spdlog::logger>("multi_sink", sinks);
        logger->set_level(log_level);
        spdlog::set_default_logger(logger);
    }
}  // namespace Deltacast::VideoMonitor