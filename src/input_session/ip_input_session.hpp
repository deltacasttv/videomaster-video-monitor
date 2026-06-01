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
#include <VideoMasterCppApi/helper/sdp.hpp>
#include <VideoMasterCppApi/helper/video.hpp>
#include <VideoMasterCppApi/stream/ip/st2110_stream.hpp>
#if defined(__APPLE__)
#include <VideoMasterHD/VideoMasterHD_Core.h>
#include <VideoMasterHD/VideoMasterHD_SDP.h>
#else
#include <VideoMasterHD_Core.h>
#include <VideoMasterHD_SDP.h>
#endif
#include <cstdint>
#include <filesystem>
#include <ipaddress/ip-any-address.hpp>
#include <ipaddress/ipaddress.hpp>
#include <ipaddress/ipv4-address.hpp>
#include <utility>
#include <vector>

#include "input_session.hpp"
#include "shared_resources.hpp"

namespace Deltacast::VideoMonitor::Session
{
    enum class IpNetworkMode
    {
        Dhcp,
        Static
    };

    struct IpNetworkConfiguration
    {
        IpNetworkMode           mode = IpNetworkMode::Dhcp;
        ipaddress::ipv4_address ip_address_v4;
        ipaddress::ipv4_address subnet_mask_v4;
        ipaddress::ipv4_address gateway_v4;

        bool                    has_sps = false;
        IpNetworkMode           sps_mode = IpNetworkMode::Dhcp;
        ipaddress::ipv4_address sps_ip_address_v4;
        ipaddress::ipv4_address sps_subnet_mask_v4;
    };

    struct IpInputSessionConfig : InputSessionConfig
    {
        std::filesystem::path  sdp_file_path;
        IpNetworkConfiguration network_configuration;
    };

    class IpInputSession : public InputSession<Deltacast::Wrapper::Ip2110Stream>
    {
     public:
        explicit IpInputSession(const IpInputSessionConfig&               config,
                                Deltacast::VideoMonitor::SharedResources& shared_resources);

        virtual ~IpInputSession() = default;

        void open_board() override;
        void prepare_video_stream() override;
        void configure_video_stream() override;
        auto video_input_has_changed() -> bool override;
        auto get_video_buffer() -> std::pair<UBYTE*, ULONG> override;
        auto get_video_characteristics()
            -> Deltacast::Wrapper::Helper::VideoCharacteristics override;

     protected:
        auto has_video_input_changed() -> bool override;

     private:
        void join_multicast_group(const VHD_SDP_IP_ADDRESS& ip_address_struct, uint32_t port_index);

        IpNetworkConfiguration                                              m_network_configuration;
        VHD_SDP_SESSION                                                     m_session;
        VHD_SDP_MEDIA                                                       m_main_media;
        VHD_SDP_MEDIA                                                       m_sps_media;
        std::vector<std::pair<uint32_t, ipaddress::ip_address>>             m_multicast_groups;
        Deltacast::Wrapper::Helper::VideoCharacteristicsFractionalFramerate m_video_characteristics;
        bool m_use_sps_stream{ false };
    };
}  // namespace Deltacast::VideoMonitor::Session