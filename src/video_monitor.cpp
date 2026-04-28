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
#include "ip_input_session.hpp"
#include "shared_resources.hpp"
#include "version.hpp"
#include "windowed_renderer.hpp"

#include <CLI/CLI.hpp>
#include <VideoMasterCppApi/api.hpp>
#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/exception.hpp>
#include <bits/chrono.h>
#include <chrono>
#include <exception>
#include <fmt/format.h>
#include <ipaddress/ipaddress.hpp>
#include <ipaddress/ipv4-address.hpp>
#include <memory>
#include <optional>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <videoviewer/videoviewer.hpp>

using namespace std::chrono_literals;

namespace Deltacast::VideoMonitor
{
    namespace
    {
        constexpr auto log_pattern = "[%Y-%b-%d %T.%e] [%l] %v";
        constexpr auto log_file_name = "video_monitor.log";
        constexpr auto window_refresh_interval = 10ms;

        auto parse_ipv4(const std::string& address,
                        const std::string& option_name) -> ipaddress::ipv4_address
        {
            try
            {
                return ipaddress::ipv4_address::parse(address);
            }
            catch (const std::exception& ex)
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    fmt::format("Invalid IPv4 address for {}: {} ({})", option_name, address,
                                ex.what()));
            }
        }

        auto build_ip_network_configuration(std::optional<bool>               dhcp_requested,
                                            const std::optional<std::string>& ip_address,
                                            const std::optional<std::string>& subnet_mask,
                                            const std::optional<std::string>& gateway,
                                            std::optional<bool>               sps_dhcp_requested,
                                            const std::optional<std::string>& sps_ip_address,
                                            const std::optional<std::string>& sps_subnet_mask,
                                            const std::optional<std::string>& sps_gateway)
            -> std::optional<Deltacast::VideoMonitor::Session::IpNetworkConfiguration>
        {
            const bool has_static_parameter = ip_address.has_value() || subnet_mask.has_value() ||
                                              gateway.has_value();
            const bool has_sps_static_parameter = sps_ip_address.has_value() ||
                                                  sps_subnet_mask.has_value() ||
                                                  sps_gateway.has_value();
            const bool has_sps_parameter = sps_dhcp_requested.has_value() ||
                                           has_sps_static_parameter;

            if (dhcp_requested.has_value() && dhcp_requested.value() && has_static_parameter)
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    "Options --ip-dhcp and --ip-address/--ip-subnet/--ip-gateway are mutually "
                    "exclusive");
            }

            if (has_static_parameter &&
                !(ip_address.has_value() && subnet_mask.has_value() && gateway.has_value()))
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    "Static IP mode requires --ip-address, --ip-subnet and --ip-gateway");
            }

            if (sps_dhcp_requested.has_value() && sps_dhcp_requested.value() &&
                has_sps_static_parameter)
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    "Options --ip-sps-dhcp and --ip-sps-address/--ip-sps-subnet/--ip-sps-gateway "
                    "are mutually exclusive");
            }

            if (has_sps_static_parameter &&
                !(sps_ip_address.has_value() && sps_subnet_mask.has_value() &&
                  sps_gateway.has_value()))
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    "Static SPS IP mode requires --ip-sps-address, --ip-sps-subnet and "
                    "--ip-sps-gateway");
            }

            if (!has_static_parameter && !dhcp_requested.has_value())
            {
                return std::nullopt;
            }

            auto config = Deltacast::VideoMonitor::Session::IpNetworkConfiguration{};
            config.has_sps = has_sps_parameter;

            if (has_static_parameter)
            {
                config.mode = Deltacast::VideoMonitor::Session::IpNetworkMode::Static;
                config.ip_address_v4 = parse_ipv4(ip_address.value(), "--ip-address");
                config.subnet_mask_v4 = parse_ipv4(subnet_mask.value(), "--ip-subnet");
                config.gateway_v4 = parse_ipv4(gateway.value(), "--ip-gateway");
            }
            else if (dhcp_requested.has_value() && dhcp_requested.value())
            {
                config.mode = Deltacast::VideoMonitor::Session::IpNetworkMode::Dhcp;
            }

            if (has_sps_static_parameter)
            {
                config.sps_mode = Deltacast::VideoMonitor::Session::IpNetworkMode::Static;
                config.sps_ip_address_v4 = parse_ipv4(sps_ip_address.value(), "--ip-sps-address");
                config.sps_subnet_mask_v4 = parse_ipv4(sps_subnet_mask.value(), "--ip-sps-subnet");
                config.sps_gateway_v4 = parse_ipv4(sps_gateway.value(), "--ip-sps-gateway");
            }
            else if (sps_dhcp_requested.has_value() && sps_dhcp_requested.value())
            {
                config.sps_mode = Deltacast::VideoMonitor::Session::IpNetworkMode::Dhcp;
            }

            return config;
        }

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
            spdlog::error("Invalid device ID");
            return false;
        }
        return true;
    }

    auto VideoMonitorApp::run(int argc, char** argv) -> int
    {
        CLI11_PARSE(m_app, argc, argv);

        init_log();

        spdlog::info("VideoMaster video-monitor ({})", VERSTRING);

        spdlog::trace("VideoMaster API version: {}", Deltacast::Wrapper::api_version());
        spdlog::trace("Discovered {} devices", Deltacast::Wrapper::Board::count());

        if (!check_device_id())
        {
            return static_cast<int>(ExitCode::FailureUnexpected);
        }

        auto session = Deltacast::VideoMonitor::Session::InputSessionFactory::create_input_session(
            m_device_id, m_stream_id, m_sdp_file_path,
            build_ip_network_configuration(m_ip_dhcp, m_ip_address, m_ip_subnet, m_ip_gateway,
                                           m_ip_sps_dhcp, m_ip_sps_address, m_ip_sps_subnet,
                                           m_ip_sps_gateway),
            m_shared_resources);

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

                {
                    auto [buffer, buffer_size] = session->get_video_buffer();

                    renderer.render_buffer(buffer, buffer_size);
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

        auto* common_option_group = m_app.add_option_group("Common options");
        common_option_group->add_option("-d,--device", m_device_id, "ID of the device to use")
            ->check(CLI::NonNegativeNumber);
        common_option_group
            ->add_option("-i,--input", m_stream_id, "ID of the input connector to use")
            ->check(CLI::NonNegativeNumber);

        auto* ip_board_option_group = m_app.add_option_group("IP board options");
        ip_board_option_group
            ->add_option("--sdp-file", m_sdp_file_path, "Path to save the SDP file for IP input")
            ->check(CLI::ExistingFile)
            ->capture_default_str();
        ip_board_option_group->add_flag("--ip-dhcp", m_ip_dhcp, "Configure IP port through DHCP");
        ip_board_option_group->add_option("--ip-address", m_ip_address,
                                          "Static IPv4 address for IP input port");
        ip_board_option_group->add_option("--ip-subnet", m_ip_subnet,
                                          "Static IPv4 subnet mask for IP input port");
        ip_board_option_group->add_option("--ip-gateway", m_ip_gateway,
                                          "Static IPv4 gateway for IP input port");
        ip_board_option_group->add_flag("--ip-sps-dhcp", m_ip_sps_dhcp,
                                        "Configure SPS IP port through DHCP");
        ip_board_option_group->add_option("--ip-sps-address", m_ip_sps_address,
                                          "Static IPv4 address for SPS IP port");
        ip_board_option_group->add_option("--ip-sps-subnet", m_ip_sps_subnet,
                                          "Static IPv4 subnet mask for SPS IP port");
        ip_board_option_group->add_option("--ip-sps-gateway", m_ip_sps_gateway,
                                          "Static IPv4 gateway for SPS IP port");
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