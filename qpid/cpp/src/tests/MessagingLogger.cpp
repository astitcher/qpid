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

#include "qpid/log/Statement.h"
#include "qpid/messaging/Connection.h"
#include "qpid/messaging/Logger.h"

#include <iostream>
#include <memory>
#include <stdexcept>

#include <vector>

#define UNIT_TEST
#ifdef UNIT_TEST

#include "unit_test.h"

namespace qpid {
namespace tests {

QPID_AUTO_TEST_SUITE(MessagingLoggerSuite)

class StringLogger : public qpid::messaging::LoggerOutput {
    std::string& outString;

    void log(qpid::messaging::Level /*level*/, const char* /*file*/, int /*line*/, const char* /*function*/, const std::string& message){
        outString += message;
    }

public:
    StringLogger(std::string& os) :
        outString(os)
    {}
};


std::string logOutput;

QPID_AUTO_TEST_CASE(testLogger)
{
    StringLogger logger(logOutput);

    const char* args[]={"", "--log-enable", "debug", 0};
    qpid::messaging::Logger::configure(3, args);
    logOutput.clear();
    qpid::messaging::Logger::setOutput(logger);
    QPID_LOG(trace, "trace level output");
    QPID_LOG(debug, "debug level output");
    QPID_LOG(info, "info level output");
    QPID_LOG(notice, "notice level output");
    QPID_LOG(warning, "warning level output");
    QPID_LOG(critical, "critical level output");

    BOOST_CHECK_EQUAL(logOutput, "debug level output\ncritical level output\n");
}

QPID_AUTO_TEST_SUITE_END()
}}

#else

namespace qpid {
namespace tests {

class MyLogger : public qpid::messaging::LoggerOutput {
    std::ostream& outStream;

    void log(qpid::messaging::Level /*level*/, const char* file, int line, const char* function, const std::string& message){
        outStream << file << ":" << line << ":["<< function << "()]:"  << message;
    }

public:
    MyLogger(std::ostream& os) :
        outStream(os)
    {}
};

std::string UsageOption("--help");

int main(int argc, char* argv[]) {
    std::vector<const char*> args(&argv[0], &argv[argc]);

    bool showUsage = false;
    for (int i=0; i<argc; ++i){
        if ( UsageOption == args[i] ) showUsage = true;
    }

    try {
        qpid::messaging::Logger::configure(argc, argv, "qpid");
    } catch (std::exception& e) {
        std::cerr << "Caught exception configuring logger: " << e.what() << "\n";
        showUsage = true;
    }

    try {
        if (showUsage) {
            std::cerr << qpid::messaging::Logger::usage();
            return 0;
        }

        MyLogger logger(std::cout);
        qpid::messaging::Logger::setOutput(logger);

        using qpid::messaging::Connection;
        Connection c("localhost");

        c.open();
    } catch (std::exception& e) {
        qpid::messaging::Logger::log(qpid::messaging::critical, __FILE__, __LINE__, __FUNCTION__  , std::string("Caught exception: ") + e.what());
    }

    return 0;
}

}}

int main(int argc, char* argv[]) {
    return qpid::tests::main(argc, argv);
}
#endif
