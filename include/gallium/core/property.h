#ifndef GALLIUM__CORE__PROPERTY_H
#define GALLIUM__CORE__PROPERTY_H
#pragma once

#include <gallium/core/ctti.h>
#include <functional>

namespace ga::core
{
    template<typename T>
    class Property
        : public ga::core::CttiObject
    {
        GA_CTTI_OBJECT(ga::core::Property, ga::core::CttiObject);

        std::function<T&()>           m_getter;
        std::function<void(const T&)> m_setter;
        std::function<void()>         m_changeSignal;
        bool                          m_changedIfNoCallback;
        T                             m_valueIfNoAccessors;

        T& Get()
        {
            if (m_changeSignal)
                m_changeSignal();
            else
                m_changedIfNoCallback = true;

            if (m_getter)
                return m_getter();
            else
                return m_valueIfNoAccessors;
        }

        const T& Get() const
        {
            if (m_getter)
                return m_getter();
            else
                return m_valueIfNoAccessors;
        }

        void Set(const T& value)
        {
            if (m_setter)
                m_setter(value);
            else
                m_valueIfNoAccessors = value;

            if (m_changeSignal)
                m_changeSignal();
            else
                m_changedIfNoCallback = true;
        }

        typename typedef Property<T> PropertyType;

    public:
        Property()
            : m_getter()
            , m_setter()
            , m_valueIfNoAccessors(T())
            , m_changedIfNoCallback(false)
        { }

        Property(const T& t)
            : m_getter()
            , m_setter()
            , m_valueIfNoAccessors(t)
            , m_changedIfNoCallback(false)
        { }

        Property(const std::function<T()>& getter)
            : m_getter(getter)
            , m_setter()
            , m_valueIfNoAccessors(T())
            , m_changedIfNoCallback(false)
        { }

        Property(const std::function<T()>& getter, const std::function<void(const T&)>& setter)
            : m_getter(getter)
            , m_setter(setter)
            , m_valueIfNoAccessors(T())
            , m_changedIfNoCallback(false)
        { }

        Property(const Property& other)
            : m_getter(other.m_getter)
            , m_setter(other.m_setter)
            , m_valueIfNoAccessors(other.m_valueIfNoAccessors)
            , m_changedIfNoCallback(other.m_changedIfNoCallback)
        { }

        Property& operator=(const T& t)
        {
            Set(t);
            return *this;
        }

        operator T& ()
        {
            if (m_changeSignal)
                m_changeSignal();
            else
                m_changedIfNoCallback = true;

            return Get();
        }

        operator const T& () const
        {
            return Get();
        }

        T& operator()()
        {
            if (m_getter)
                return m_getter();

            return m_valueIfNoAccessors;
        }

        const T& operator()() const
        {
            return Get();
        }

        const T& ConstRef() const
        {
            return Get();
        }

        T& NonConstRef_Unguarded()
        {
            return operator()();
        }

        T* operator->()
        {
            return &Get();
        }

        void OnChange(const std::function<void()>& changeSignal)
        {
            m_changeSignal = changeSignal;
        }

        void TriggerChanged()
        {
            if (m_changeSignal)
                m_changeSignal();
        }

        bool HasChanged()
        {
            bool retval = m_changedIfNoCallback;
            m_changedIfNoCallback = false;
            return retval;
        }

        template<typename TI> PropertyType& operator/=(TI value) { Set(Get() / value); return *this; }
        template<typename TI> PropertyType& operator+=(TI value) { Set(Get() + value); return *this; }
        template<typename TI> PropertyType& operator-=(TI value) { Set(Get() - value); return *this; }
        template<typename TI> PropertyType& operator*=(TI value) { Set(Get() * value); return *this; }

        template<typename TI> auto operator/(TI value) const -> decltype(T() / TI()) { return Get() / value; }
        template<typename TI> auto operator+(TI value) const -> decltype(T() + TI()) { return Get() + value; }
        template<typename TI> auto operator-(TI value) const -> decltype(T() - TI()) { return Get() - value; }
        template<typename TI> auto operator*(TI value) const -> decltype(T() * TI()) { return Get() * value; }

        void Serialize(ga::core::CttiSerializer& serializer) const override { serializer << Get();            }
        void Deserialize(ga::core::CttiDeserializer& deserializer) override { T t; deserializer >> t; Set(t); }
    };
}

#endif /* GALLIUM__CORE__PROPERTY_H */
