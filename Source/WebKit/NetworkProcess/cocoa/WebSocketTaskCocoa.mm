/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebSocketTaskCocoa.h"

#if HAVE(NSURLSESSION_WEBSOCKET)

#import "DataReference.h"
#import "NetworkSocketChannel.h"
#import <Foundation/NSURLSession.h>
#import <WebCore/WebSocketChannel.h>
#import <wtf/BlockPtr.h>

namespace WebKit {

WebSocketTask::WebSocketTask(NetworkSocketChannel& channel, RetainPtr<NSURLSessionWebSocketTask>&& task)
    : m_channel(channel)
    , m_task(WTFMove(task))
{
    readNextMessage();
}

WebSocketTask::~WebSocketTask()
{
}

void WebSocketTask::readNextMessage()
{
    [m_task receiveMessageWithCompletionHandler: makeBlockPtr([this, weakThis = makeWeakPtr(this)](NSURLSessionWebSocketMessage* _Nullable message, NSError * _Nullable error) {
        if (!weakThis)
            return;

        if (error) {
            // FIXME: the error code is probably not a correct WebSocket code.
            didClose([error code], [error localizedDescription]);
            return;
        }
        if (!message) {
            // FIXME: this is a workaround and we should probably never get there.
            didClose(1000, "Unknown error");
            return;
        }
        if (message.type == NSURLSessionWebSocketMessageTypeString)
            m_channel.didReceiveText(message.string);
        else
            m_channel.didReceiveBinaryData(static_cast<const uint8_t*>(message.data.bytes), message.data.length);

        readNextMessage();
    }).get()];
}

void WebSocketTask::cancel()
{
    [m_task cancel];
}

void WebSocketTask::resume()
{
    [m_task resume];
}

void WebSocketTask::didConnect()
{
    m_channel.didConnect();
}

void WebSocketTask::didClose(unsigned short code, const String& reason)
{
    if (m_receivedDidClose)
        return;

    m_receivedDidClose = true;
    m_channel.didClose(code, reason);
}

void WebSocketTask::sendString(const String& text , CompletionHandler<void()>&& callback)
{
    auto message = adoptNS([[NSURLSessionWebSocketMessage alloc] initWithString: text]);
    [m_task sendMessage: message.get() completionHandler: makeBlockPtr([callback = WTFMove(callback)](NSError * _Nullable) mutable {
        callback();
    }).get()];
}

void WebSocketTask::sendData(const IPC::DataReference& data, CompletionHandler<void()>&& callback)
{
    auto nsData = adoptNS([[NSData alloc] initWithBytes:data.data() length:data.size()]);
    auto message = adoptNS([[NSURLSessionWebSocketMessage alloc] initWithData: nsData.get()]);
    [m_task sendMessage: message.get() completionHandler: makeBlockPtr([callback = WTFMove(callback)](NSError * _Nullable) mutable {
        callback();
    }).get()];
}

void WebSocketTask::close(int32_t code, const String& reason)
{
    // FIXME: Should NSURLSession provide a way to call cancelWithCloseCode without any specific code.
    if (code == WebCore::WebSocketChannel::CloseEventCodeNotSpecified)
        code = 1005;
    auto nsData = adoptNS([[NSData alloc] initWithBytes:reason.utf8().data() length:reason.sizeInBytes()]);
    [m_task cancelWithCloseCode: (NSURLSessionWebSocketCloseCode)code reason: nsData.get()];
}

WebSocketTask::TaskIdentifier WebSocketTask::identifier() const
{
    return [m_task taskIdentifier];
}

}

#endif
