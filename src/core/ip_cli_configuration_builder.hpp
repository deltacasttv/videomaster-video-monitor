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

#pragma once

#include "ip_input_configuration.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Deltacast::VideoMonitor::Core
{
    struct IpNetworkCliOptions
    {
        std::optional<bool>        dhcp_requested;
        std::optional<std::string> gateway;
        std::optional<std::string> ip_address;
        std::optional<std::string> subnet_mask;
        std::optional<bool>        sps_dhcp_requested;
        std::optional<std::string> sps_ip_address;
        std::optional<std::string> sps_subnet_mask;
    };

    struct IpMediaStreamCliOptions
    {
        std::optional<std::string> destination;
        std::optional<uint16_t>    udp_port;
        std::optional<uint16_t>    payload_type;
        std::optional<std::string> source_filter_mode;
        std::vector<std::string>   source_filter_sources;
    };

    struct IpMediaCoreCliOptions
    {
        std::optional<uint32_t> width;
        std::optional<uint32_t> height;
        std::optional<bool>     interlaced;
        std::optional<uint32_t> bit_depth;
        std::optional<uint32_t> framerate_numerator;
        std::optional<uint32_t> framerate_denominator;
    };

    struct IpMediaCliOptions
    {
        std::optional<std::string> source_ip;
        IpMediaStreamCliOptions    main;
        IpMediaStreamCliOptions    sps;
        IpMediaCoreCliOptions      core;
    };

    auto build_ip_network_configuration(const IpNetworkCliOptions& options)
        -> std::optional<Deltacast::VideoMonitor::Session::IpNetworkConfiguration>;

    auto build_ip_media_configuration(const IpMediaCliOptions& options)
        -> std::optional<Deltacast::VideoMonitor::Session::IpInputConfiguration>;

    void
    validate_explicit_ip_media_options(const std::optional<std::filesystem::path>& sdp_file_path,
                                       const IpMediaCliOptions&                    options);
}  // namespace Deltacast::VideoMonitor::Core
