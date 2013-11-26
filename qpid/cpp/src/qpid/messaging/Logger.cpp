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

#include <sstream>
#include <string>

using std::ostringstream;
using std::string;

namespace qpid {
namespace messaging {

// Proxy class to call the users output class/routine
class ProxyOutput : public qpid::log::Logger::Output {
    LoggerOutput& output;

    void log(const qpid::log::Statement& s, const string& message) {
        output.log(qpid::messaging::Level(s.level), s.file, s.line, s.function, message);
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
    std::vector<std::string> selectors;
    std::vector<std::string> deselectors;
    bool time, level, thread, source, function, hiresTs;

    selectors.push_back("notice+");

    ostringstream levels;
    levels << qpid::log::LevelTraits::name(qpid::log::Level(0));
    for (int i = 1; i < qpid::log::LevelTraits::COUNT; ++i)
        levels << " " << qpid::log::LevelTraits::name(qpid::log::Level(i));

    ostringstream categories;
    categories << qpid::log::CategoryTraits::name(qpid::log::Category(0));
    for (int i = 1; i < qpid::log::CategoryTraits::COUNT; ++i)
        categories << " " << qpid::log::CategoryTraits::name(qpid::log::Category(i));


    qpid::Options myOptions;
    myOptions.addOptions()
    ("log-enable", optValue(selectors, "RULE"),
     ("Enables logging for selected levels and components. "
     "RULE is in the form 'LEVEL[+-][:PATTERN]'\n"
     "LEVEL is one of: \n\t "+levels.str()+"\n"
     "PATTERN is a logging category name, or a namespace-qualified "
     "function name or name fragment. "
     "Logging category names are: \n\t "+categories.str()+"\n"
     "For example:\n"
     "\t'--log-enable warning+'\n"
     "logs all warning, error and critical messages.\n"
     "\t'--log-enable trace+:Broker'\n"
     "logs all category 'Broker' messages.\n"
     "\t'--log-enable debug:framing'\n"
     "logs debug messages from all functions with 'framing' in the namespace or function name.\n"
     "This option can be used multiple times").c_str())
    ("log-disable", optValue(deselectors, "RULE"),
     ("Disables logging for selected levels and components. "
     "RULE is in the form 'LEVEL[+-][:PATTERN]'\n"
     "LEVEL is one of: \n\t "+levels.str()+"\n"
     "PATTERN is a logging category name, or a namespace-qualified "
     "function name or name fragment. "
     "Logging category names are: \n\t "+categories.str()+"\n"
     "For example:\n"
     "\t'--log-disable warning-'\n"
     "disables logging all warning, notice, info, debug, and trace messages.\n"
     "\t'--log-disable trace:Broker'\n"
     "disables all category 'Broker' trace messages.\n"
     "\t'--log-disable debug-:qmf::'\n"
     "disables logging debug and trace messages from all functions with 'qmf::' in the namespace.\n"
     "This option can be used multiple times").c_str())
    ("log-time", optValue(time, "yes|no"), "Include time in log messages")
    ("log-level", optValue(level,"yes|no"), "Include severity level in log messages")
    ("log-source", optValue(source,"yes|no"), "Include source file:line in log messages")
    ("log-thread", optValue(thread,"yes|no"), "Include thread ID in log messages")
    ("log-function", optValue(function,"yes|no"), "Include function signature in log messages")
    ("log-hires-timestamp", optValue(hiresTs,"yes|no"), "Use hi-resolution timestamps in log messages")
    ;

    myOptions.parse(argc, argv, std::string(), true);
    logger().format(myOptions);
    logger().select(Selector(myOptions));
}

void Logger::setOutput(LoggerOutput& o)
{
    logger().output(std::auto_ptr<qpid::log::Logger::Output>(new ProxyOutput(o)));
}

void Logger::log(Level level, const char* file, int line, const char* function, const string& message)
{
    qpid::log::Statement s = {
        true,
        file,
        line,
        function,
        qpid::log::Level(level),
        qpid::log::unspecified,
    };
    logger().log(s, message);
}
}} // namespace qpid::messaging
