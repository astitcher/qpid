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
package org.apache.qpid.client;

import org.apache.log4j.Logger;
import org.apache.mina.common.ByteBuffer;
import org.apache.qpid.AMQException;
import org.apache.qpid.client.message.AbstractJMSMessage;
import org.apache.qpid.client.message.JMSBytesMessage;
import org.apache.qpid.client.protocol.AMQProtocolHandler;
import org.apache.qpid.framing.*;

import javax.jms.*;
import java.io.UnsupportedEncodingException;
import java.util.Enumeration;

public class BasicMessageProducer extends Closeable implements org.apache.qpid.jms.MessageProducer
{
    protected final Logger _logger = Logger.getLogger(getClass());

    private AMQConnection _connection;

    /**
     * If true, messages will not get a timestamp.
     */
    private boolean _disableTimestamps;

    /**
     * Priority of messages created by this producer.
     */
    private int _messagePriority;

    /**
     * Time to live of messages. Specified in milliseconds but AMQ has 1 second resolution.
     */
    private long _timeToLive;

    /**
     * Delivery mode used for this producer.
     */
    private int _deliveryMode = DeliveryMode.PERSISTENT;

    /**
     * The Destination used for this consumer, if specified upon creation.
     */
    protected AMQDestination _destination;

    /**
     * Default encoding used for messages produced by this producer.
     */
    private String _encoding;

    /**
     * Default encoding used for message produced by this producer.
     */
    private String _mimeType;

    private AMQProtocolHandler _protocolHandler;

    /**
     * True if this producer was created from a transacted session
     */
    private boolean _transacted;

    private int _channelId;

    /**
     * This is an id generated by the session and is used to tie individual producers to the session. This means we
     * can deregister a producer with the session when the producer is clsoed. We need to be able to tie producers
     * to the session so that when an error is propagated to the session it can close the producer (meaning that
     * a client that happens to hold onto a producer reference will get an error if he tries to use it subsequently).
     */
    private long _producerId;

    /**
     * The session used to create this producer
     */
    private AMQSession _session;

    private final boolean _immediate;

    private final boolean _mandatory;

    private final boolean _waitUntilSent;
    private static final ContentBody[] NO_CONTENT_BODIES = new ContentBody[0];

    protected BasicMessageProducer(AMQConnection connection, AMQDestination destination, boolean transacted,
                                   int channelId, AMQSession session, AMQProtocolHandler protocolHandler,
                                   long producerId, boolean immediate, boolean mandatory, boolean waitUntilSent)
    {
        _connection = connection;
        _destination = destination;
        _transacted = transacted;
        _protocolHandler = protocolHandler;
        _channelId = channelId;
        _session = session;
        _producerId = producerId;
        if (destination != null)
        {
            declareDestination(destination);
        }
        _immediate = immediate;
        _mandatory = mandatory;
        _waitUntilSent = waitUntilSent;
    }

    void resubscribe() throws AMQException
    {
        if (_destination != null)
        {
            declareDestination(_destination);
        }
    }

    private void declareDestination(AMQDestination destination)
    {
        // Declare the exchange
        // Note that the durable and internal arguments are ignored since passive is set to false
        // AMQP version change: Hardwire the version to 0-8 (major=8, minor=0)
        // TODO: Connect this to the session version obtained from ProtocolInitiation for this session.
        // Be aware of possible changes to parameter order as versions change.
        AMQFrame declare = ExchangeDeclareBody.createAMQFrame(_channelId,
            (byte)8, (byte)0,	// AMQP version (major, minor)
            null,	// arguments
            false,	// autoDelete
            false,	// durable
            destination.getExchangeName(),	// exchange
            false,	// internal
            true,	// nowait
            false,	// passive
            0,	// ticket
            destination.getExchangeClass());	// type
        _protocolHandler.writeFrame(declare);
    }

    public void setDisableMessageID(boolean b) throws JMSException
    {
        checkPreConditions();
        checkNotClosed();
        // IGNORED
    }

    public boolean getDisableMessageID() throws JMSException
    {
        checkNotClosed();
        // Always false for AMQP
        return false;
    }

    public void setDisableMessageTimestamp(boolean b) throws JMSException
    {
        checkPreConditions();
        _disableTimestamps = b;
    }

    public boolean getDisableMessageTimestamp() throws JMSException
    {
        checkNotClosed();
        return _disableTimestamps;
    }

    public void setDeliveryMode(int i) throws JMSException
    {
        checkPreConditions();
        if (i != DeliveryMode.NON_PERSISTENT && i != DeliveryMode.PERSISTENT)
        {
            throw new JMSException("DeliveryMode must be either NON_PERSISTENT or PERSISTENT. Value of " + i +
                    " is illegal");
        }
        _deliveryMode = i;
    }

    public int getDeliveryMode() throws JMSException
    {
        checkNotClosed();
        return _deliveryMode;
    }

    public void setPriority(int i) throws JMSException
    {
        checkPreConditions();
        if (i < 0 || i > 9)
        {
            throw new IllegalArgumentException("Priority of " + i + " is illegal. Value must be in range 0 to 9");
        }
        _messagePriority = i;
    }

    public int getPriority() throws JMSException
    {
        checkNotClosed();
        return _messagePriority;
    }

    public void setTimeToLive(long l) throws JMSException
    {
        checkPreConditions();
        if (l < 0)
        {
            throw new IllegalArgumentException("Time to live must be non-negative - supplied value was " + l);
        }
        _timeToLive = l;
    }

    public long getTimeToLive() throws JMSException
    {
        checkNotClosed();
        return _timeToLive;
    }

    public Destination getDestination() throws JMSException
    {
        checkNotClosed();
        return _destination;
    }

    public void close() throws JMSException
    {
        _closed.set(true);
        _session.deregisterProducer(_producerId);
    }

    public void send(Message message) throws JMSException
    {
        checkPreConditions();
        checkInitialDestination();


        synchronized (_connection.getFailoverMutex())
        {
            sendImpl(_destination, message, _deliveryMode, _messagePriority, _timeToLive,
                     _mandatory, _immediate);
        }
    }

    public void send(Message message, int deliveryMode) throws JMSException
    {
        checkPreConditions();
        checkInitialDestination();

        synchronized (_connection.getFailoverMutex())
        {
            sendImpl(_destination, message, deliveryMode, _messagePriority, _timeToLive,
                     _mandatory, _immediate);
        }
    }

    public void send(Message message, int deliveryMode, boolean immediate) throws JMSException
    {
        checkPreConditions();
        checkInitialDestination();
        synchronized (_connection.getFailoverMutex())
        {
            sendImpl(_destination, message, deliveryMode, _messagePriority, _timeToLive,
                     _mandatory, immediate);
        }
    }

    public void send(Message message, int deliveryMode, int priority,
                     long timeToLive) throws JMSException
    {
        checkPreConditions();
        checkInitialDestination();
        synchronized (_connection.getFailoverMutex())
        {
            sendImpl(_destination, message, deliveryMode, priority, timeToLive, _mandatory,
                     _immediate);
        }
    }

    public void send(Destination destination, Message message) throws JMSException
    {
        checkPreConditions();
        checkDestination(destination);
        synchronized (_connection.getFailoverMutex())
        {
            validateDestination(destination);
            sendImpl((AMQDestination) destination, message, _deliveryMode, _messagePriority, _timeToLive,
                     _mandatory, _immediate);
        }
    }

    public void send(Destination destination, Message message, int deliveryMode,
                     int priority, long timeToLive)
            throws JMSException
    {
        checkPreConditions();
        checkDestination(destination);
        synchronized (_connection.getFailoverMutex())
        {
            validateDestination(destination);
            sendImpl((AMQDestination) destination, message, deliveryMode, priority, timeToLive,
                     _mandatory, _immediate);
        }
    }

    public void send(Destination destination, Message message, int deliveryMode,
                     int priority, long timeToLive, boolean mandatory)
            throws JMSException
    {
        checkPreConditions();
        checkDestination(destination);
        synchronized (_connection.getFailoverMutex())
        {
            validateDestination(destination);
            sendImpl((AMQDestination) destination, message, deliveryMode, priority, timeToLive,
                     mandatory, _immediate);
        }
    }

    public void send(Destination destination, Message message, int deliveryMode,
                     int priority, long timeToLive, boolean mandatory, boolean immediate)
            throws JMSException
    {
        checkPreConditions();
        checkDestination(destination);
        synchronized (_connection.getFailoverMutex())
        {
            validateDestination(destination);
            sendImpl((AMQDestination) destination, message, deliveryMode, priority, timeToLive,
                     mandatory, immediate);
        }
    }

    public void send(Destination destination, Message message, int deliveryMode,
                     int priority, long timeToLive, boolean mandatory,
                     boolean immediate, boolean waitUntilSent)
            throws JMSException
    {
        checkPreConditions();
        checkDestination(destination);
        synchronized (_connection.getFailoverMutex())
        {
            validateDestination(destination);
            sendImpl((AMQDestination) destination, message, deliveryMode, priority, timeToLive,
                     mandatory, immediate, waitUntilSent);
        }
    }


    private AbstractJMSMessage convertToNativeMessage(Message message) throws JMSException
    {
        if (message instanceof AbstractJMSMessage)
        {
            return (AbstractJMSMessage) message;
        }
        else
        {
            AbstractJMSMessage newMessage;

            if (message instanceof BytesMessage)
            {
                BytesMessage bytesMessage = (BytesMessage) message;
                bytesMessage.reset();

                JMSBytesMessage nativeMsg = (JMSBytesMessage) _session.createBytesMessage();


                byte[] buf = new byte[1024];

                int len;

                while ((len = bytesMessage.readBytes(buf)) != -1)
                {
                    nativeMsg.writeBytes(buf, 0, len);
                }

                newMessage = nativeMsg;
            }
            else if (message instanceof MapMessage)
            {
                MapMessage origMessage = (MapMessage) message;
                MapMessage nativeMessage = _session.createMapMessage();

                Enumeration mapNames = origMessage.getMapNames();
                while (mapNames.hasMoreElements())
                {
                    String name = (String) mapNames.nextElement();
                    nativeMessage.setObject(name, origMessage.getObject(name));
                }
                newMessage = (AbstractJMSMessage) nativeMessage;
            }
            else if (message instanceof ObjectMessage)
            {
                ObjectMessage origMessage = (ObjectMessage) message;
                ObjectMessage nativeMessage = _session.createObjectMessage();

                nativeMessage.setObject(origMessage.getObject());

                newMessage = (AbstractJMSMessage) nativeMessage;
            }
            else if (message instanceof TextMessage)
            {
                TextMessage origMessage = (TextMessage) message;
                TextMessage nativeMessage = _session.createTextMessage();

                nativeMessage.setText(origMessage.getText());

                newMessage = (AbstractJMSMessage) nativeMessage;
            }
            else if (message instanceof StreamMessage)
            {
                StreamMessage origMessage = (StreamMessage) message;
                StreamMessage nativeMessage = _session.createStreamMessage();


                try
                {
                    origMessage.reset();
                    while (true)
                    {
                        nativeMessage.writeObject(origMessage.readObject());
                    }
                }
                catch (MessageEOFException e)
                {
                    ;//
                }
                newMessage = (AbstractJMSMessage) nativeMessage;
            }
            else
            {
                newMessage = (AbstractJMSMessage) _session.createMessage();

            }

            Enumeration propertyNames = message.getPropertyNames();
            while (propertyNames.hasMoreElements())
            {
                String propertyName = String.valueOf(propertyNames.nextElement());
                if (!propertyName.startsWith("JMSX_"))
                {
                    Object value = message.getObjectProperty(propertyName);
                    newMessage.setObjectProperty(propertyName, value);
                }
            }

            newMessage.setJMSDeliveryMode(message.getJMSDeliveryMode());


            int priority = message.getJMSPriority();
            if (priority < 0)
            {
                priority = 0;
            }
            else if (priority > 9)
            {
                priority = 9;
            }

            newMessage.setJMSPriority(priority);
            if (message.getJMSReplyTo() != null)
            {
                newMessage.setJMSReplyTo(message.getJMSReplyTo());
            }
            newMessage.setJMSType(message.getJMSType());


            if (newMessage != null)
            {
                return newMessage;
            }
            else
            {
                throw new JMSException("Unable to send message, due to class conversion error: " + message.getClass().getName());
            }
        }
    }


    private void validateDestination(Destination destination) throws JMSException
    {
        if (!(destination instanceof AMQDestination))
        {
            throw new JMSException("Unsupported destination class: " +
                    (destination != null ? destination.getClass() : null));
        }
        declareDestination((AMQDestination) destination);
    }

    protected void sendImpl(AMQDestination destination, Message message, int deliveryMode, int priority,
                            long timeToLive, boolean mandatory, boolean immediate) throws JMSException
    {
        sendImpl(destination, message, deliveryMode, priority, timeToLive, mandatory, immediate, _waitUntilSent);
    }

    /**
     * The caller of this method must hold the failover mutex.
     *
     * @param destination
     * @param origMessage
     * @param deliveryMode
     * @param priority
     * @param timeToLive
     * @param mandatory
     * @param immediate
     * @throws JMSException
     */
    protected void sendImpl(AMQDestination destination, Message origMessage, int deliveryMode, int priority,
                            long timeToLive, boolean mandatory, boolean immediate, boolean wait) throws JMSException
    {
        checkTemporaryDestination(destination);
        origMessage.setJMSDestination(destination);

        
        AbstractJMSMessage message = convertToNativeMessage(origMessage);
        message.getJmsContentHeaderProperties().setBytes(CustomJMSXProperty.JMSX_QPID_JMSDESTINATIONURL.getShortStringName(), destination.toByteEncoding());
        // AMQP version change: Hardwire the version to 0-8 (major=8, minor=0)
        // TODO: Connect this to the session version obtained from ProtocolInitiation for this session.
        // Be aware of possible changes to parameter order as versions change.
        AMQFrame publishFrame = BasicPublishBody.createAMQFrame(_channelId,
            (byte)8, (byte)0,	// AMQP version (major, minor)
            destination.getExchangeName(),	// exchange
            immediate,	// immediate
            mandatory,	// mandatory
            destination.getRoutingKey(),	// routingKey
            0);	// ticket



        message.prepareForSending();
        ByteBuffer payload = message.getData();
        BasicContentHeaderProperties contentHeaderProperties = message.getJmsContentHeaderProperties();

        if (!_disableTimestamps)
        {
            final long currentTime = System.currentTimeMillis();
            contentHeaderProperties.setTimestamp(currentTime);

            if (timeToLive > 0)
            {
                contentHeaderProperties.setExpiration(currentTime + timeToLive);
            }
            else
            {
                contentHeaderProperties.setExpiration(0);
            }
        }
        contentHeaderProperties.setDeliveryMode((byte) deliveryMode);
        contentHeaderProperties.setPriority((byte) priority);

        final int size = (payload != null) ? payload.limit() : 0;
        final int contentBodyFrameCount = calculateContentBodyFrameCount(payload);
        final AMQFrame[] frames = new AMQFrame[2 + contentBodyFrameCount];

        if(payload != null)
        {
            createContentBodies(payload, frames, 2, _channelId);
        }

        if (contentBodyFrameCount != 0 && _logger.isDebugEnabled())
        {
            _logger.debug("Sending content body frames to " + destination);
        }

        // weight argument of zero indicates no child content headers, just bodies
        // AMQP version change: Hardwire the version to 0-8 (major=8, minor=0)
        // TODO: Connect this to the session version obtained from ProtocolInitiation for this session.
        AMQFrame contentHeaderFrame = ContentHeaderBody.createAMQFrame(_channelId, BasicConsumeBody.getClazz((byte)8, (byte)0), 0,
                                                                       contentHeaderProperties,
                                                                       size);
        if (_logger.isDebugEnabled())
        {
            _logger.debug("Sending content header frame to " + destination);
        }

        frames[0] = publishFrame;
        frames[1] = contentHeaderFrame;
        CompositeAMQDataBlock compositeFrame = new CompositeAMQDataBlock(frames);
        _protocolHandler.writeFrame(compositeFrame, wait);


        if (message != origMessage)
        {
            _logger.debug("Updating original message");
            origMessage.setJMSPriority(message.getJMSPriority());
            origMessage.setJMSTimestamp(message.getJMSTimestamp());
            _logger.debug("Setting JMSExpiration:" + message.getJMSExpiration());
            origMessage.setJMSExpiration(message.getJMSExpiration());
            origMessage.setJMSMessageID(message.getJMSMessageID());
        }
    }

    private void checkTemporaryDestination(AMQDestination destination) throws JMSException
    {
        if(destination instanceof TemporaryDestination)
        {
            _logger.debug("destination is temporary destination");
            TemporaryDestination tempDest = (TemporaryDestination) destination;
            if(tempDest.getSession().isClosed())
            {
                _logger.debug("session is closed");
                throw new JMSException("Session for temporary destination has been closed");
            }
            if(tempDest.isDeleted())
            {
                _logger.debug("destination is deleted");
                throw new JMSException("Cannot send to a deleted temporary destination");
            }
        }
    }

    /**
     * Create content bodies. This will split a large message into numerous bodies depending on the negotiated
     * maximum frame size.
     *
     * @param payload
     * @param frames
     * @param offset
     * @param channelId @return the array of content bodies
     */
    private void createContentBodies(ByteBuffer payload, AMQFrame[] frames, int offset, int channelId)
    {

        if (frames.length == offset + 1)
        {
            frames[offset] = ContentBody.createAMQFrame(channelId,new ContentBody(payload));
        }
        else
        {

            final long framePayloadMax = _session.getAMQConnection().getMaximumFrameSize() - 1;
            long remaining = payload.remaining();
            for (int i = offset; i < frames.length; i++)
            {
                payload.position((int) framePayloadMax * (i-offset));
                int length = (remaining >= framePayloadMax) ? (int) framePayloadMax : (int) remaining;
                payload.limit(payload.position() + length);
                frames[i] = ContentBody.createAMQFrame(channelId,new ContentBody(payload.slice()));
                                            
                remaining -= length;
            }
        }

    }

    private int calculateContentBodyFrameCount(ByteBuffer payload)
    {
        // we substract one from the total frame maximum size to account for the end of frame marker in a body frame
        // (0xCE byte).
        int frameCount;
        if(payload == null || payload.remaining() == 0)
        {
            frameCount = 0;
        }
        else
        {
            int dataLength = payload.remaining();
            final long framePayloadMax = _session.getAMQConnection().getMaximumFrameSize() - 1;
            int lastFrame = (dataLength % framePayloadMax) > 0 ? 1 : 0;
            frameCount = (int) (dataLength / framePayloadMax) + lastFrame;
        }
        return frameCount;
    }

    public void setMimeType(String mimeType) throws JMSException
    {
        checkNotClosed();
        _mimeType = mimeType;
    }

    public void setEncoding(String encoding) throws JMSException, UnsupportedEncodingException
    {
        checkNotClosed();
        _encoding = encoding;
    }

    private void checkPreConditions() throws javax.jms.IllegalStateException, JMSException
    {
        checkNotClosed();

        if (_session == null || _session.isClosed())
        {
            throw new javax.jms.IllegalStateException("Invalid Session");
        }
    }

    private void checkInitialDestination()
    {
        if (_destination == null)
        {
            throw new UnsupportedOperationException("Destination is null");
        }
    }

    private void checkDestination(Destination suppliedDestination) throws InvalidDestinationException
    {
        if (_destination != null && suppliedDestination != null)
        {
            throw new UnsupportedOperationException("This message producer was created with a Destination, therefore you cannot use an unidentified Destination");
        }

        if (suppliedDestination == null)
        {
            throw new InvalidDestinationException("Supplied Destination was invalid");
        }


    }


    public AMQSession getSession()
    {
        return _session;
    }
}
