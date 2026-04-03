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

#include <atomic>
#include <iostream>
#include <variant>

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/helper/video.hpp>
#include <VideoMasterCppApi/stream/dv/dv_stream.hpp>
#include <VideoMasterCppApi/stream/ip/st2110_stream.hpp>
#include <VideoMasterCppApi/stream/sdi/sdi_stream.hpp>

std::ostream& operator<<(std::ostream& os, Deltacast::Wrapper::Board& board);

namespace Deltacast::VideoMonitor::Helper
{
    namespace Sdi
    {
        struct SignalInformation
        {
            VHD_VIDEOSTANDARD video_standard;
            VHD_CLOCKDIVISOR  clock_divisor;
            VHD_INTERFACE     video_interface;

            bool operator==(const SignalInformation& other) const
            {
                return video_standard == other.video_standard &&
                       clock_divisor == other.clock_divisor &&
                       video_interface == other.video_interface;
            }
            bool operator!=(const SignalInformation& other) const { return !(*this == other); }
        };

    }  // namespace Sdi

    namespace Dv
    {
        struct SignalInformation
        {
            unsigned int    width;
            unsigned int    height;
            bool            progressive;
            unsigned int    framerate;
            VHD_DV_CS       cable_color_space;
            VHD_DV_SAMPLING cable_sampling;

            bool operator==(const SignalInformation& other) const
            {
                return width == other.width && height == other.height &&
                       framerate == other.framerate && progressive == other.progressive &&
                       cable_color_space == other.cable_color_space &&
                       cable_sampling == other.cable_sampling;
            }
            bool operator!=(const SignalInformation& other) const { return !(*this == other); }
        };
    }  // namespace Dv

    using TechStream = std::variant<Deltacast::Wrapper::SdiStream, Deltacast::Wrapper::DvStream>;
    using SignalInformation = std::variant<Sdi::SignalInformation, Dv::SignalInformation>;

    void enable_loopback(Deltacast::Wrapper::Board& board, unsigned int channel_index);
    void disable_loopback(Deltacast::Wrapper::Board& board, unsigned int channel_index);

    VHD_STREAMTYPE rx_index_to_streamtype(unsigned int rx_index);

    bool wait_for_input(Deltacast::Wrapper::BoardComponents::RxConnector& rx_connector,
                        const std::atomic_bool&                           stop_is_requested);

    TechStream open_stream(Deltacast::Wrapper::Board& board, VHD_STREAMTYPE stream_type);
    Deltacast::Wrapper::Stream& to_base_stream(TechStream& stream);

    void        configure_stream(TechStream& stream, const SignalInformation& signal_information);
    std::string get_information_string(const SignalInformation& signal_information,
                                       const std::string&       prefix = "");
    SignalInformation detect_information(TechStream& stream);

    Deltacast::Wrapper::Helper::VideoCharacteristics
    get_video_characteristics(const SignalInformation& signal_information);
}  // namespace Deltacast::VideoMonitor::Helper
