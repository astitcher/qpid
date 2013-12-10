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

#include <qpid/messaging/Connection.h>
#include <qpid/messaging/Logger.h>

#include <iostream>
#include <memory>
#include <stdexcept>

class MyLogger : public qpid::messaging::LoggerOutput {
    std::ostream& outStream;

    void log(qpid::messaging::Level /*level*/, const char* file, int line, const char* function, const std::string& message){
        outStream << file << ": " << function << ":" << line << ": " << message;
    }

public:
    MyLogger(std::ostream& os) :
        outStream(os)
    {}
};

int main(int argc, char* argv[]) {
    try {
        qpid::messaging::Logger::configure(argc, argv, "qpid");
        MyLogger logger(std::cout);
        qpid::messaging::Logger::setOutput(logger);

        using qpid::messaging::Connection;
        Connection c("localhost");

        c.open();
    } catch (std::exception& e) {
        qpid::messaging::Logger::log(qpid::messaging::critical, __FILE__, __LINE__, __FUNCTION__  , std::string("Caught exception: ") + e.what());
    }
}
