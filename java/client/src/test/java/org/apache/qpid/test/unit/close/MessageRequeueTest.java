/*
 *  Licensed to the Apache Software Foundation (ASF) under one
 *  or more contributor license agreements.  See the NOTICE file
 *  distributed with this work for additional information
 *  regarding copyright ownership.  The ASF licenses this file
 *  to you under the Apache License, Version 2.0 (the
 *  "License"); you may not use this file except in compliance
 *  with the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an
 *  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *  KIND, either express or implied.  See the License for the
 *  specific language governing permissions and limitations
 *  under the License.
 *
 *
 */
package org.apache.qpid.test.unit.close;

import junit.framework.TestCase;

import org.apache.qpid.AMQException;
import org.apache.qpid.client.AMQConnection;
import org.apache.qpid.client.message.AbstractJMSMessage;
import org.apache.qpid.client.transport.TransportConnection;
import org.apache.qpid.testutil.QpidClientConnection;
import org.apache.qpid.url.URLSyntaxException;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageConsumer;
import javax.jms.Queue;
import javax.jms.Session;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.UUID;

public class MessageRequeueTest extends TestCase
{
    private static final Logger _logger = LoggerFactory.getLogger(MessageRequeueTest.class);

    protected static AtomicInteger consumerIds = new AtomicInteger(0);
    protected final Integer numTestMessages = 150;

    protected final int consumeTimeout = 3000;


    protected String payload = "Message:";

    protected final String BROKER = "vm://:1";
    private boolean testReception = true;

    private long[] receieved = new long[numTestMessages + 1];
    private boolean passed = false;

    protected void setUp() throws Exception
    {
        super.setUp();
        TransportConnection.createVMBroker(1);


    }

    protected void tearDown() throws Exception
    {
        super.tearDown();


        TransportConnection.killVMBroker(1);
    }

    /**
     * multiple consumers
     *
     * @throws javax.jms.JMSException if a JMS problem occurs
     * @throws InterruptedException   on timeout
     */
    public void testDrain() throws JMSException, InterruptedException
    {
        final String queueName = "direct://amq.direct//queue" + UUID.randomUUID().toString();

        QpidClientConnection conn = new QpidClientConnection(BROKER);

        conn.connect();
        // clear queue
        conn.consume(queueName, consumeTimeout);
        // load test data
        _logger.info("creating test data, " + numTestMessages + " messages");
        conn.put(queueName, payload, numTestMessages);
        // close this connection
        conn.disconnect();

        conn = new QpidClientConnection(BROKER);

        conn.connect();

        _logger.info("consuming queue " + queueName);
        Queue q = conn.getSession().createQueue(queueName);

        final MessageConsumer consumer = conn.getSession().createConsumer(q);
        int messagesReceived = 0;

        long[] messageLog = new long[numTestMessages + 1];

        _logger.info("consuming...");
        Message msg = consumer.receive(1000);
        while (msg != null)
        {
            messagesReceived++;

            long dt = ((AbstractJMSMessage) msg).getDeliveryTag();

            int msgindex = msg.getIntProperty("index");
            if (messageLog[msgindex] != 0)
            {
                _logger.error("Received Message(" + msgindex + ":" + ((AbstractJMSMessage) msg).getDeliveryTag()
                    + ") more than once.");
            }

            if (_logger.isInfoEnabled())
            {
                _logger.info("Received Message(" + System.identityHashCode(msgindex) + ") " + "DT:" + dt + "IN:" + msgindex);
            }

            if (dt == 0)
            {
                _logger.error("DT is zero for msg:" + msgindex);
            }

            messageLog[msgindex] = dt;

            // get Next message
            msg = consumer.receive(1000);
        }

        conn.getSession().commit();
        consumer.close();
        assertEquals("number of consumed messages does not match initial data", (int) numTestMessages, messagesReceived);

        int index = 0;
        StringBuilder list = new StringBuilder();
        list.append("Failed to receive:");
        int failed = 0;

        for (long b : messageLog)
        {
            if ((b == 0) && (index != 0)) // delivery tag of zero shouldn't exist
            {
                _logger.error("Index: " + index + " was not received.");
                list.append(" ");
                list.append(index);
                list.append(":");
                list.append(b);
                failed++;
            }

            index++;
        }

        assertEquals(list.toString(), 0, failed);
        _logger.info("consumed: " + messagesReceived);
        conn.disconnect();
        passed = true;

    }



    /** multiple consumers
     * Based on code subbmitted by client FT-304
     */
    public void testCompetingConsumers() throws JMSException, InterruptedException
    {
        final String queueName = "direct://amq.direct//queue" + UUID.randomUUID().toString();

        QpidClientConnection conn = new QpidClientConnection(BROKER);

        conn.connect();
        // clear queue
        conn.consume(queueName, consumeTimeout);
        // load test data
        _logger.info("creating test data, " + numTestMessages + " messages");
        conn.put(queueName, payload, numTestMessages);
        // close this connection
        conn.disconnect();

        Consumer c1 = new Consumer(queueName);
        Consumer c2 = new Consumer(queueName);
        Consumer c3 = new Consumer(queueName);
        Consumer c4 = new Consumer(queueName);

        Thread t1 = new Thread(c1);
        Thread t2 = new Thread(c2);
        Thread t3 = new Thread(c3);
        Thread t4 = new Thread(c4);

        t1.start();
        t2.start();
        t3.start();
        t4.start();

        try
        {
            t1.join();
            t2.join();
            t3.join();
            t4.join();
        }
        catch (InterruptedException e)
        {
            fail("Uanble to join to Consumer theads");
        }

        _logger.info("consumer 1 count is " + c1.getCount());
        _logger.info("consumer 2 count is " + c2.getCount());
        _logger.info("consumer 3 count is " + c3.getCount());
        _logger.info("consumer 4 count is " + c4.getCount());

        Integer totalConsumed = c1.getCount() + c2.getCount() + c3.getCount() + c4.getCount();

        // Check all messages were correctly delivered
        int index = 0;
        StringBuilder list = new StringBuilder();
        list.append("Failed to receive:");
        int failed = 0;

        for (long b : receieved)
        {
            if ((b == 0) && (index != 0)) // delivery tag of zero shouldn't exist (and we don't have msg 0)
            {
                _logger.error("Index: " + index + " was not received.");
                list.append(" ");
                list.append(index);
                list.append(":");
                list.append(b);
                failed++;
            }

            index++;
        }

        assertEquals(list.toString() + "-" + numTestMessages + "-" + totalConsumed, 0, failed);
        assertEquals("number of consumed messages does not match initial data", numTestMessages, totalConsumed);

    }

    class Consumer implements Runnable
    {
        private Integer count = 0;
        private Integer id;
        private final String _queueName;

        public Consumer(String queueName)
        {
            _queueName = queueName;
            id = consumerIds.addAndGet(1);
        }

        public void run()
        {
            try
            {
                _logger.info("consumer-" + id + ": starting");
                QpidClientConnection conn = new QpidClientConnection(BROKER);

                conn.connect();

                _logger.info("consumer-" + id + ": connected, consuming...");
                Message result;
                do
                {
                    result = conn.getNextMessage(_queueName, consumeTimeout);
                    if (result != null)
                    {

                        long dt = ((AbstractJMSMessage) result).getDeliveryTag();

                        if (testReception)
                        {
                            int msgindex = result.getIntProperty("index");
                            if (receieved[msgindex] != 0)
                            {
                                _logger.error("Received Message(" + msgindex + ":"
                                    + ((AbstractJMSMessage) result).getDeliveryTag() + ") more than once.");
                            }

                            if (_logger.isInfoEnabled())
                            {
                                _logger.info("Received Message(" + System.identityHashCode(msgindex) + ") " + "DT:" + dt
                                    + "IN:" + msgindex);
                            }

                            if (dt == 0)
                            {
                                _logger.error("DT is zero for msg:" + msgindex);
                            }

                            receieved[msgindex] = dt;
                        }

                        count++;
                        if ((count % 100) == 0)
                        {
                            _logger.info("consumer-" + id + ": got " + result + ", new count is " + count);
                        }
                    }
                }
                while (result != null);

                _logger.info("consumer-" + id + ": complete");
                conn.disconnect();

            }
            catch (Exception e)
            {
                e.printStackTrace();
            }
        }

        public Integer getCount()
        {
            return count;
        }

        public Integer getId()
        {
            return id;
        }
    }

    public void testRequeue() throws JMSException, AMQException, URLSyntaxException, InterruptedException
    {
        final String queue = "direct://amq.direct//queue" + UUID.randomUUID().toString();

        QpidClientConnection conn = new QpidClientConnection(BROKER);

        conn.connect();
        // clear queue
        conn.consume(queue, consumeTimeout);
        // load test data
        _logger.info("creating test data, " + numTestMessages + " messages");
        conn.put(queue, payload, numTestMessages);
        // close this connection
        conn.disconnect();

        int run = 0;
        // while (run < 10)
        {
            run++;

            if (_logger.isInfoEnabled())
            {
                _logger.info("testRequeue run " + run);
            }

            String virtualHost = "/test";
            String brokerlist = BROKER;
            String brokerUrl = "amqp://guest:guest@" + virtualHost + "?brokerlist='" + brokerlist + "'";

            AMQConnection amqConn = new AMQConnection(brokerUrl);
            Session session = amqConn.createSession(false, Session.CLIENT_ACKNOWLEDGE);
            Queue q = session.createQueue(queue);

            _logger.debug("Create Consumer");
            MessageConsumer consumer = session.createConsumer(q);

            amqConn.start();

            _logger.debug("Receiving msg");
            Message msg = consumer.receive(2000);

            assertNotNull("Message should not be null", msg);

            // As we have not ack'd message will be requeued.
            _logger.debug("Close Consumer");
            consumer.close();

            _logger.debug("Close Connection");
            amqConn.close();
        }
    }

}
