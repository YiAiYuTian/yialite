#ifndef YIALITE_DELEGATE_H
#define YIALITE_DELEGATE_H

#include "memory/allocator.h"
#include "base_types.h"

#include <type_traits>
#include <functional>

namespace yialite
{

namespace detail
{
    inline constexpr size_t SBO_SIZE  = 32;
    inline constexpr size_t SBO_ALIGN = alignof(void*);
}

template <typename T>
class Delegate;

template <typename Ret, typename... Args>
class Delegate<Ret(Args...)>
{
public:
    using result_type = Ret;

    Delegate() noexcept = default;
    Delegate(std::nullptr_t) noexcept : Delegate() {}

    // noexcept, although cloning the stored callable is only nothrow if the
    // callable itself is. The promise is deliberate: the engine runs without
    // exceptions, so a throwing callable copy would terminate at the next
    // frame boundary anyway. Declaring it here moves that termination to the
    // point of the copy, which is where the stack still says whose callable it
    // was. It is also what lets List<WrappedHandle> satisfy its move/copy
    // requirements.
    Delegate(const Delegate& other) noexcept
    {
        if (!other.m_callable) return;

        m_callable = other.m_callable->clone(m_sbo);
        set_inline_from_callable();
    }
    Delegate(Delegate&& other) noexcept { steal_from(other); }

    template <typename Fn>
    requires(
        !std::is_same_v<std::decay_t<Fn>, Delegate>             &&
        (std::is_invocable_r_v<Ret, std::decay_t<Fn>, Args...>  ||
        std::is_member_function_pointer_v<std::decay_t<Fn>>)
    )
    Delegate(Fn&& func) noexcept
    {
        using Decayed = std::decay_t<Fn>;

        if constexpr (std::is_member_function_pointer_v<Decayed>) emplace(std::mem_fn(std::forward<Fn>(func)));
        else
        {
            if constexpr (std::is_copy_constructible_v<Decayed>     &&
                sizeof(CallableImpl<Decayed>) <= detail::SBO_SIZE   &&
                alignof(CallableImpl<Decayed>) <= detail::SBO_ALIGN)
            {
                emplace(std::forward<Fn>(func));
            }
            else emplace_heap(std::forward<Fn>(func));
        }
    }
    ~Delegate() noexcept { tidy(); }

    //operators
    Delegate& operator=(const Delegate& other) noexcept
    {
        if (this == &other) return *this;

        Delegate tmp(other);
        tidy();
        steal_from(tmp);
        return *this;
    }

    Delegate& operator=(Delegate&& other) noexcept
    {
        if (this == &other) return *this;

        tidy();
        steal_from(other);
        return *this;
    }

    template <typename Fn>
    requires(
        !std::is_same_v<std::decay_t<Fn>, Delegate>             &&
        (std::is_invocable_r_v<Ret, std::decay_t<Fn>, Args...>  ||
        std::is_member_function_pointer_v<std::decay_t<Fn>>)
    )
    Delegate& operator=(Fn&& func) noexcept
    {
        using Decayed = std::decay_t<Fn>;

        if constexpr (std::is_member_function_pointer_v<Decayed>) emplace(std::mem_fn(std::forward<Fn>(func)));
        else
        {
            if constexpr (std::is_copy_constructible_v<Decayed>     &&
                sizeof(CallableImpl<Decayed>) <= detail::SBO_SIZE   &&
                alignof(CallableImpl<Decayed>) <= detail::SBO_ALIGN)
            {
                emplace(std::forward<Fn>(func));
            }
            else emplace_heap(std::forward<Fn>(func));
        }
        return *this;
    }

    Delegate& operator=(std::nullptr_t) noexcept
    {
        tidy();
        return *this;
    }

    bool operator==(std::nullptr_t) const noexcept { return m_callable == nullptr; }
    explicit operator bool() const noexcept { return m_callable != nullptr; }

    Ret operator()(Args... args) const
    {
        if (!m_callable)
        {
            if constexpr (std::is_void_v<Ret>) return;
            else return Ret{};
        }

        return m_callable->invoke(std::forward<Args>(args)...);
    }

    //tools
    void swap(Delegate& other) noexcept
    {
        if (this == &other) return;

        Delegate tmp;
        tmp.steal_from(*this);
        this->steal_from(other);
        other.steal_from(tmp);
    }

    void reset() noexcept { tidy(); }
    bool empty() const noexcept { return m_callable == nullptr; }
private:
    //Callable
    class ICallable
    {
    public:
        virtual ~ICallable() = default;
        virtual Ret invoke(Args... args) const = 0;
        virtual ICallable* clone(void* dst) = 0;
        virtual void relocate(void* dst) noexcept = 0;
    };

    template <typename Fn>
    class CallableImpl final : public ICallable
    {
    public:
        mutable Fn m_func;

        template <typename U>
        requires(std::is_constructible_v<Fn, U&&>)
        explicit CallableImpl(U&& f) noexcept(std::is_nothrow_constructible_v<Fn, U&&>) : m_func(std::forward<U>(f)) {}

        Ret invoke(Args... args) const override
        {
            return m_func(std::forward<Args>(args)...);
        }

        ICallable* clone(void* dst) override
        {
            if constexpr (std::is_copy_constructible_v<Fn>)
            {
                Fn copy = m_func;
                if constexpr (sizeof(CallableImpl) <= detail::SBO_SIZE && alignof(CallableImpl) <= detail::SBO_ALIGN)
                {
                    return new (dst) CallableImpl(std::move(copy));
                }
                else return alloc_obj<CallableImpl>(std::move(copy));
            }
            else return nullptr;
        }

        void relocate(void* dst) noexcept override
        {
            new (dst) CallableImpl(std::move(m_func));
            this->~CallableImpl();
        }
    };
private:
    void tidy() noexcept
    {
        if (!m_callable) return;

        if (m_inline) m_callable->~ICallable();
        else dealloc_obj(m_callable);

        m_callable = nullptr;
        m_inline = false;
    }

    bool is_sbo(const ICallable* p) const noexcept
    {
        return static_cast<const void*>(p) == static_cast<const void*>(m_sbo);
    }

    void set_inline_from_callable() noexcept
    {
        m_inline = is_sbo(m_callable);
    }

    void steal_from(Delegate& other) noexcept
    {
        if (!other.m_callable) return;

        if (other.m_inline)
        {
            other.m_callable->relocate(m_sbo);
            m_callable = std::launder(reinterpret_cast<ICallable*>(m_sbo));
            m_inline = true;
        }
        else
        {
            m_callable = other.m_callable;
            m_inline = false;
        }

        other.m_callable = nullptr;
        other.m_inline = false;
    }

    template <typename Fn>
    void emplace(Fn&& func)
    {
        tidy();
        using Impl = std::decay_t<Fn>;
        static_assert(sizeof(CallableImpl<Impl>) <= detail::SBO_SIZE, "Delegate: callable too large for SBO");
        static_assert(alignof(CallableImpl<Impl>) <= detail::SBO_ALIGN, "Delegate: callable alignment too large for SBO");
        ICallable* p = new (m_sbo) CallableImpl<Impl>(std::forward<Fn>(func));
        m_callable = p;
        m_inline = true;
    }

    template <typename Fn>
    void emplace_heap(Fn&& func)
    {
        tidy();
        using Impl = std::decay_t<Fn>;
        ICallable* p = alloc_obj<CallableImpl<Impl>>(std::forward<Fn>(func));
        m_callable = p;
        m_inline = false;
    }
private:
    alignas(detail::SBO_ALIGN) Uint8 m_sbo[detail::SBO_SIZE]{};
    ICallable* m_callable = nullptr;
    bool       m_inline   = false;
};

template <typename Ret, typename... Args>
void swap(Delegate<Ret(Args...)>& a, Delegate<Ret(Args...)>& b) noexcept
{
    a.swap(b);
}

} // namespace yialite

#endif // YIALITE_DELEGATE_H
