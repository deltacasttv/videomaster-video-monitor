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

#include "VideoMasterAPIHelper/enum_to_string.hpp"

#include "VideoMasterHD_Core.h"
#include "VideoMasterHD_Sdi.h"

#include <cstdint>
#include <iostream>

namespace Deltacast
{
    struct SignalInformation
    {
        VHD_VIDEOSTANDARD video_standard;
        VHD_CLOCKDIVISOR clock_divisor;
        VHD_INTERFACE interface;

        friend bool operator==(const SignalInformation& left, const SignalInformation& right)
        {
            return left.video_standard == right.video_standard
            && left.clock_divisor == right.clock_divisor
            && left.interface == right.interface;
        }

        friend std::ostream& operator<<(std::ostream& os, const SignalInformation& signal_info)
        {
            os << "Signal information:" << std::endl;
            os << "\t" << "video standard: " << Deltacast::Helper::enum_to_string(signal_info.video_standard) << std::endl;
            os << "\t" << "clock divisor: " << Deltacast::Helper::enum_to_string(signal_info.clock_divisor) << std::endl;
            os << "\t" << "interface: " << Deltacast::Helper::enum_to_string(signal_info.interface) << std::endl;

            return os;
        }
    };

    struct DecodedSignalInformation
    {
        uint32_t width;
        uint32_t height;
        bool progressive;
        double framerate;

        friend std::ostream& operator<<(std::ostream& os, const DecodedSignalInformation& decoded_signal_info)
        {
            os << "Decoded signal information:" << std::endl;
            os << "\t" << "width: " << decoded_signal_info.width << std::endl;
            os << "\t" << "height: " << decoded_signal_info.height << std::endl;
            os << "\t" << "progressive: " << (decoded_signal_info.progressive ? "true" : "false") << std::endl;
            os << "\t" << "framerate: " << decoded_signal_info.framerate << std::endl;

            return os;
        }
    };

    DecodedSignalInformation decode(const SignalInformation& signal_information);
}