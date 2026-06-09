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

#include "exceptions.hpp"
#include "shared_resources.hpp"
#include "video_monitor.hpp"

#include <CLI/CLI.hpp>
#include <VideoMasterCppApi/api.hpp>
#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/exception.hpp>
#include <VideoMasterCppApi/to_string.hpp>
#include <atomic>
#include <csignal>
#include <exception>
#include <spdlog/spdlog.h>

Deltacast::VideoMonitor::SharedResources shared_resources;

void on_close(int /*signal*/)
{
    shared_resources.stop_is_requested = true;
}

int main(int argc, char** argv)
{
    signal(SIGINT, on_close);

    try
    {
        Deltacast::VideoMonitor::VideoMonitorApp app(shared_resources);

        return app.run(argc, argv);
    }
    catch (const Deltacast::VideoMonitor::Exceptions::VideoMonitorException& e)
    {
        spdlog::error("Application Exception: {}", e.what());
        return static_cast<int>(Deltacast::VideoMonitor::ExitCode::FailureUnexpected);
    }
    catch (const Deltacast::Wrapper::ApiException& e)
    {
        spdlog::error("API Exception: {}", e.what());
        spdlog::error("Logs: {}", e.logs());
        return static_cast<int>(Deltacast::VideoMonitor::ExitCode::FailureUnexpected);
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception: {}", e.what());
        return static_cast<int>(Deltacast::VideoMonitor::ExitCode::FailureUnexpected);
    }
}