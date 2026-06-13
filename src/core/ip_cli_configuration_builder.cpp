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

#include "ip_cli_configuration_builder.hpp"

#include "exceptions.hpp"
#include "helper.hpp"
#include "ip_input_configuration.hpp"

#include <filesystem>
#include <fmt/format.h>
#include <ipaddress/ip-any-address.hpp>
#include <optional>
#include <string>

namespace Deltacast::VideoMonitor::Core
{
    namespace
    {
        auto has_explicit_stream_option(const IpMediaStreamCliOptions& options) -> bool
        {
            return options.destination.has_value() || options.udp_port.has_value() ||
                   options.payload_type.has_value() || options.source_filter_mode.has_value() ||
                   !options.source_filter_sources.empty();
        }

        auto has_any_explicit_media_core_option(const IpMediaCoreCliOptions& options) -> bool
        {
            return options.width.has_value() || options.height.has_value() ||
                   options.bit_depth.has_value() || options.framerate_numerator.has_value() ||
                   options.framerate_denominator.has_value();
        }

        auto has_explicit_media_core(const IpMediaCoreCliOptions& options) -> bool
        {
            return options.width.has_value() && options.height.has_value() &&
                   options.bit_depth.has_value() && options.framerate_numerator.has_value() &&
                   options.framerate_denominator.has_value();
        }

        auto validate_source_filter_options(const IpMediaStreamCliOptions& options,
                                            const std::string&             destination_option_name,
                                            const std::string& source_filter_mode_option,
                                            const std::string& source_filter_sources_option) -> void
        {
            if (options.source_filter_mode.has_value() && options.source_filter_sources.empty())
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    fmt::format("{} requires at least one value in {}", source_filter_mode_option,
                                source_filter_sources_option));
            }

            if (!options.source_filter_mode.has_value() && !options.source_filter_sources.empty())
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    fmt::format("{} requires {}", source_filter_sources_option,
                                source_filter_mode_option));
            }

            if ((options.source_filter_mode.has_value() ||
                 !options.source_filter_sources.empty()) &&
                !options.destination.has_value())
            {
                throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                    fmt::format("{} requires {}", source_filter_mode_option,
                                destination_option_name));
            }
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
    }  // namespace

    void
    validate_explicit_ip_media_options(const std::optional<std::filesystem::path>& sdp_file_path,
                                       const IpMediaCliOptions&                    options)
    {
        const bool has_main_explicit_option = has_explicit_stream_option(options.main);
        const bool has_sps_explicit_option = has_explicit_stream_option(options.sps);
        const bool has_any_core_explicit_option = has_any_explicit_media_core_option(options.core);

        if (!has_main_explicit_option && !has_sps_explicit_option && !has_any_core_explicit_option)
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

        if (!has_explicit_media_core(options.core))
        {
            throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                "Explicit IP media mode requires --ip-video-width, --ip-video-height, "
                "--ip-video-bit-depth, --ip-video-framerate-num and --ip-video-framerate-den");
        }

        if (has_sps_explicit_option && !has_explicit_media_core(options.core))
        {
            throw Deltacast::VideoMonitor::Exceptions::ConfigurationException(
                "Explicit SPS media mode requires --ip-video-width, --ip-video-height, "
                "--ip-video-bit-depth, --ip-video-framerate-num and --ip-video-framerate-den");
        }

        if (options.source_ip.has_value())
        {
            Deltacast::VideoMonitor::Helper::parse_ip_address(options.source_ip.value(),
                                                              "--ip-source-ip");
        }

        if (options.main.destination.has_value())
        {
            Deltacast::VideoMonitor::Helper::parse_ip_address(options.main.destination.value(),
                                                              "--ip-main-destination");
        }

        validate_source_filter_options(options.main, "--ip-main-destination",
                                       "--ip-main-source-filter-mode",
                                       "--ip-main-source-filter-sources");

        if (options.sps.destination.has_value())
        {
            Deltacast::VideoMonitor::Helper::parse_ip_address(options.sps.destination.value(),
                                                              "--ip-sps-destination");
        }

        validate_source_filter_options(options.sps, "--ip-sps-destination",
                                       "--ip-sps-source-filter-mode",
                                       "--ip-sps-source-filter-sources");

        for (const auto& source_ip : options.main.source_filter_sources)
        {
            Deltacast::VideoMonitor::Helper::parse_ip_address(source_ip,
                                                              "--ip-main-source-filter-sources");
        }

        for (const auto& source_ip : options.sps.source_filter_sources)
        {
            Deltacast::VideoMonitor::Helper::parse_ip_address(source_ip,
                                                              "--ip-sps-source-filter-sources");
        }
    }

    auto build_ip_network_configuration(const IpNetworkCliOptions& options)
        -> std::optional<Deltacast::VideoMonitor::Session::IpNetworkConfiguration>
    {
        const bool has_main_dhcp_parameter = options.dhcp_requested.value_or(false);
        const bool has_sps_dhcp_parameter = options.sps_dhcp_requested.value_or(false);
        const bool has_main_static_parameter = options.ip_address.has_value() &&
                                               options.subnet_mask.has_value();
        const bool has_sps_static_parameter = options.sps_ip_address.has_value() &&
                                              options.sps_subnet_mask.has_value();
        const bool has_gateway_parameter = options.gateway.has_value();

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
            config.ip_address_v4 = Deltacast::VideoMonitor::Helper::parse_ipv4_address(
                options.ip_address.value(), "--ip-address");
            config.subnet_mask_v4 = Deltacast::VideoMonitor::Helper::parse_ipv4_address(
                options.subnet_mask.value(), "--ip-subnet");
        }
        else if (has_main_dhcp_parameter)
        {
            config.mode = Deltacast::VideoMonitor::Session::IpNetworkMode::Dhcp;
        }

        if (has_gateway_parameter)
        {
            config.gateway_v4 = Deltacast::VideoMonitor::Helper::parse_ipv4_address(
                options.gateway.value(), "--ip-gateway");
        }

        if (has_sps_static_parameter)
        {
            config.sps_mode = Deltacast::VideoMonitor::Session::IpNetworkMode::Static;
            config.sps_ip_address_v4 = Deltacast::VideoMonitor::Helper::parse_ipv4_address(
                options.sps_ip_address.value(), "--ip-sps-address");
            config.sps_subnet_mask_v4 = Deltacast::VideoMonitor::Helper::parse_ipv4_address(
                options.sps_subnet_mask.value(), "--ip-sps-subnet");
        }
        else if (has_sps_dhcp_parameter)
        {
            config.sps_mode = Deltacast::VideoMonitor::Session::IpNetworkMode::Dhcp;
        }

        return config;
    }

    auto build_ip_media_configuration(const IpMediaCliOptions& options)
        -> std::optional<Deltacast::VideoMonitor::Session::IpInputConfiguration>
    {
        const bool has_main_explicit_option = has_explicit_stream_option(options.main);
        const bool has_sps_explicit_option = has_explicit_stream_option(options.sps);
        const bool has_any_core_explicit_option = has_any_explicit_media_core_option(options.core);

        if (!has_main_explicit_option && !has_sps_explicit_option && !has_any_core_explicit_option)
        {
            return std::nullopt;
        }

        Deltacast::VideoMonitor::Session::IpDestinationConfiguration      main_destination_config;
        Deltacast::VideoMonitor::Session::IpFilteringConfiguration        main_filtering_config;
        Deltacast::VideoMonitor::Session::IpMediaDescriptionConfiguration media_description;
        std::optional<ipaddress::ip_address>                              source_ip_address =
            options.source_ip.has_value()
                ? std::make_optional(
                      Deltacast::VideoMonitor::Helper::parse_ip_address(options.source_ip.value(),
                                                                        "--ip-source-ip"))
                : std::nullopt;
        if (options.main.destination.has_value())
        {
            main_destination_config.destination_ip_address =
                Deltacast::VideoMonitor::Helper::parse_ip_address(options.main.destination.value(),
                                                                  "--ip-main-destination");
        }
        main_destination_config.udp_port = options.main.udp_port;
        main_filtering_config.payload_type = options.main.payload_type;
        media_description.width = options.core.width.value();
        media_description.height = options.core.height.value();
        media_description.bit_depth = options.core.bit_depth.value();
        media_description.framerate_numerator = options.core.framerate_numerator.value();
        media_description.framerate_denominator = options.core.framerate_denominator.value();

        if (const auto filter_mode = source_filter_mode_from_cli(options.main.source_filter_mode);
            filter_mode.has_value())
        {
            Deltacast::VideoMonitor::Session::IpSourceFilterConfiguration source_filter;
            source_filter.mode = filter_mode.value();
            for (const auto& source_ip : options.main.source_filter_sources)
            {
                source_filter.source_ip_addresses.push_back(
                    Deltacast::VideoMonitor::Helper::parse_ip_address(
                        source_ip, "--ip-main-source-filter-sources"));
            }
            main_filtering_config.source_filter = source_filter;
        }

        Deltacast::VideoMonitor::Session::IpInputConfiguration config{
            source_ip_address, main_destination_config, main_filtering_config, std::nullopt,
            std::nullopt,      media_description
        };

        if (has_sps_explicit_option)
        {
            Deltacast::VideoMonitor::Session::IpDestinationConfiguration sps_destination_config;
            Deltacast::VideoMonitor::Session::IpFilteringConfiguration   sps_filtering_config;
            if (options.sps.destination.has_value())
            {
                sps_destination_config.destination_ip_address =
                    Deltacast::VideoMonitor::Helper::parse_ip_address(
                        options.sps.destination.value(), "--ip-sps-destination");
            }
            sps_destination_config.udp_port = options.sps.udp_port;
            sps_filtering_config.payload_type = options.sps.payload_type;

            if (const auto filter_mode = source_filter_mode_from_cli(
                    options.sps.source_filter_mode);
                filter_mode.has_value())
            {
                Deltacast::VideoMonitor::Session::IpSourceFilterConfiguration source_filter;
                source_filter.mode = filter_mode.value();
                for (const auto& source_ip : options.sps.source_filter_sources)
                {
                    source_filter.source_ip_addresses.push_back(
                        Deltacast::VideoMonitor::Helper::parse_ip_address(
                            source_ip, "--ip-sps-source-filter-sources"));
                }
                sps_filtering_config.source_filter = source_filter;
            }

            config.sps_destination_config = sps_destination_config;
            config.sps_filtering_config = sps_filtering_config;
        }

        return config;
    }
}  // namespace Deltacast::VideoMonitor::Core
