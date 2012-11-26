#ifndef _sys_ProtocolFactory_h
#define _sys_ProtocolFactory_h

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

#include "qpid/SharedObject.h"
#include "qpid/broker/Broker.h"
#include "qpid/sys/IntegerTypes.h"
#include "qpid/sys/ConnectionCodec.h"
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/function.hpp>

namespace qpid {
namespace sys {

class Poller;

class ProtocolFactory : public qpid::SharedObject<ProtocolFactory>
{
  public:
    typedef boost::function2<void, int, std::string> ConnectFailedCallback;

    virtual ~ProtocolFactory() = 0;
    virtual uint16_t getPort() const = 0;
    virtual void accept(boost::shared_ptr<Poller>, ConnectionCodec::Factory*) = 0;
    virtual void connect(
        boost::shared_ptr<Poller>,
        const std::string& name,
        const std::string& host, const std::string& port,
        ConnectionCodec::Factory* codec,
        ConnectFailedCallback failed) = 0;
};

inline ProtocolFactory::~ProtocolFactory() {}

class Socket;
typedef boost::function0<Socket*> SocketFactory;
uint16_t listenTo(const std::vector<std::string>& interfaces, const std::string& port, int backlog,
                  const SocketFactory& factory,
                  /*out*/boost::ptr_vector<Socket>& listeners);

void establishedIncoming(boost::shared_ptr<Poller>, const qpid::broker::Broker::Options& opts, Timer* timer,
                         const Socket& s, ConnectionCodec::Factory* f);

void establishedOutgoing(boost::shared_ptr<Poller>, const qpid::broker::Broker::Options& opts, Timer* timer,
                         const Socket& s, ConnectionCodec::Factory* f, const std::string& name);

void connectFailed(const Socket&, int, const std::string&, ProtocolFactory::ConnectFailedCallback);

void connect(
    boost::shared_ptr<Poller> poller, const qpid::broker::Broker::Options& opts, Timer* timer,
    const SocketFactory& factory,
    const std::string& name,
    const std::string& host, const std::string& port,
    ConnectionCodec::Factory* fact,
    ProtocolFactory::ConnectFailedCallback failed);
}}


    
#endif  //!_sys_ProtocolFactory_h
