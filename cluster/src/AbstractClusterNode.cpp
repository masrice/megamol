/*
 * AbstractClusterNode.cpp
 *
 * Copyright (C) 2006 - 2008 by Universitaet Stuttgart (VIS). 
 * Alle Rechte vorbehalten.
 */

#include "vislib/AbstractClusterNode.h"

#include "vislib/clustermessages.h"
#include "vislib/MissingImplementationException.h"
#include "vislib/RawStorage.h"
#include "the/trace.h"
#include "the/types.h"

#include "messagereceiver.h"


/*
 * vislib::net::cluster::AbstractClusterNode::DEFAULT_PORT
 */
const SHORT vislib::net::cluster::AbstractClusterNode::DEFAULT_PORT = 28182;


/*
 * vislib::net::cluster::AbstractClusterNode::~AbstractClusterNode
 */
vislib::net::cluster::AbstractClusterNode::~AbstractClusterNode(void) {
}


/*
 * vislib::net::cluster::AbstractClusterNode::AbstractClusterNode
 */
vislib::net::cluster::AbstractClusterNode::AbstractClusterNode(void) {
    // Nothing to do.
}


/*
 * vislib::net::cluster::AbstractClusterNode::AbstractClusterNode
 */
vislib::net::cluster::AbstractClusterNode::AbstractClusterNode(
        const AbstractClusterNode& rhs) {
    // Nothing to do.
}


/*
 * vislib::net::cluster::AbstractClusterNode::onMessageReceived
 */
//void vislib::net::cluster::AbstractClusterNode::onMessageReceived(
//        const Socket& src, const unsigned int msgId, const uint8_t *body, 
//        const size_t cntBody) {
//    // Do nothing. Implementing subclasses override this method if they are
//    // interested in messages.
//}


/*
 * vislib::net::cluster::AbstractClusterNode::onCommunicationError
 */
void vislib::net::cluster::AbstractClusterNode::onCommunicationError(
        const PeerIdentifier& peerId, const ComErrorSource src,
        const SocketException& err) throw() {
    THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "A communication error occurred while talking "
        "to %s: %s\n", peerId.ToStringA().PeekBuffer(), err.GetMsgA());
}


/*
 * vislib::net::cluster::AbstractClusterNode::onMessageReceiverExiting
 */
void vislib::net::cluster::AbstractClusterNode::onMessageReceiverExiting(
        vislib::net::Socket& socket, PReceiveMessagesCtx rmc) {
    THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "AbstractClusterNode::onMessageReceiverExiting "
        "releasing receive context ...\n");
    FreeRecvMsgCtx(rmc);
}


/*
 * vislib::net::cluster::AbstractClusterNode::onPeerConnected
 */
void vislib::net::cluster::AbstractClusterNode::onPeerConnected(
        const PeerIdentifier& peerId) throw() {
    // Nothing to do.
}


/*
 * vislib::net::cluster::AbstractClusterNode::sendMessage
 */
size_t vislib::net::cluster::AbstractClusterNode::sendMessage(
        const uint32_t msgId, const uint8_t *data, const uint32_t cntData) {
    RawStorage msg(sizeof(MessageHeader) + cntData);

    InitialiseMessageHeader(*msg.As<MessageHeader>(), msgId, cntData);
    ::memcpy(msg.At(sizeof(MessageHeader)), data, cntData);

    return this->sendToEachPeer(msg.As<uint8_t>(), msg.GetSize());
}


/*
 * vislib::net::cluster::AbstractClusterNode::sendMessage
 */
bool vislib::net::cluster::AbstractClusterNode::sendMessage(
        const PeerIdentifier& peerId, const uint32_t msgId, const uint8_t *data, 
        const uint32_t cntData) {
    RawStorage msg(sizeof(MessageHeader) + cntData);

    InitialiseMessageHeader(*msg.As<MessageHeader>(), msgId, cntData);
    ::memcpy(msg.At(sizeof(MessageHeader)), data, cntData);

    return this->sendToPeer(peerId, msg.As<uint8_t>(), msg.GetSize());
}


/*
 * vislib::net::cluster::AbstractClusterNode::sendToEachPeer
 */
size_t vislib::net::cluster::AbstractClusterNode::sendToEachPeer(
        const uint8_t *data, const size_t cntData) {
    SendToPeerCtx context;
    context.Data = data;
    context.CntData = cntData;

    return this->forEachPeer(sendToPeerFunc, &context);
}


/*
 * vislib::net::cluster::AbstractClusterNode::sendToPeer
 */
bool vislib::net::cluster::AbstractClusterNode::sendToPeer(
        const PeerIdentifier& peerId, const uint8_t *data, const size_t cntData) {
    SendToPeerCtx context;
    context.Data = data;
    context.CntData = cntData;

    return this->forPeer(peerId, sendToPeerFunc, &context);
}


/*
 * vislib::net::cluster::AbstractClusterNode::operator =
 */
vislib::net::cluster::AbstractClusterNode& 
vislib::net::cluster::AbstractClusterNode::operator =(
        const AbstractClusterNode& rhs) {
    return *this;
}


/*
 * vislib::net::cluster::AbstractClusterNode::sendToPeerFunc
 */
bool vislib::net::cluster::AbstractClusterNode::sendToPeerFunc(
        AbstractClusterNode *thisPtr, const PeerIdentifier& peerId,
        Socket& peerSocket, void *context) {
    SendToPeerCtx *ctx = static_cast<SendToPeerCtx *>(context);

    try {
        peerSocket.Send(ctx->Data, ctx->CntData, Socket::TIMEOUT_INFINITE, 0,
            true);
    } catch (SocketException e) {
        thisPtr->onCommunicationError(peerId, SEND_COMMUNICATION_ERROR, e);
        throw e;
    }

    return true;
}
