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
#include "qpid/log/OstreamOutput.h"

#include <string>
#include <vector>

using std::string;
using std::vector;

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

void Logger::configure(int argc, char* argv[], const string& pre)
{
    bool logToStdout;
    bool logToStderr;
    string logFile;
    std::vector<std::string> selectors;
    std::vector<std::string> deselectors;
    bool time, level, thread, source, function, hiresTs;

    selectors.push_back("notice+");

    string prefix = pre.empty() ? pre : pre+"-";
    qpid::Options myOptions;
    myOptions.addOptions()
    ((prefix+"log-enable").c_str(), optValue(selectors, "RULE"),
     ("Enables logging for selected levels and components. "
     "RULE is in the form 'LEVEL[+-][:PATTERN]'\n"
     "LEVEL is one of: \n\t "+qpid::log::getLevels()+"\n"
     "PATTERN is a logging category name, or a namespace-qualified "
     "function name or name fragment. "
     "Logging category names are: \n\t "+qpid::log::getCategories()+"\n"
     "For example:\n"
     "\t'--log-enable warning+'\n"
     "logs all warning, error and critical messages.\n"
     "\t'--log-enable trace+:Broker'\n"
     "logs all category 'Broker' messages.\n"
     "\t'--log-enable debug:framing'\n"
     "logs debug messages from all functions with 'framing' in the namespace or function name.\n"
     "This option can be used multiple times").c_str())
    ((prefix+"log-disable").c_str(), optValue(deselectors, "RULE"),
     ("Disables logging for selected levels and components. "
     "RULE is in the form 'LEVEL[+-][:PATTERN]'\n"
     "LEVEL is one of: \n\t "+qpid::log::getLevels()+"\n"
     "PATTERN is a logging category name, or a namespace-qualified "
     "function name or name fragment. "
     "Logging category names are: \n\t "+qpid::log::getCategories()+"\n"
     "For example:\n"
     "\t'--log-disable warning-'\n"
     "disables logging all warning, notice, info, debug, and trace messages.\n"
     "\t'--log-disable trace:Broker'\n"
     "disables all category 'Broker' trace messages.\n"
     "\t'--log-disable debug-:qmf::'\n"
     "disables logging debug and trace messages from all functions with 'qmf::' in the namespace.\n"
     "This option can be used multiple times").c_str())
    ((prefix+"log-time").c_str(), optValue(time, "yes|no"), "Include time in log messages")
    ((prefix+"log-level").c_str(), optValue(level,"yes|no"), "Include severity level in log messages")
    ((prefix+"log-source").c_str(), optValue(source,"yes|no"), "Include source file:line in log messages")
    ((prefix+"log-thread").c_str(), optValue(thread,"yes|no"), "Include thread ID in log messages")
    ((prefix+"log-function").c_str(), optValue(function,"yes|no"), "Include function signature in log messages")
    ((prefix+"log-hires-timestamp").c_str(), optValue(hiresTs,"yes|no"), "Use hi-resolution timestamps in log messages")
    ((prefix+"log-to-stderr").c_str(), optValue(logToStderr, "yes|no"), "Send logging output to stderr")
    ((prefix+"log-to-stdout").c_str(), optValue(logToStdout, "yes|no"), "Send logging output to stdout")
    ((prefix+"log-to-file").c_str(), optValue(logFile, "FILE"), "Send log output to FILE.")
    ;

    // Parse the command line not failing for unrecognised options
    myOptions.parse(argc, argv, std::string(), true);

    // Set the logger options according to what we just parsed
    qpid::log::Options logOptions;
    logOptions.selectors = selectors;
    logOptions.deselectors = deselectors;
    logOptions.time = time;
    logOptions.level = level;
    logOptions.category = false;
    logOptions.thread = thread;
    logOptions.source = source;
    logOptions.function = function;
    logOptions.hiresTs = hiresTs;

    logger().clear(); // Need to clear before configuring as it will have been initialised statically already
    logger().format(logOptions);
    logger().select(qpid::log::Selector(logOptions));

    // Have to set up the standard output sinks manually
    if (logToStderr)
        logger().output(std::auto_ptr<qpid::log::Logger::Output>
                       (new qpid::log::OstreamOutput(std::clog)));
    if (logToStdout)
        logger().output(std::auto_ptr<qpid::log::Logger::Output>
                       (new qpid::log::OstreamOutput(std::cout)));

    if (logFile.length() > 0)
        logger().output(std::auto_ptr<qpid::log::Logger::Output>
                         (new qpid::log::OstreamOutput(logFile)));
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
