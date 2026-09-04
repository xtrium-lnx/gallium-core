#ifndef GALLIUM__CORE__MESSAGEBUS_H
#define GALLIUM__CORE__MESSAGEBUS_H
#pragma once

#include <gallium/macros.h>
#include <gallium/globalinstance.h>
#include <gallium/string_hash.h>

#include <gallium/core/delegate.h>
#include <gallium/core/ctti.h>

namespace ga::core
{
	class MessageDataBase
		: public CttiObject
	{
		GA_CTTI_OBJECT(MessageDataBase, CttiObject);

	public:
		virtual ~MessageDataBase() = default;
	};

	using MessageHandler = DelegateBase<void, MessageDataBase*>;

	class MessageBus
		: public GlobalInstance<MessageBus>
	{
		struct Internal;
		std::unique_ptr<Internal> m_pImpl;

	public:
		explicit MessageBus();
		~MessageBus();

		void Initialize() override;
		void Terminate() override;

		void AddRecipient(string_hash_t id, MessageHandler* handler);
		void RemoveRecipient(string_hash_t id, MessageHandler* handler);

		void Post(string_hash_t id);
		void Post(string_hash_t id, const MessageDataBase& data);
		void PostAsync(string_hash_t id);
		void PostAsync(string_hash_t id, const MessageDataBase& data);
		void PostImmediate(string_hash_t id);
		void PostImmediate(string_hash_t id, const MessageDataBase& data);

		void Poll();
	};
}

#define MSG_LAMBDA(X) new ga::core::LambdaDelegate<void, ga::core::MessageDataBase*>(X)

#define GA_MESSAGEDATA(NAME, MEMBERS) \
	struct NAME##Desc { MEMBERS }; \
    class NAME : public ga::core::MessageDataBase { \
        GA_DEBUG_ONLY( GA_CTTI_OBJECT(NAME, MessageDataBase); ) \
    public: \
		virtual ~NAME() = default; \
		NAME##Desc data; \
		NAME(const NAME##Desc& desc) : data(desc) {} \
		CttiObject* Clone() const override { NAME* result = new NAME(data); return result; } }

#endif /* GALLIUM__CORE__MESSAGEBUS_H */