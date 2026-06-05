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
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fmt/format.h>
#include <ipaddress/ip-any-address.hpp>
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
#include <vector>
#include <videoviewer/videoviewer.hpp>

using namespace std::chrono_literals;

namespace Deltacast::VideoMonitor
{
    namespace
    {
        constexpr auto log_pattern = "[%Y-%b-%d %T.%e] [%l] %v";
        constexpr auto log_file_name = "video_monitor.log";
        constexpr auto window_refresh_interval = 10ms;

        auto parse_ipv4(const std::string& address, const std::string& option_name)
            -> ipaddress::ipv4_address
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

        auto parse_ip_address(const std::string& address, const std::string& option_name)
            -> ipaddress::ip_address
        {
            try
            {
                return ipaddress::ip_address::parse(address);
            }
            catch (const std::exception& ex)
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    fmt::format("Invalid IP address for {}: {} ({})", option_name, address,
                                ex.what()));
            }
        }

        auto has_explicit_media_option(const std::optional<std::string>& destination,
                                       const std::optional<uint16_t>&    udp_port,
                                       const std::optional<uint32_t>&    video_width,
                                       const std::optional<uint32_t>&    video_height,
                                       const std::optional<uint32_t>&    bit_depth,
                                       const std::optional<uint32_t>&    framerate_numerator,
                                       const std::optional<uint32_t>&    framerate_denominator,
                                       const std::optional<std::string>& source_ip,
                                       const std::optional<std::string>& source_filter_mode,
                                       const std::vector<std::string>&   source_filter_sources)
            -> bool
        {
            return destination.has_value() || udp_port.has_value() || video_width.has_value() ||
                   video_height.has_value() || bit_depth.has_value() ||
                   framerate_numerator.has_value() || framerate_denominator.has_value() ||
                   source_ip.has_value() || source_filter_mode.has_value() ||
                   !source_filter_sources.empty();
        }

        auto has_explicit_media_core(const std::optional<uint32_t>& video_width,
                                     const std::optional<uint32_t>& video_height,
                                     const std::optional<uint32_t>& bit_depth,
                                     const std::optional<uint32_t>& framerate_numerator,
                                     const std::optional<uint32_t>& framerate_denominator) -> bool
        {
            return video_width.has_value() && video_height.has_value() && bit_depth.has_value() &&
                   framerate_numerator.has_value() && framerate_denominator.has_value();
        }

        auto validate_source_filter_options(const std::optional<std::string>& destination,
                                            const std::optional<std::string>& source_filter_mode,
                                            const std::vector<std::string>&   source_filter_sources,
                                            const std::string& destination_option_name,
                                            const std::string& source_filter_mode_option,
                                            const std::string& source_filter_sources_option) -> void
        {
            if (source_filter_mode.has_value() && source_filter_sources.empty())
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    fmt::format("{} requires at least one value in {}", source_filter_mode_option,
                                source_filter_sources_option));
            }

            if (!source_filter_mode.has_value() && !source_filter_sources.empty())
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    fmt::format("{} requires {}", source_filter_sources_option,
                                source_filter_mode_option));
            }

            if ((source_filter_mode.has_value() || !source_filter_sources.empty()) &&
                !destination.has_value())
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    fmt::format("{} requires {}", source_filter_mode_option,
                                destination_option_name));
            }
        }

        auto validate_explicit_ip_media_options(
            const std::optional<std::filesystem::path>& sdp_file_path,
            const std::optional<std::string>&           main_destination,
            const std::optional<uint16_t>&              main_udp_port,
            const std::optional<std::string>&           main_source_ip,
            const std::optional<std::string>&           main_source_filter_mode,
            const std::vector<std::string>&             main_source_filter_sources,
            const std::optional<std::string>&           sps_destination,
            const std::optional<uint16_t>&              sps_udp_port,
            const std::optional<std::string>&           sps_source_ip,
            const std::optional<std::string>&           sps_source_filter_mode,
            const std::vector<std::string>&             sps_source_filter_sources,
            const std::optional<uint32_t>& video_width, const std::optional<uint32_t>& video_height,
            const std::optional<uint32_t>& bit_depth,
            const std::optional<uint32_t>& framerate_numerator,
            const std::optional<uint32_t>& framerate_denominator) -> void
        {
            const bool has_main_explicit_option = has_explicit_media_option(
                main_destination, main_udp_port, video_width, video_height, bit_depth,
                framerate_numerator, framerate_denominator, main_source_ip, main_source_filter_mode,
                main_source_filter_sources);
            const bool has_sps_explicit_option = has_explicit_media_option(
                sps_destination, sps_udp_port, video_width, video_height, bit_depth,
                framerate_numerator, framerate_denominator, sps_source_ip, sps_source_filter_mode,
                sps_source_filter_sources);

            if (!has_main_explicit_option && !has_sps_explicit_option)
            {
                return;
            }

            if (sdp_file_path.has_value())
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    "Explicit IP media options cannot be combined with --sdp-file");
            }

            if (has_sps_explicit_option && !has_main_explicit_option)
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    "SPS media options require main media options");
            }

            if (!has_explicit_media_core(video_width, video_height, bit_depth, framerate_numerator,
                                         framerate_denominator))
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    "Explicit main media mode requires --ip-main-video-width, "
                    "--ip-main-video-height, --ip-main-bit-depth, --ip-main-framerate-num and "
                    "--ip-main-framerate-den");
            }

            if (has_sps_explicit_option &&
                !has_explicit_media_core(video_width, video_height, bit_depth, framerate_numerator,
                                         framerate_denominator))
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    "Explicit SPS media mode requires --ip-sps-video-width, "
                    "--ip-sps-video-height, --ip-sps-bit-depth, --ip-sps-framerate-num and "
                    "--ip-sps-framerate-den");
            }

            if (main_destination.has_value())
            {
                parse_ip_address(main_destination.value(), "--ip-main-destination");
            }
            if (main_source_ip.has_value())
            {
                parse_ip_address(main_source_ip.value(), "--ip-main-source-ip");
            }

            validate_source_filter_options(main_destination, main_source_filter_mode,
                                           main_source_filter_sources, "--ip-main-destination",
                                           "--ip-main-source-filter-mode",
                                           "--ip-main-source-filter-sources");

            if (sps_destination.has_value())
            {
                parse_ip_address(sps_destination.value(), "--ip-sps-destination");
            }
            if (sps_source_ip.has_value())
            {
                parse_ip_address(sps_source_ip.value(), "--ip-sps-source-ip");
            }

            validate_source_filter_options(sps_destination, sps_source_filter_mode,
                                           sps_source_filter_sources, "--ip-sps-destination",
                                           "--ip-sps-source-filter-mode",
                                           "--ip-sps-source-filter-sources");

            for (const auto& source_ip : main_source_filter_sources)
            {
                parse_ip_address(source_ip, "--ip-main-source-filter-sources");
            }

            for (const auto& source_ip : sps_source_filter_sources)
            {
                parse_ip_address(source_ip, "--ip-sps-source-filter-sources");
            }
        }

        auto build_ip_network_configuration(std::optional<bool>               dhcp_requested,
                                            const std::optional<std::string>& gateway,
                                            const std::optional<std::string>& ip_address,
                                            const std::optional<std::string>& subnet_mask,
                                            std::optional<bool>               sps_dhcp_requested,
                                            const std::optional<std::string>& sps_ip_address,
                                            const std::optional<std::string>& sps_subnet_mask)
            -> std::optional<Deltacast::VideoMonitor::Session::IpNetworkConfiguration>
        {
            const bool has_main_dhcp_parameter = dhcp_requested.value_or(false);
            const bool has_sps_dhcp_parameter = sps_dhcp_requested.value_or(false);
            const bool has_main_static_parameter = ip_address.has_value() &&
                                                   subnet_mask.has_value();
            const bool has_sps_static_parameter = sps_ip_address.has_value() &&
                                                  sps_subnet_mask.has_value();
            const bool has_gateway_parameter = gateway.has_value();

            const bool has_main_parameter = has_main_dhcp_parameter || has_main_static_parameter;
            const bool has_sps_parameter = has_sps_dhcp_parameter || has_sps_static_parameter;

            if (has_sps_parameter && !has_main_parameter)
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    "SPS IP configuration requires main IP configuration");
            }

            if (!has_main_parameter && !has_sps_parameter && !has_gateway_parameter)
            {
                return std::nullopt;
            }

            auto config = Deltacast::VideoMonitor::Session::IpNetworkConfiguration{};
            config.has_main = has_main_parameter;
            config.has_gateway = has_gateway_parameter;
            config.has_sps = has_sps_parameter;

            if (has_main_static_parameter)
            {
                config.mode = Deltacast::VideoMonitor::Session::IpNetworkMode::Static;
                config.ip_address_v4 = parse_ipv4(ip_address.value(), "--ip-address");
                config.subnet_mask_v4 = parse_ipv4(subnet_mask.value(), "--ip-subnet");
            }
            else if (has_main_dhcp_parameter)
            {
                config.mode = Deltacast::VideoMonitor::Session::IpNetworkMode::Dhcp;
            }

            if (has_gateway_parameter)
            {
                config.gateway_v4 = parse_ipv4(gateway.value(), "--ip-gateway");
            }

            if (has_sps_static_parameter)
            {
                config.sps_mode = Deltacast::VideoMonitor::Session::IpNetworkMode::Static;
                config.sps_ip_address_v4 = parse_ipv4(sps_ip_address.value(), "--ip-sps-address");
                config.sps_subnet_mask_v4 = parse_ipv4(sps_subnet_mask.value(), "--ip-sps-subnet");
            }
            else if (has_sps_dhcp_parameter)
            {
                config.sps_mode = Deltacast::VideoMonitor::Session::IpNetworkMode::Dhcp;
            }

            return config;
        }

        auto source_filter_mode_from_cli(const std::optional<std::string>& source_filter_mode)
            -> std::optional<Deltacast::VideoMonitor::Session::IpSourceFilterMode>
        {
            if (!source_filter_mode.has_value())
            {
                return std::nullopt;
            }

            if (source_filter_mode.value() == "include")
            {
                return Deltacast::VideoMonitor::Session::IpSourceFilterMode::Include;
            }

            return Deltacast::VideoMonitor::Session::IpSourceFilterMode::Exclude;
        }

        auto
        build_ip_media_configuration(const std::optional<std::string>& main_destination,
                                     const std::optional<uint16_t>&    main_udp_port,
                                     const std::optional<uint16_t>&    main_payload_type,
                                     const std::optional<std::string>& main_source_ip,
                                     const std::optional<std::string>& main_source_filter_mode,
                                     const std::vector<std::string>&   main_source_filter_sources,
                                     const std::optional<std::string>& sps_destination,
                                     const std::optional<uint16_t>&    sps_udp_port,
                                     const std::optional<uint16_t>&    sps_payload_type,
                                     const std::optional<std::string>& sps_source_ip,
                                     const std::optional<std::string>& sps_source_filter_mode,
                                     const std::vector<std::string>&   sps_source_filter_sources,
                                     const std::optional<uint32_t>&    video_width,
                                     const std::optional<uint32_t>&    video_height,
                                     const std::optional<uint32_t>&    bit_depth,
                                     const std::optional<uint32_t>&    framerate_numerator,
                                     const std::optional<uint32_t>&    framerate_denominator)
            -> std::optional<Deltacast::VideoMonitor::Session::IpInputConfiguration>
        {
            const bool has_main_explicit_option = has_explicit_media_option(
                main_destination, main_udp_port, video_width, video_height, bit_depth,
                framerate_numerator, framerate_denominator, main_source_ip, main_source_filter_mode,
                main_source_filter_sources);
            const bool has_sps_explicit_option = has_explicit_media_option(
                sps_destination, sps_udp_port, video_width, video_height, bit_depth,
                framerate_numerator, framerate_denominator, sps_source_ip, sps_source_filter_mode,
                sps_source_filter_sources);

            if (!has_main_explicit_option && !has_sps_explicit_option)
            {
                return std::nullopt;
            }

            Deltacast::VideoMonitor::Session::IpDestinationConfiguration main_destination_config;
            Deltacast::VideoMonitor::Session::IpFilteringConfiguration   main_filtering_config;
            Deltacast::VideoMonitor::Session::IpMediaDescriptionConfiguration media_description;
            if (main_destination.has_value())
            {
                main_destination_config.destination_ip_address =
                    parse_ip_address(main_destination.value(), "--ip-main-destination");
            }
            main_destination_config.udp_port = main_udp_port;
            main_filtering_config.payload_type = main_payload_type;
            media_description.video_width = video_width.value();
            media_description.video_height = video_height.value();
            media_description.bit_depth = bit_depth.value();
            media_description.framerate_numerator = framerate_numerator.value();
            media_description.framerate_denominator = framerate_denominator.value();
            if (main_source_ip.has_value())
            {
                main_filtering_config.source_ip_address = parse_ip_address(main_source_ip.value(),
                                                                           "--ip-main-source-ip");
            }

            if (const auto filter_mode = source_filter_mode_from_cli(main_source_filter_mode);
                filter_mode.has_value())
            {
                Deltacast::VideoMonitor::Session::IpSourceFilterConfiguration source_filter;
                source_filter.mode = filter_mode.value();
                for (const auto& source_ip : main_source_filter_sources)
                {
                    source_filter.source_ip_addresses.push_back(
                        parse_ip_address(source_ip, "--ip-main-source-filter-sources"));
                }
                main_filtering_config.source_filter = source_filter;
            }

            Deltacast::VideoMonitor::Session::IpInputConfiguration config{
                main_destination_config, main_filtering_config, std::nullopt, std::nullopt,
                media_description
            };

            if (has_sps_explicit_option)
            {
                Deltacast::VideoMonitor::Session::IpDestinationConfiguration sps_destination_config;
                Deltacast::VideoMonitor::Session::IpFilteringConfiguration   sps_filtering_config;
                if (sps_destination.has_value())
                {
                    sps_destination_config.destination_ip_address =
                        parse_ip_address(sps_destination.value(), "--ip-sps-destination");
                }
                sps_destination_config.udp_port = sps_udp_port;
                sps_filtering_config.payload_type = sps_payload_type;
                if (sps_source_ip.has_value())
                {
                    sps_filtering_config.source_ip_address = parse_ip_address(sps_source_ip.value(),
                                                                              "--ip-sps-source-ip");
                }

                if (const auto filter_mode = source_filter_mode_from_cli(sps_source_filter_mode);
                    filter_mode.has_value())
                {
                    Deltacast::VideoMonitor::Session::IpSourceFilterConfiguration source_filter;
                    source_filter.mode = filter_mode.value();
                    for (const auto& source_ip : sps_source_filter_sources)
                    {
                        source_filter.source_ip_addresses.push_back(
                            parse_ip_address(source_ip, "--ip-sps-source-filter-sources"));
                    }
                    sps_filtering_config.source_filter = source_filter;
                }

                config.sps_destination_config = sps_destination_config;
                config.sps_filtering_config = sps_filtering_config;
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

        validate_explicit_ip_media_options(
            m_sdp_file_path, m_ip_main_destination, m_ip_main_udp_port, m_ip_main_source_ip,
            m_ip_main_source_filter_mode, m_ip_main_source_filter_sources, m_ip_sps_destination,
            m_ip_sps_udp_port, m_ip_sps_source_ip, m_ip_sps_source_filter_mode,
            m_ip_sps_source_filter_sources, m_ip_video_width, m_ip_video_height, m_ip_bit_depth,
            m_ip_framerate_numerator, m_ip_framerate_denominator);

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
            build_ip_network_configuration(m_ip_dhcp, m_gateway, m_ip_address, m_ip_subnet,
                                           m_ip_sps_dhcp, m_ip_sps_address, m_ip_sps_subnet),
            build_ip_media_configuration(
                m_ip_main_destination, m_ip_main_udp_port, m_ip_main_payload_type,
                m_ip_main_source_ip, m_ip_main_source_filter_mode, m_ip_main_source_filter_sources,
                m_ip_sps_destination, m_ip_sps_udp_port, m_ip_sps_payload_type, m_ip_sps_source_ip,
                m_ip_sps_source_filter_mode, m_ip_sps_source_filter_sources, m_ip_video_width,
                m_ip_video_height, m_ip_bit_depth, m_ip_framerate_numerator,
                m_ip_framerate_denominator),
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