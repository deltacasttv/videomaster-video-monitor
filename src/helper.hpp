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

#include <iostream>
#include <atomic>

#include <VideoMasterCppApi/helper/video.hpp>
#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/stream/sdi/sdi_stream.hpp>

std::ostream& operator<<(std::ostream& os, Deltacast::Wrapper::Board& board);

namespace Application::Helper
{
    void enable_loopback(Deltacast::Wrapper::Board& board, unsigned int channel_index);
    void disable_loopback(Deltacast::Wrapper::Board& board, unsigned int channel_index);

    VHD_STREAMTYPE rx_index_to_streamtype(unsigned int rx_index);
    VHD_STREAMTYPE tx_index_to_streamtype(unsigned int tx_index);

    bool wait_for_input(Deltacast::Wrapper::BoardComponents::RxConnector& rx_connector, const std::atomic_bool& stop_is_requested);

    struct SdiSignalInformation
    {
        VHD_VIDEOSTANDARD video_standard;
        VHD_CLOCKDIVISOR clock_divisor;
        VHD_INTERFACE video_interface;

        bool operator==(const SdiSignalInformation& other) const
        {
            return video_standard == other.video_standard 
                    && clock_divisor == other.clock_divisor 
                    && video_interface == other.video_interface;
        }
        bool operator!=(const SdiSignalInformation& other) const
        {
            return !(*this == other);
        }
    };
    SdiSignalInformation detect_information(Deltacast::Wrapper::SdiStream& sdi_stream);

    Deltacast::Wrapper::Helper::VideoCharacteristics get_video_characteristics(const SdiSignalInformation& sdi_signal_information);
}
