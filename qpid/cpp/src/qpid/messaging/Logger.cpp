/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include "qpid/messaging/Logger.h"

#include "qpid/log/Logger.h"

namespace qpid {
namespace messaging {

// Proxy class to call the users output class/routine
class ProxyOutput : public qpid::log::Logger::Output {
    LoggerOutput& output;

    void log(const qpid::log::Statement&, const std::string& message) {
        output.log(message);
    }

public:
    ProxyOutput(LoggerOutput& o) :
        output(o)
    {}
};

inline qpid::log::Logger& logger() {
    static qpid::log::Logger& theLogger=qpid::log::Logger::instance();
    return theLogger;
}

void Logger::configure(int argc, char* argv[])
{
    qpid::log::Options options(argc > 0 ? argv[0] : "");
    options.parse(argc, argv);
    logger().configure(options);
}

void Logger::setOutput(LoggerOutput& o)
{
    logger().output(std::auto_ptr<qpid::log::Logger::Output>(new ProxyOutput(o)));
}

}} // namespace qpid::messaging
