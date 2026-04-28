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

#include "ip_input_session.hpp"
#include "exceptions.hpp"
#include "input_session.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/board/ip/port/port.hpp>
#include <VideoMasterCppApi/helper/ip.hpp>
#include <VideoMasterCppApi/helper/sdp.hpp>
#include <VideoMasterCppApi/helper/video.hpp>
#include <VideoMasterCppApi/slot/ip/st2110_slot.hpp>
#include <VideoMasterCppApi/stream/ip/essence.hpp>
#include <VideoMasterCppApi/stream/ip/st2110_stream.hpp>
#include <VideoMasterCppApi/stream/ip/video.hpp>
#include <VideoMasterCppApi/to_string.hpp>
#if defined(__APPLE__)
#include <VideoMasterHD/VideoMasterHD_Core.h>
#include <VideoMasterHD/VideoMasterHD_Ip_Board.h>
#include <VideoMasterHD/VideoMasterHD_Ip_ST2110_Board.h>
#include <VideoMasterHD/VideoMasterHD_SDP.h>
#else
#include <VideoMasterHD_Core.h>
#include <VideoMasterHD_Ip_Board.h>
#include <VideoMasterHD_Ip_ST2110_Board.h>
#include <VideoMasterHD_SDP.h>
#endif
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <functional>
#include <ipaddress/ip-address-base.hpp>
#include <ipaddress/ip-any-address.hpp>
#include <ipaddress/ipaddress.hpp>
#include <ipaddress/ipv4-address.hpp>
#include <iterator>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace Deltacast::VideoMonitor::Session
{
    namespace
    {

        constexpr uint32_t dhcp_timeout_seconds = 10;
        constexpr uint32_t dhcp_poll_interval_ms = 100;
        constexpr uint32_t main_port_index = 0;
        constexpr uint32_t sps_port_index = 1;
        constexpr uint32_t ipv6_byte_count = 16;

        auto parse_sdp_ip_address(const VHD_SDP_IP_ADDRESS& ip_address_struct)
            -> ipaddress::ip_address
        {
            if (ip_address_struct.Version == VHD_SDP_IP_VERSION_4)
            {
                return ipaddress::ipv4_address::from_uint(ip_address_struct.AddressV4);
            }

            if (ip_address_struct.Version == VHD_SDP_IP_VERSION_6)
            {
                const auto* bytes = reinterpret_cast<const uint8_t*>(
                    &ip_address_struct.AddressV6[0]);
                return ipaddress::ip_address::from_bytes(bytes, sizeof(ip_address_struct.AddressV6),
                                                         ipaddress::ip_version::V6);
            }

            throw Exceptions::ConfigurationException("Unsupported IP version in SDP");
        }

        auto to_ipv6_bytes(const ipaddress::ip_address& addr)
            -> std::array<uint8_t, ipv6_byte_count>
        {
            const auto&                          v6_bytes = addr.v6().value().bytes();
            std::array<uint8_t, ipv6_byte_count> result{};
            std::memcpy(result.data(), v6_bytes.data(), result.size());
            return result;
        }

        auto network_mode_to_string(IpNetworkMode mode) -> const char*
        {
            return mode == IpNetworkMode::Dhcp ? "DHCP" : "static";
        }

        auto wait_for_dhcp_ip(Deltacast::Wrapper::BoardComponents::IpComponents::Port& port) -> void
        {
            const auto deadline = std::chrono::steady_clock::now() +
                                  std::chrono::seconds(dhcp_timeout_seconds);
            spdlog::trace("Waiting for DHCP lease assignment...");
            while (!static_cast<bool>(port.dhcp().status().IPValid))
            {
                if (std::chrono::steady_clock::now() >= deadline)
                {
                    spdlog::warn("DHCP lease assignment timed out after {} seconds",
                                 dhcp_timeout_seconds);
                    throw Exceptions::NetworkException(
                        "Timed out waiting for DHCP to assign an IP address");
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(dhcp_poll_interval_ms));
            }

            spdlog::info("DHCP lease acquired");
            spdlog::debug("DHCP assigned IP address: {}",
                          ipaddress::ipv4_address::from_uint(port.ip_address()).to_string());
            spdlog::debug("DHCP assigned subnet mask: {}",
                          ipaddress::ipv4_address::from_uint(port.subnet_mask()).to_string());
            spdlog::debug("DHCP assigned gateway: {}",
                          ipaddress::ipv4_address::from_uint(port.gateway()).to_string());
        };

        auto configure_ip_port(Deltacast::Wrapper::Board& board, uint32_t port_index,
                               IpNetworkMode mode, const ipaddress::ipv4_address& ip_address_v4,
                               const ipaddress::ipv4_address& subnet_mask_v4,
                               const ipaddress::ipv4_address& gateway_v4,
                               const char*                    unsupported_dhcp_message) -> void
        {
            auto& port = board.ip().port(port_index);
            spdlog::trace("Configuring IP port {} in {} mode", port_index,
                          network_mode_to_string(mode));

            if (mode == IpNetworkMode::Dhcp)
            {
                if (!port.has_dhcp())
                {
                    spdlog::warn("DHCP requested on IP port {} but not supported", port_index);
                    throw Exceptions::NetworkException(unsupported_dhcp_message);
                }

                port.dhcp().enable();
                wait_for_dhcp_ip(port);
                spdlog::trace("IP port {} DHCP configuration applied", port_index);
                return;
            }

            if (port.has_dhcp())
            {
                port.dhcp().disable();
            }

            port.set_ip_address(ip_address_v4.to_uint());
            port.set_subnet_mask(subnet_mask_v4.to_uint());
            port.set_gateway(gateway_v4.to_uint());
            spdlog::trace("IP port {} configured with IP address {}, subnet mask {}, gateway {}",
                          port_index,
                          ipaddress::ipv4_address::from_uint(port.ip_address()).to_string(),
                          ipaddress::ipv4_address::from_uint(port.subnet_mask()).to_string(),
                          ipaddress::ipv4_address::from_uint(port.gateway()).to_string());

            spdlog::debug("Configured IP port {} with address {}, subnet {}, gateway {}",
                          port_index, ip_address_v4.to_string(), subnet_mask_v4.to_string(),
                          gateway_v4.to_string());
        };

        auto set_stream_address(
            Deltacast::Wrapper::StreamComponents::IpComponents::Essence&            stream,
            const ipaddress::ip_address&                                            ip_address,
            const std::function<void(Deltacast::Wrapper::StreamComponents::IpComponents::Essence&,
                                     uint32_t)>&                                    set_v4,
            const std::function<void(Deltacast::Wrapper::StreamComponents::IpComponents::Essence&,
                                     const std::array<uint8_t, ipv6_byte_count>&)>& set_v6,
            const char* address_kind) -> void
        {
            if (ip_address.is_v4())
            {
                set_v4(stream, ip_address.to_uint32());
                spdlog::trace("Configuring {} IP address {} for IPv4 stream", address_kind,
                              ip_address.to_string());
                return;
            }

            if (ip_address.is_v6())
            {
                set_v6(stream, to_ipv6_bytes(ip_address));
                spdlog::trace("Configuring {} IP address {} for IPv6 stream", address_kind,
                              ip_address.to_string());
            }
        }

        auto
        set_destination_address(Deltacast::Wrapper::StreamComponents::IpComponents::Essence& stream,
                                const ipaddress::ip_address& ip_address) -> void
        {
            set_stream_address(
                stream, ip_address,
                [](Deltacast::Wrapper::StreamComponents::IpComponents::Essence& current_stream,
                   uint32_t address_v4) { current_stream.set_destination_ip_address(address_v4); },
                [](Deltacast::Wrapper::StreamComponents::IpComponents::Essence& current_stream,
                   const std::array<uint8_t, ipv6_byte_count>&                  address_v6)
                { current_stream.set_destination_ipv6_address(address_v6); },
                "destination");
        }

        auto set_source_address(Deltacast::Wrapper::StreamComponents::IpComponents::Essence& stream,
                                const ipaddress::ip_address& ip_address) -> void
        {
            set_stream_address(
                stream, ip_address,
                [](Deltacast::Wrapper::StreamComponents::IpComponents::Essence& current_stream,
                   uint32_t address_v4) { current_stream.set_source_ip_address(address_v4); },
                [](Deltacast::Wrapper::StreamComponents::IpComponents::Essence& current_stream,
                   const std::array<uint8_t, ipv6_byte_count>&                  address_v6)
                { current_stream.set_source_ipv6_address(address_v6); },
                "source");
        }

        auto source_filter_sources_to_string(const VHD_SDP_MEDIA& media_description) -> std::string
        {
            std::string result;
            for (ULONG i = 0; i < media_description.SourceFilter.SourceIPCount; ++i)
            {
                if (i > 0)
                {
                    result += ", ";
                }
                result += parse_sdp_ip_address(media_description.SourceFilter.SourceIPArray[i])
                              .to_string();
            }
            return result;
        }

        auto configure_multicast_filtering(
            Deltacast::Wrapper::StreamComponents::IpComponents::Essence& stream,
            Deltacast::Wrapper::BoardComponents::IpComponents::Port&     port,
            const VHD_SDP_MEDIA&                                         media_description) -> void
        {
            stream.set_filtering_mask(VHD_IP_FILTER_RTP_PAYLOAD_TYPE | VHD_IP_FILTER_UDP_PORT_DEST |
                                      VHD_IP_FILTER_IP_ADDR_DEST);

            if (!static_cast<bool>(media_description.SourceFilter.UseSourceFilter))
            {
                spdlog::trace(
                    "Configuring multicast filtering for destination {} with no source filtering",
                    ipaddress::ipv4_address::from_uint(
                        media_description.SourceFilter.DestinationIP.AddressV4)
                        .to_string());
                return;
            }

            if (media_description.SourceFilter.DestinationIP.Version != VHD_SDP_IP_VERSION_4)
            {
                throw Exceptions::ConfigurationException(
                    "Source filtering is only supported for IPv4 addresses");
            }

            spdlog::trace(
                "Configuring multicast filtering for destination {}: filter mode {}, source "
                "IPs {}",
                ipaddress::ipv4_address::from_uint(
                    media_description.SourceFilter.DestinationIP.AddressV4)
                    .to_string(),
                media_description.SourceFilter.FilterMode == VHD_SDP_FILTER_MODE_INCL ? "INCLUDE"
                                                                                      : "EXCLUDE",
                source_filter_sources_to_string(media_description));

            port.multicast().set_source_mode(media_description.SourceFilter.DestinationIP.AddressV4,
                                             media_description.SourceFilter.FilterMode ==
                                                     VHD_SDP_FILTER_MODE_INCL
                                                 ? VHD_IP_BRD_FILTERMODEMULTICAST_INCLUDE
                                                 : VHD_IP_BRD_FILTERMODEMULTICAST_EXCLUDE);

            for (ULONG i = 0; i < media_description.SourceFilter.SourceIPCount; ++i)
            {
                const auto source_ip_address = ipaddress::ipv4_address::from_uint(
                    media_description.SourceFilter.SourceIPArray[i].AddressV4);
                port.multicast().add_source(media_description.SourceFilter.DestinationIP.AddressV4,
                                            source_ip_address.to_uint());
            }
        }

        auto
        configure_destination(Deltacast::Wrapper::StreamComponents::IpComponents::Essence& stream,
                              Deltacast::Wrapper::BoardComponents::IpComponents::Port&     port,
                              const VHD_SDP_SESSION&                                       session,
                              const VHD_SDP_MEDIA&         media_description,
                              const ipaddress::ip_address& ip_address) -> void
        {
            set_destination_address(stream, ip_address);
            spdlog::trace(
                "Configured destination IP address for stream: {}",
                ipaddress::ip_address::from_uint(stream.destination_ip_address()).to_string());

            if (ip_address.is_multicast() && port.has_multicast())
            {
                configure_multicast_filtering(stream, port, media_description);
            }
            else if (ip_address.is_multicast())
            {
                throw Exceptions::ConfigurationException(
                    fmt::format("IP address {} in SDP is multicast but port does not support "
                                "multicast reception",
                                ip_address.to_string()));
            }
            else
            {
                spdlog::trace("Configuring unicast source IP address for destination {}",
                              ip_address.to_string());
                const auto source_ip_address = parse_sdp_ip_address(session.SourceIP);
                spdlog::trace("Parsed source IP address {} from SDP session",
                              source_ip_address.to_string());
                set_source_address(stream, source_ip_address);
                spdlog::trace(
                    "Configured unicast source IP address for destination {}: {}",
                    ip_address.to_string(),
                    ipaddress::ip_address::from_uint(stream.source_ip_address()).to_string());
            }

            stream.set_destination_port(media_description.UdpPort);
            stream.set_rtp_payload_type(media_description.PayloadType);
        }

    }  // namespace

    IpInputSession::IpInputSession(const IpInputSessionConfig&               config,
                                   Deltacast::VideoMonitor::SharedResources& shared_resources)
        : InputSession<Deltacast::Wrapper::Ip2110Stream>(config, shared_resources),
          m_network_configuration(config.network_configuration)
    {
        spdlog::trace("Loading SDP file '{}' for IP RX{}", config.sdp_file_path.string(),
                      config.stream_id);
        std::string sdp_content;
        {
            std::ifstream sdp_file(config.sdp_file_path);
            if (!sdp_file.is_open())
            {
                throw Exceptions::ConfigurationException(
                    fmt::format("Failed to open SDP file: {}", config.sdp_file_path.string()));
            }
            sdp_content.assign((std::istreambuf_iterator<char>(sdp_file)),
                               std::istreambuf_iterator<char>());
        }

        auto [session, media] = Deltacast::Wrapper::Helper::Ip::read_sdp(sdp_content);
        this->m_session = session;
        spdlog::debug("Parsed SDP session with {} media description(s)", media.size());
        if (media.empty())
        {
            throw Exceptions::ConfigurationException(
                "SDP file must contain at least one media description");
        }

        if (media[0].MediaType == VHD_SDP_MEDIA_TYPE_ST2110_20)
        {
            this->m_main_media = media[0];

            if (media.size() > 1 && media[1].MediaType != VHD_SDP_MEDIA_TYPE_ST2110_20)
            {
                throw Exceptions::ConfigurationException(
                    "If a second media description is present in the SDP file, it must be of "
                    "type ST2110-20 for SPS");
            }

            if (media.size() > 1 && !m_network_configuration.has_sps)
            {
                spdlog::warn("A SPS media description is present in the SDP file but SPS is not "
                             "enabled in network "
                             "configuration. SPS stream will be ignored.");
            }
            else if (m_network_configuration.has_sps && media.size() == 1)
            {
                spdlog::warn("SPS is enabled in network configuration but only one media "
                             "description found in SDP. SPS stream will be ignored.");
            }
            else if (m_network_configuration.has_sps && media.size() > 1)
            {
                this->m_sps_media = media[1];
                m_use_sps_stream = true;
            }

            if (media.size() > 2)
            {
                spdlog::warn(
                    "SDP file contains more than 2 media descriptions. Only the first 2 will "
                    "be processed.");
            }
        }
        else
        {
            throw Exceptions::ConfigurationException(
                "The media described in the SDP file must be of type ST2110-20");
        }
    }

    void IpInputSession::join_multicast_group(const VHD_SDP_IP_ADDRESS& ip_address_struct,
                                              uint32_t                  port_index)
    {
        auto&      port = this->m_board->ip().port(port_index);
        const auto ip_address = parse_sdp_ip_address(ip_address_struct);

        if (!ip_address.is_multicast())
        {
            spdlog::trace("Skipping multicast join for unicast destination {} on port {}",
                          ip_address.to_string(), port_index);
            return;
        }

        if (!port.has_multicast())
        {
            throw Exceptions::NetworkException(fmt::format(
                "Multicast IP address {} in SDP but the port does not support multicast",
                ip_address.to_string()));
        }

        port.multicast().set_version(VHD_IP_BRD_IGMP_VERSION_V3);

        if (ip_address.is_v4())
        {

            port.multicast().join(ip_address.to_uint32());
        }
        else if (ip_address.is_v6())
        {
            port.multicast().join(to_ipv6_bytes(ip_address));
        }

        m_multicast_groups.emplace_back(port_index, ip_address);
        spdlog::trace("Joined multicast group {} on port {}", ip_address.to_string(), port_index);
    }

    void IpInputSession::open_board()
    {
        spdlog::trace("Opening IP board {} for RX{}", this->device_id(), this->stream_id());
        m_multicast_groups.clear();

        this->m_board = std::make_unique<Deltacast::Wrapper::Board>(Deltacast::Wrapper::Board::open(
            this->device_id(),
            [&](auto& board)
            {
                for (const auto& [port_index, group] : m_multicast_groups)
                {
                    auto& port = board.ip().port(port_index);
                    if (group.is_v6())
                    {
                        port.multicast().leave(to_ipv6_bytes(group));
                    }
                    else if (group.is_v4())
                    {
                        port.multicast().leave(group.to_uint32());
                    }
                }
            }));

        spdlog::debug("Configuring Main port {}", main_port_index);
        configure_ip_port(this->board(), main_port_index, m_network_configuration.mode,
                          m_network_configuration.ip_address_v4,
                          m_network_configuration.subnet_mask_v4,
                          m_network_configuration.gateway_v4,
                          "DHCP mode requested but DHCP is not supported on this IP port");

        if (m_use_sps_stream)
        {
            spdlog::debug("Configuring SPS port {}", sps_port_index);
            configure_ip_port(
                this->board(), sps_port_index, m_network_configuration.sps_mode,
                m_network_configuration.sps_ip_address_v4,
                m_network_configuration.sps_subnet_mask_v4, m_network_configuration.sps_gateway_v4,
                "DHCP mode requested for SPS but DHCP is not supported on SPS IP port");
        }

        join_multicast_group(m_main_media.DestinationIP, main_port_index);

        if (m_use_sps_stream)
        {
            join_multicast_group(m_sps_media.DestinationIP, sps_port_index);
        }
    }

    void IpInputSession::prepare_video_stream()
    {
        auto& board = this->board();
        auto  stream_id = this->stream_id();

        spdlog::trace("Opening ST2110-20 essence stream for RX{}", stream_id);

        m_stream = std::make_unique<Deltacast::Wrapper::Ip2110Stream>(
            board.ip().ip2110().open_essence_stream(VHD_ET_ST2110_20, VHD_RX_CHANNEL, stream_id));

        auto destination_ip_address = parse_sdp_ip_address(m_main_media.DestinationIP);
        spdlog::info("Configuring main ST2110 media: destination {}, "
                     "UDP {}, payload {}",
                     destination_ip_address.to_string(), m_main_media.UdpPort,
                     m_main_media.PayloadType);
        configure_destination(m_stream->main_stream(), board.ip().port(main_port_index), m_session,
                              m_main_media, destination_ip_address);

        m_stream->video().set_video_standard(m_main_media.ST2110_20.VideoStandard);
        m_stream->video().set_sampling_rate(m_main_media.ST2110_20.Sampling);
        m_stream->video().set_bit_depth(m_main_media.ST2110_20.Depth);

        m_video_characteristics = Deltacast::Wrapper::Helper::Ip::video_standard_to_characteristics(
            m_main_media.ST2110_20.VideoStandard);

        spdlog::info("Detected ST2110 video standard {} ({}x{}, interlaced={})",
                     Deltacast::Wrapper::to_pretty_string(m_main_media.ST2110_20.VideoStandard),
                     m_video_characteristics.width, m_video_characteristics.height,
                     static_cast<bool>(m_video_characteristics.interlaced));

        if (m_use_sps_stream)
        {
            auto destination_ip_address = parse_sdp_ip_address(m_sps_media.DestinationIP);
            spdlog::info("Configuring SPS ST2110 media: destination {}, UDP {}, "
                         "payload {}",
                         destination_ip_address.to_string(), m_sps_media.UdpPort,
                         m_sps_media.PayloadType);
            configure_destination(m_stream->sps_stream(), board.ip().port(sps_port_index),
                                  m_session, m_sps_media, destination_ip_address);
        }
    }

    void IpInputSession::configure_video_stream()
    {
        auto& stream = this->stream();

        stream.buffer_queue().set_depth(buffer_queue_size);
        stream.set_buffer_packing(VHD_BUFPACK_VIDEO_YUV422_8);
        spdlog::trace("Configured ST2110 buffer queue depth={} packing=YUV422_8",
                      buffer_queue_size);
    }

    auto IpInputSession::has_video_input_changed() -> bool
    {
        return false;
    }

    auto IpInputSession::video_input_has_changed() -> bool
    {
        return false;
    }

    auto IpInputSession::get_video_buffer() -> std::pair<UBYTE*, ULONG>
    {
        this->ensure_board_is_opened();
        auto  current_slot = this->stream().pop_slot();
        auto& slot = static_cast<Deltacast::Wrapper::Ip2110Slot&>(*current_slot);
        return slot.video_essence().buffer();
    }

    auto IpInputSession::get_video_characteristics()
        -> Deltacast::Wrapper::Helper::VideoCharacteristics
    {
        return { m_video_characteristics.width, m_video_characteristics.height,
                 m_video_characteristics.interlaced, m_video_characteristics.framerate };
    }

}  // namespace Deltacast::VideoMonitor::Session