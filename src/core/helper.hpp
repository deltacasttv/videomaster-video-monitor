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

#include <atomic>
#include <iostream>
#include <ipaddress/ip-any-address.hpp>
#include <ipaddress/ipaddress.hpp>
#include <ipaddress/ipv4-address.hpp>
#include <string>

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/board/rx/rx.hpp>
#include <VideoMasterCppApi/exception.hpp>
#include <VideoMasterCppApi/helper/sdi.hpp>
#include <VideoMasterCppApi/to_string.hpp>
#if defined(__APPLE__)
#include <VideoMasterHD/VideoMasterHD_Core.h>
#else
#include <VideoMasterHD_Core.h>
#endif

auto operator<<(std::ostream& output_stream, Deltacast::Wrapper::Board& board) -> std::ostream&;

namespace Deltacast::VideoMonitor::Helper
{
    auto parse_ipv4_address(const std::string& address, const std::string& option_name)
        -> ipaddress::ipv4_address;
    auto parse_ip_address(const std::string& address, const std::string& option_name)
        -> ipaddress::ip_address;

    auto rx_index_to_streamtype(unsigned int rx_index) -> VHD_STREAMTYPE;
    auto wait_for_input(Deltacast::Wrapper::BoardComponents::RxConnector& rx_connector,
                        const std::atomic_bool& stop_is_requested) -> bool;
}  // namespace Deltacast::VideoMonitor::Helper
