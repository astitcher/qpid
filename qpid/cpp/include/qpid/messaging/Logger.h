#ifndef QPID_MESSAGING_LOGGING_H
#define QPID_MESSAGING_LOGGING_H

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

#include "qpid/messaging/ImportExport.h"

#include <string>

namespace qpid {
namespace messaging {
/**
 * These log levels need to be kept in sync with the log levels
 * defined internally in qpid::log (but I don't think they are likely
 * to change anyway
 */
enum Level { trace, debug, info, notice, warning, error, critical };

/**
 * Interface class to allow redirection of log output
 */
class QPID_MESSAGING_CLASS_EXTERN LoggerOutput
{
public:
    virtual void log(Level level, const char* file, int line, const char* function, const std::string& message) = 0;
};

/**
 * A utility class to allow the application to control the logging
 * output of the qpid messaging library
 */
class QPID_MESSAGING_CLASS_EXTERN Logger
{
public:
    /**
     *
     */
    QPID_MESSAGING_EXTERN static void configure(int argc, char* argv[], const std::string& prefix=std::string());

    /**
     * 
     */
    QPID_MESSAGING_EXTERN static std::string usage();

    /**
     *
     */
    QPID_MESSAGING_EXTERN static void setOutput(LoggerOutput& output);

    /**
     *
     */
    QPID_MESSAGING_EXTERN static void log(Level level, const char* file, int line, const char* function, const std::string& message);

private:
    //This class has only one instance so no need to copy
    Logger();
    ~Logger();

    Logger(const Logger&);
    Logger operator=(const Logger&);
};
}} // namespace qpid::messaging

#endif  /*!QPID_MESSAGING_LOGGING_H*/
