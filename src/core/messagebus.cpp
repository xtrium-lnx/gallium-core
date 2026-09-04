#include <gallium/core/messagebus.h>
#include <gallium/core/task.h>

#include <cassert>

using namespace ga::core;

struct MessageBus::Internal
{
    std::unordered_map<string_hash_t, std::vector<MessageHandler* >> handlers;
    std::mutex msgQueueMutex;

    std::vector<std::tuple<std::function<void(string_hash_t, MessageDataBase*)>, string_hash_t, MessageDataBase*>> deferredMessages;
};

MessageBus::MessageBus()
    : m_pImpl(new Internal)
{
}

MessageBus::~MessageBus()
{
    Terminate();
}

void MessageBus::Initialize()
{
}

void MessageBus::Terminate()
{
    for (auto& a : m_pImpl->handlers)
        for (auto& b : a.second)
            delete b;

    m_pImpl->handlers.clear();
}

void MessageBus::AddRecipient(string_hash_t id, MessageHandler* handler)
{
    assert(handler && "Cannot add a null handler");
    m_pImpl->handlers[id].push_back(handler);
}

void MessageBus::RemoveRecipient(string_hash_t id, MessageHandler* handler)
{
    assert(handler && "Cannot remove a null handler");

    if (m_pImpl->handlers.count(id))
    {
        auto it = std::find(m_pImpl->handlers[id].begin(), m_pImpl->handlers[id].end(), handler);

        if (it != m_pImpl->handlers[id].end())
            m_pImpl->handlers[id].erase(it);
    }
}

void MessageBus::Post(string_hash_t id, const MessageDataBase& data)
{
    auto tuple = std::make_tuple([=](string_hash_t localId, MessageDataBase* localData) {
        PostImmediate(localId, *localData);
    }, id, data.Clone()->As<MessageDataBase>());

    m_pImpl->deferredMessages.push_back(tuple);
}

void MessageBus::Post(string_hash_t id)
{
    auto tuple = std::make_tuple([=](string_hash_t localId, MessageDataBase*) {
        PostImmediate(localId);
    }, id, nullptr);

    m_pImpl->deferredMessages.push_back(tuple);
}

void MessageBus::PostAsync(string_hash_t id, const MessageDataBase& data)
{
    TaskScheduler::Global().EnqueueTask(new ga::core::LambdaDelegate<void>([=]() {
        __debugbreak(); // NOT IMPLEMENTED
        //PostImmediate(id, data);
    }));
}

void MessageBus::PostAsync(string_hash_t id)
{
    TaskScheduler::Global().EnqueueTask(new ga::core::LambdaDelegate<void>([=]() {
        __debugbreak(); // NOT IMPLEMENTED
        //PostImmediate(id);
    }));
}

void MessageBus::PostImmediate(string_hash_t id, const MessageDataBase& data)
{
    //std::unique_lock<std::mutex> lock(m_pImpl->msgQueueMutex);

    if (m_pImpl->handlers.count(id) && m_pImpl->handlers.at(id).size() > 0)
        for (auto& a : m_pImpl->handlers[id])
            a->Execute(const_cast<MessageDataBase*>(&data));
}

void MessageBus::PostImmediate(string_hash_t id)
{
    //std::unique_lock<std::mutex> lock(m_pImpl->msgQueueMutex);

    if (m_pImpl->handlers.count(id) && m_pImpl->handlers.at(id).size() > 0)
        for (auto& a : m_pImpl->handlers[id])
            a->Execute(nullptr);
}

void MessageBus::Poll()
{
    for (auto& a : m_pImpl->deferredMessages)
    {
        std::get<0>(a)(std::get<1>(a), std::get<2>(a));
        delete std::get<2>(a);
    }

    m_pImpl->deferredMessages.clear();
}
