/*
 *
 * Copyright (c) 2006 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include "BrokerAdapter.h"
#include "Session.h"
#include "SessionAdapter.h"
#include "Connection.h"
#include "DeliveryToken.h"
#include "MessageDelivery.h"
#include "qpid/framing/AMQMethodBody.h"
#include "qpid/Exception.h"

namespace qpid {
namespace broker {

using namespace qpid;
using namespace qpid::framing;

typedef std::vector<Queue::shared_ptr> QueueVector;

// FIXME aconway 2007-08-31: now that functionality is distributed
// between different handlers, BrokerAdapter should be dropped.
// Instead the individual class Handler interfaces can be implemented
// by the handlers responsible for those classes.
//

BrokerAdapter::BrokerAdapter(Session& s, ChannelAdapter& a) :
    CoreRefs(s,
             s.getAdapter()->getConnection(),
             s.getAdapter()->getConnection().broker,
             a),
    basicHandler(*this),
    exchangeHandler(*this),
    bindingHandler(*this),
    messageHandler(*this),
    queueHandler(*this),
    txHandler(*this),
    dtxHandler(*this)
{}


ProtocolVersion BrokerAdapter::getVersion() const {
    return connection.getVersion();
}

void BrokerAdapter::ExchangeHandlerImpl::declare(uint16_t /*ticket*/, const string& exchange, const string& type, 
                                                 const string& alternateExchange, 
                                                 bool passive, bool durable, bool /*autoDelete*/, const FieldTable& args){
    Exchange::shared_ptr alternate;
    if (!alternateExchange.empty()) {
        alternate = broker.getExchanges().get(alternateExchange);
    }
    if(passive){
        Exchange::shared_ptr actual(broker.getExchanges().get(exchange));
        checkType(actual, type);
        checkAlternate(actual, alternate);
    }else{        
        try{
            std::pair<Exchange::shared_ptr, bool> response = broker.getExchanges().declare(exchange, type, durable, args);
            if (response.second) {
                if (durable) {
                    broker.getStore().create(*response.first);
                }
                if (alternate) {
                    response.first->setAlternate(alternate);
                    alternate->incAlternateUsers();
                }
            } else {
                checkType(response.first, type);
                checkAlternate(response.first, alternate);
            }
        }catch(UnknownExchangeTypeException& e){
            throw ConnectionException(
                503, "Exchange type not implemented: " + type);
        }
    }
}

void BrokerAdapter::ExchangeHandlerImpl::checkType(Exchange::shared_ptr exchange, const std::string& type)
{
    if (!type.empty() && exchange->getType() != type) {
        throw ConnectionException(530, "Exchange declared to be of type " + exchange->getType() + ", requested " + type);
    }
}

void BrokerAdapter::ExchangeHandlerImpl::checkAlternate(Exchange::shared_ptr exchange, Exchange::shared_ptr alternate)
{
    if (alternate && alternate != exchange->getAlternate()) {
        throw ConnectionException(530, "Exchange declared with alternate-exchange "
                                  + exchange->getAlternate()->getName() + ", requested " 
                                  + alternate->getName());
    }

}
                
void BrokerAdapter::ExchangeHandlerImpl::delete_(uint16_t /*ticket*/, const string& name, bool /*ifUnused*/){
    //TODO: implement unused
    Exchange::shared_ptr exchange(broker.getExchanges().get(name));
    if (exchange->inUseAsAlternate()) throw ConnectionException(530, "Exchange in use as alternate-exchange.");
    if (exchange->isDurable()) broker.getStore().destroy(*exchange);
    if (exchange->getAlternate()) exchange->getAlternate()->decAlternateUsers();
    broker.getExchanges().destroy(name);
} 

ExchangeQueryResult BrokerAdapter::ExchangeHandlerImpl::query(u_int16_t /*ticket*/, const string& name)
{
    try {
        Exchange::shared_ptr exchange(broker.getExchanges().get(name));
        return ExchangeQueryResult(exchange->getType(), exchange->isDurable(), false, exchange->getArgs());
    } catch (const ChannelException& e) {
        return ExchangeQueryResult("", false, true, FieldTable());        
    }
}

BindingQueryResult BrokerAdapter::BindingHandlerImpl::query(u_int16_t /*ticket*/,
                                                            const std::string& exchangeName,
                                                            const std::string& queueName,
                                                            const std::string& key,
                                                            const framing::FieldTable& args)
{
    Exchange::shared_ptr exchange;
    try {
        exchange = broker.getExchanges().get(exchangeName);
    } catch (const ChannelException&) {}

    Queue::shared_ptr queue;
    if (!queueName.empty()) {
        queue = broker.getQueues().find(queueName);
    }

    if (!exchange) {
        return BindingQueryResult(true, false, false, false, false);
    } else if (!queueName.empty() && !queue) {
        return BindingQueryResult(false, true, false, false, false);
    } else if (exchange->isBound(queue, key.empty() ? 0 : &key, args.count() > 0 ? &args : &args)) {
        return BindingQueryResult(false, false, false, false, false);
    } else {
        //need to test each specified option individually
        bool queueMatched = queueName.empty() || exchange->isBound(queue, 0, 0);
        bool keyMatched = key.empty() || exchange->isBound(Queue::shared_ptr(), &key, 0);
        bool argsMatched = args.count() == 0 || exchange->isBound(Queue::shared_ptr(), 0, &args);

        return BindingQueryResult(false, false, !queueMatched, !keyMatched, !argsMatched);
    }
}

QueueQueryResult BrokerAdapter::QueueHandlerImpl::query(const string& name)
{
    Queue::shared_ptr queue = session.getQueue(name);
    Exchange::shared_ptr alternateExchange = queue->getAlternateExchange();

    return QueueQueryResult(queue->getName(), 
                            alternateExchange ? alternateExchange->getName() : "", 
                            queue->isDurable(), 
                            queue->hasExclusiveOwner(),
                            queue->isAutoDelete(),
                            queue->getSettings(),
                            queue->getMessageCount(),
                            queue->getConsumerCount());
}

void BrokerAdapter::QueueHandlerImpl::declare(uint16_t /*ticket*/, const string& name, const string& alternateExchange,
                                              bool passive, bool durable, bool exclusive, 
                                              bool autoDelete, const qpid::framing::FieldTable& arguments){
 
    Exchange::shared_ptr alternate;
    if (!alternateExchange.empty()) {
        alternate = broker.getExchanges().get(alternateExchange);
    }
    Queue::shared_ptr queue;
    if (passive && !name.empty()) {
	queue = session.getQueue(name);
        //TODO: check alternate-exchange is as expected
    } else {
	std::pair<Queue::shared_ptr, bool> queue_created =  
            broker.getQueues().declare(
                name, durable,
                autoDelete && !exclusive,
                exclusive ? &connection : 0);
	queue = queue_created.first;
	assert(queue);
	if (queue_created.second) { // This is a new queue
	    session.setDefaultQueue(queue);
            if (alternate) {
                queue->setAlternateExchange(alternate);
                alternate->incAlternateUsers();
            }

            //apply settings & create persistent record if required
            queue_created.first->create(arguments);

	    //add default binding:
	    broker.getExchanges().getDefault()->bind(queue, name, 0);
            queue->bound(broker.getExchanges().getDefault()->getName(), name, arguments);

            //handle automatic cleanup:
	    if (exclusive) {
		connection.exclusiveQueues.push_back(queue);
	    }
	}
    }
    if (exclusive && !queue->isExclusiveOwner(&connection)) 
	throw ResourceLockedException(
            QPID_MSG("Cannot grant exclusive access to queue "
                     << queue->getName()));
} 
        
void BrokerAdapter::QueueHandlerImpl::bind(uint16_t /*ticket*/, const string& queueName, 
                                           const string& exchangeName, const string& routingKey, 
                                           const FieldTable& arguments){

    Queue::shared_ptr queue = session.getQueue(queueName);
    Exchange::shared_ptr exchange = broker.getExchanges().get(exchangeName);
    if(exchange){
        string exchangeRoutingKey = routingKey.empty() && queueName.empty() ? queue->getName() : routingKey;
        if (exchange->bind(queue, exchangeRoutingKey, &arguments)) {
            queue->bound(exchangeName, routingKey, arguments);
            if (exchange->isDurable() && queue->isDurable()) {
                broker.getStore().bind(*exchange, *queue, routingKey, arguments);
            }
        }
    }else{
        throw NotFoundException(
            "Bind failed. No such exchange: " + exchangeName);
    }
}
 
void 
BrokerAdapter::QueueHandlerImpl::unbind(uint16_t /*ticket*/,
                                        const string& queueName,
                                        const string& exchangeName,
                                        const string& routingKey,
                                        const qpid::framing::FieldTable& arguments )
{
    Queue::shared_ptr queue = session.getQueue(queueName);
    if (!queue.get()) throw NotFoundException("Unbind failed. No such exchange: " + exchangeName);

    Exchange::shared_ptr exchange = broker.getExchanges().get(exchangeName);
    if (!exchange.get()) throw NotFoundException("Unbind failed. No such exchange: " + exchangeName);

    if (exchange->unbind(queue, routingKey, &arguments) && exchange->isDurable() && queue->isDurable()) {
        broker.getStore().unbind(*exchange, *queue, routingKey, arguments);
    }

}
        
void BrokerAdapter::QueueHandlerImpl::purge(uint16_t /*ticket*/, const string& queue){
    session.getQueue(queue)->purge();
} 
        
void BrokerAdapter::QueueHandlerImpl::delete_(uint16_t /*ticket*/, const string& queue, bool ifUnused, bool ifEmpty){
    ChannelException error(0, "");
    Queue::shared_ptr q = session.getQueue(queue);
    if(ifEmpty && q->getMessageCount() > 0){
        throw PreconditionFailedException("Queue not empty.");
    }else if(ifUnused && q->getConsumerCount() > 0){
        throw PreconditionFailedException("Queue in use.");
    }else{
        //remove the queue from the list of exclusive queues if necessary
        if(q->isExclusiveOwner(&connection)){
            QueueVector::iterator i = find(connection.exclusiveQueues.begin(), connection.exclusiveQueues.end(), q);
            if(i < connection.exclusiveQueues.end()) connection.exclusiveQueues.erase(i);
        }
        q->destroy();
        broker.getQueues().destroy(queue);
        q->unbind(broker.getExchanges(), q);
    }
} 
              
        


void BrokerAdapter::BasicHandlerImpl::qos(uint32_t prefetchSize, uint16_t prefetchCount, bool /*global*/){
    //TODO: handle global
    session.setPrefetchSize(prefetchSize);
    session.setPrefetchCount(prefetchCount);
} 
        
void BrokerAdapter::BasicHandlerImpl::consume(uint16_t /*ticket*/, 
                                              const string& queueName, const string& consumerTag, 
                                              bool noLocal, bool noAck, bool exclusive, 
                                              bool nowait, const FieldTable& fields)
{
    
    Queue::shared_ptr queue = session.getQueue(queueName);    
    if(!consumerTag.empty() && session.exists(consumerTag)){
        throw ConnectionException(530, "Consumer tags must be unique");
    }
    string newTag = consumerTag;
    //need to generate name here, so we have it for the adapter (it is
    //also version specific behaviour now)
    if (newTag.empty()) newTag = tagGenerator.generate();
    DeliveryToken::shared_ptr token(MessageDelivery::getBasicConsumeToken(newTag));
    session.consume(token, newTag, queue, noLocal, !noAck, true, exclusive, &fields);

    if(!nowait) client.consumeOk(newTag);

    //allow messages to be dispatched if required as there is now a consumer:
    queue->requestDispatch();
} 
        
void BrokerAdapter::BasicHandlerImpl::cancel(const string& consumerTag){
    session.cancel(consumerTag);
} 
        
void BrokerAdapter::BasicHandlerImpl::get(uint16_t /*ticket*/, const string& queueName, bool noAck){
    Queue::shared_ptr queue = session.getQueue(queueName);    
    DeliveryToken::shared_ptr token(MessageDelivery::getBasicGetToken(queue));
    if(!session.get(token, queue, !noAck)){
        string clusterId;//not used, part of an imatix hack

        client.getEmpty(clusterId);
    }
} 
        
void BrokerAdapter::BasicHandlerImpl::ack(uint64_t deliveryTag, bool multiple){
    if (multiple) {
        session.ackCumulative(deliveryTag);
    } else {
        session.ackRange(deliveryTag, deliveryTag);
    }
} 
        
void BrokerAdapter::BasicHandlerImpl::reject(uint64_t /*deliveryTag*/, bool /*requeue*/){} 
        
void BrokerAdapter::BasicHandlerImpl::recover(bool requeue)
{
    session.recover(requeue);
} 

void BrokerAdapter::TxHandlerImpl::select()
{
    session.startTx();
}

void BrokerAdapter::TxHandlerImpl::commit()
{
    session.commit(&broker.getStore());
}

void BrokerAdapter::TxHandlerImpl::rollback()
{    
    session.rollback();
    session.recover(false);    
}
              
}} // namespace qpid::broker

