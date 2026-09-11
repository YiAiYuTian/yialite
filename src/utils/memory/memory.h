#ifndef YIALITE_MEMORY_H
#define YIALITE_MEMORY_H

#include "allocator.h"

#include <type_traits>
#include <utility>

namespace yialite
{

template <typename T>
class Scope
{
public:
    Scope() noexcept = default;
    Scope(std::nullptr_t) noexcept : Scope() {}
    explicit Scope(T* ptr) noexcept : m_ptr(ptr) {}
    Scope(Scope&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    ~Scope() { DEALLOCATE_OBJECT(m_ptr); m_ptr = nullptr; }

    //operators
    Scope& operator=(Scope&& other) noexcept
    {
        if (this != &other)
        {
            DEALLOCATE_OBJECT(m_ptr);
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }
    T* operator->() { return m_ptr; }
    const T* operator->() const { return m_ptr; }
    T& operator*() { return *m_ptr; }
    const T& operator*() const { return *m_ptr; }
    operator bool() const { return m_ptr != nullptr; }

    //tools
    [[nodiscard]] T* get() { return m_ptr; }
    [[nodiscard]] const T* get() const { return m_ptr; }
    [[nodiscard]] T* release() { T* ptr = m_ptr; m_ptr = nullptr; return ptr; }
    
    void reset(T* ptr = nullptr) { DEALLOCATE_OBJECT(m_ptr); m_ptr = ptr; }
private:
    T* m_ptr = nullptr;
};

class ControlBlock
{
    FRIEND_ALLOCATOR
public:
    void add_ref() { ++m_ref; }
    void add_weak() { ++m_weak; }

    void release_ref()
    {
        if (--m_ref == 0)
        {
            destroy_object();
            release_weak();
        }
    }
    void release_weak()
    {
        if (--m_weak == 0) { dealloc_obj(this); }
    }

    size_t get_ref_count() const { return m_ref; }
    size_t get_weak_count() const { return m_weak; }
protected:
    virtual void destroy_object() = 0;
    virtual ~ControlBlock() = default;
private:
    size_t m_ref = 1;
    size_t m_weak = 1;
};

template<typename T>
class ControlBlockImpl final : public ControlBlock
{
public:
    explicit ControlBlockImpl(T* obj) : m_obj(obj) {}

    T* get() const { return m_obj; }
private:
    void destroy_object() override
    {
        DEALLOCATE_OBJECT(m_obj);
        m_obj = nullptr;
    }
private:
    T* m_obj = nullptr;
};

template <typename T> class Weak;

template <typename T>
class Ref
{
    friend class Weak<T>;
public:
    Ref() noexcept = default;
    Ref(std::nullptr_t) noexcept : Ref() {}
    explicit Ref(T* ptr) : m_ptr(ptr) 
    { 
        if (m_ptr)
        {
            m_block = alloc_obj<ControlBlockImpl<T>>(m_ptr);
        }
    }
    explicit Ref(const Ref& other)
        : m_ptr(other.m_ptr), m_block(other.m_block)
    {
        if (m_block) m_block->add_ref();
    }
    Ref(T* ptr, ControlBlock* block)
        : m_ptr(ptr), m_block(block)
    {
        if(m_block) m_block->add_ref();
    }
    Ref(Ref&& other) noexcept
        : m_ptr(other.m_ptr), m_block(other.m_block)
    { 
        other.m_ptr = nullptr;
        other.m_block = nullptr;
    }

    ~Ref() { if (m_block) m_block->release_ref(); }

    //operators
    Ref& operator=(Ref&& other) noexcept
    {
        if (this != &other)
        {
            if (m_block) m_block->release_ref();
            m_ptr = other.m_ptr;
            m_block = other.m_block;
            other.m_ptr = nullptr;
            other.m_block = nullptr;
        }
        return *this;
    }
    Ref& operator=(const Ref& other)
    {
        if (this != &other)
        {
            if (m_block) m_block->release_ref();
            m_ptr = other.m_ptr;
            m_block = other.m_block;
            if (m_block) m_block->add_ref();
        }
        return *this;
    }
    T* operator->() { return m_ptr; }
    const T* operator->() const { return m_ptr; }
    T& operator*() { return *m_ptr; }
    const T& operator*() const { return *m_ptr; }
    operator bool() const { return m_ptr != nullptr; }

    //tools
    [[nodiscard]] T* get() { return m_ptr; }
    [[nodiscard]] const T* get() const { return m_ptr; }
    [[nodiscard]] bool compare_owner(const Ref& other) const noexcept { return m_block == other.m_block; }
    [[nodiscard]] size_t get_used_count() const { return m_block ? m_block->get_ref_count() : 0; }

    void reset(T* ptr = nullptr)
    { 
        if (m_block) m_block->release_ref(); 
        if(!ptr)
        {
            m_ptr = nullptr;
            m_block = nullptr;
            return;
        }
        m_ptr = ptr;
        m_block = alloc_obj<ControlBlockImpl<T>>(m_ptr); 
        if (m_block) m_block->add_ref();
    }
private:
    T* m_ptr = nullptr;
    ControlBlock* m_block = nullptr;
};

template <typename T>
class Weak
{
public:
    Weak() noexcept = default;
    Weak(std::nullptr_t) noexcept : Weak() {}
    Weak(const Ref<T>& strong) noexcept : m_block(strong.m_block) { if (m_block) m_block->add_weak(); }
    Weak(const Weak& other) noexcept : m_block(other.m_block) { if (m_block) m_block->add_weak(); }
    Weak(Weak&& other) noexcept : m_block(other.m_block) { other.m_block = nullptr; }
    ~Weak() { if (m_block) m_block->release_weak(); }

    //operators
    Weak& operator=(const Weak& other) noexcept
    {
        if (this != &other)
        {
            if (m_block) m_block->release_weak();
            m_block = other.m_block;
            if (m_block) m_block->add_weak();
        }
        return *this;
    }
    Weak& operator=(Weak&& other) noexcept
    {
        if (this != &other)
        {
            if (m_block) m_block->release_weak();
            m_block = other.m_block;
            other.m_block = nullptr;
        }
        return *this;
    }

    //tools
    [[nodiscard]] Ref<T> lock() const noexcept
    {
        if (expired()) return nullptr;
        return Ref<T>(static_cast<ControlBlockImpl<T>*>(m_block)->get(), m_block);
    }
    [[nodiscard]] bool compare_owner(const Weak& other) const noexcept { return m_block == other.m_block; }
    [[nodiscard]] bool expired() const noexcept { return !m_block || m_block->get_ref_count() == 0; }
    [[nodiscard]] size_t get_weak_count() const { return m_block ? m_block->get_weak_count() : 0; }

    void reset() noexcept
    {
        if (m_block) m_block->release_weak();
        m_block = nullptr;
    }
    void reset(const Ref<T>& strong) noexcept
    {
        if (m_block) m_block->release_weak();
        m_block = strong.m_block;
        if (m_block) m_block->add_weak();
    }

private:
    ControlBlock* m_block = nullptr;
};

template<typename T, typename ...Args>
Scope<T> make_scope(Args&&... args)
{
    return Scope<T>(alloc_obj<T>(std::forward<Args>(args)...));
}

template<typename T, typename ...Args>
Ref<T> make_ref(Args&&... args)
{
    return Ref<T>(alloc_obj<T>(std::forward<Args>(args)...));
}

template<typename T>
Weak<T> make_weak(const Ref<T>& ref)
{
    return Weak<T>(ref);
}

}

#endif
