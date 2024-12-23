#include "../Precompiled.h"

#include <Urho3D/WebSocket/WebSocketClient.h>
#include <Urho3D/IO/VectorBuffer.h>
#include <Urho3D/IO/Log.h>

#include <rtc/websocket.hpp>

namespace Urho3D
{
WebSocketClient::WebSocketClient(Context* context)
    : Object(context)
{
    ws_ = ea::make_unique<WebSocket>();
    ws_->onOpen([=]() { onOpen_(); });
    ws_->onClosed([=]() { onClosed_(); });
    ws_->onError([=](std::string error) { onError_(error.c_str()); });
    ws_->onMessage(
        [=](binary msg) 
        { 
            VectorBuffer buffer(msg.data(), msg.size());
            onMessageBinary_(buffer); 
        },
        [=](std::string msg) { onMessageString_(msg.c_str()); }
    );
    ws_->onBufferedAmountLow([=]() { URHO3D_LOGWARNING("OnWSBufferedAmountLow");});
}

WebSocketClient::~WebSocketClient()
{
}

void WebSocketClient::Open(const ea::string& url)
{
    ws_->open(url.c_str());
}

void WebSocketClient::Close()
{
    ws_->close();
}

bool WebSocketClient::Send(const VectorBuffer& msg)
{
    return ws_->send(reinterpret_cast<const std::byte*>(msg.GetData()), msg.GetSize());
}

bool WebSocketClient::IsOpen() const
{
    return ws_->isOpen();
}

bool WebSocketClient::IsClosed() const
{
    return ws_->isClosed();
}

} // namespace Urho3D
