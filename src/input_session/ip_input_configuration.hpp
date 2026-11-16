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

#include <cstdint>
#include <ipaddress/ip-any-address.hpp>
#include <ipaddress/ipaddress.hpp>
#include <ipaddress/ipv4-address.hpp>
#include <optional>
#include <vector>

namespace Deltacast::VideoMonitor::Session
{
    enum class IpSourceFilterMode
    {
        Include,
        Exclude
    };

    struct IpSourceFilterConfiguration
    {
        IpSourceFilterMode                 mode = IpSourceFilterMode::Include;
        std::vector<ipaddress::ip_address> source_ip_addresses;
    };

    struct IpDestinationConfiguration
    {
        ipaddress::ip_address   destination_ip_address;
        std::optional<uint16_t> udp_port;
    };

    struct IpFilteringConfiguration
    {
        std::optional<uint16_t>                    payload_type;
        std::optional<IpSourceFilterConfiguration> source_filter;
    };

    struct IpMediaDescriptionConfiguration
    {
        uint32_t width = 0;
        uint32_t height = 0;
        bool     interlaced = false;
        uint32_t framerate_numerator = 0;
        uint32_t framerate_denominator = 1;
        uint32_t bit_depth = 8;  // NOLINT(readability-magic-numbers)
    };

    struct IpInputConfiguration
    {
        std::optional<ipaddress::ip_address>      source_ip_address;
        IpDestinationConfiguration                main_destination_config;
        IpFilteringConfiguration                  main_filtering_config;
        std::optional<IpDestinationConfiguration> sps_destination_config;
        std::optional<IpFilteringConfiguration>   sps_filtering_config;
        IpMediaDescriptionConfiguration           media_description;
    };

    enum class IpNetworkMode
    {
        Dhcp,
        Static
    };

    struct IpNetworkConfiguration
    {
        bool                    has_main = false;
        IpNetworkMode           mode = IpNetworkMode::Dhcp;
        ipaddress::ipv4_address ip_address_v4;
        ipaddress::ipv4_address subnet_mask_v4;

        bool                    has_gateway = false;
        ipaddress::ipv4_address gateway_v4;

        bool                    has_sps = false;
        IpNetworkMode           sps_mode = IpNetworkMode::Dhcp;
        ipaddress::ipv4_address sps_ip_address_v4;
        ipaddress::ipv4_address sps_subnet_mask_v4;
    };
}  // namespace Deltacast::VideoMonitor::Session