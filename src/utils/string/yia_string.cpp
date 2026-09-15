#include "pch.h"
#include "yia_string.h"
#include "../memory/allocator.h"

namespace yialite
{

String::String(const char* str) noexcept
{
    if (!str)
    {
        m_length = 0;
        m_capacity = 15;
        m_data = static_cast<char*>(alloc_raw(m_capacity + 1));
        m_data[0] = '\0';
        return;
    }

    m_length = strlen(str);
    m_capacity = calculate_capacity(m_length);
    m_data = static_cast<char*>(alloc_raw(m_capacity + 1));
    memcpy(m_data, str, m_length);
    m_data[m_length] = '\0';
}

String::String(size_t capacity) noexcept
{
    m_capacity = capacity;
    m_data = static_cast<char*>(alloc_raw(m_capacity + 1));
    m_data[0] = '\0';
}

String::String(const char *str, size_t len) noexcept
{
    if (!str || len == 0)
    {
        m_length = 0;
        m_capacity = 15;
        m_data = static_cast<char*>(alloc_raw(m_capacity + 1));
        m_data[0] = '\0';
        return;
    }

    m_length = len;
    m_capacity = calculate_capacity(m_length);
    m_data = static_cast<char*>(alloc_raw(m_capacity + 1));
    memcpy(m_data, str, m_length);
    m_data[m_length] = '\0';
}

String::String(StringView str) noexcept
    : String(str.data(), str.length())
{
}

String::String(const String &str) noexcept
{
    m_length = str.m_length;
    m_capacity = str.m_capacity;
    m_data = static_cast<char*>(alloc_raw(m_capacity + 1));
    memcpy(m_data, str.m_data, m_length + 1);
}

String::String(String &&str) noexcept
    : m_data(str.m_data), m_length(str.m_length), m_capacity(str.m_capacity)
{
    str.m_data = nullptr;
    str.m_length = 0;
    str.m_capacity = 0;
}

String::~String() noexcept
{
    dealloc_raw(m_data);
}

const char* String::c_str() const noexcept
{
    return m_data ? m_data : "";
}

char *String::data() noexcept
{
    return m_data;
}

const char *String::data() const noexcept
{
    return m_data;
}

size_t String::length() const noexcept
{
    return m_length;
}

size_t String::capacity() const noexcept
{
    return m_capacity;
}

char *String::begin() noexcept
{
    return m_data;
}

char *String::end() noexcept
{
    return m_data + m_length;
}

const char *String::begin() const noexcept
{
    return m_data;
}

const char *String::end() const noexcept
{
    return m_data + m_length;
}

String String::operator+(const String &str) const noexcept
{
    String temp(*this);
    temp += str;
    return temp;
}

String String::operator+(const char *str) const noexcept
{
    String temp(*this);
    temp += str;
    return temp;
}

String &String::operator+=(const char *str) noexcept
{
    if (!str) return *this;

    size_t str_len = strlen(str);
    size_t new_length = m_length + str_len;
    m_capacity = calculate_capacity(new_length);

    m_data = static_cast<char*>(realloc_raw(m_data, m_capacity + 1));
    memcpy(m_data + m_length, str, str_len + 1);
    m_length = new_length;

    return *this;
}

String &String::operator+=(const String &str) noexcept
{
    if(str.m_length == 0) return *this;

    size_t new_length = m_length + str.m_length;
    m_capacity = calculate_capacity(new_length);

    m_data = static_cast<char*>(realloc_raw(m_data, m_capacity + 1));
    memcpy(m_data + m_length, str.m_data, str.m_length + 1);
    m_length = new_length;

    return *this;
}

String& String::operator=(const String &str) noexcept
{
    if (this == &str) return *this;
    
    m_length = str.m_length;
    m_capacity = str.m_capacity;
    m_data = static_cast<char*>(realloc_raw(m_data, m_capacity + 1));
    memcpy(m_data, str.m_data, m_length + 1);

    return *this;
}

String &String::operator=(String &&str) noexcept
{
    if (this != &str)
    {
        dealloc_raw(m_data);

        m_data = str.m_data;
        m_length = str.m_length;
        m_capacity = str.m_capacity;

        str.m_data = nullptr;
        str.m_length = 0;
        str.m_capacity = 0;
    }

    return *this;
}

String& String::operator=(const char *str) noexcept
{   
    if (!str) str = "";
    
    m_length = strlen(str);
    m_capacity = calculate_capacity(m_length);
    m_data = static_cast<char*>(realloc_raw(m_data, m_capacity + 1));
    memcpy(m_data, str, m_length + 1);

    return *this;
}

char &String::operator[](size_t pos) noexcept
{
    return m_data[pos];
}

const char &String::operator[](size_t pos) const noexcept
{
    return m_data[pos];
}

bool String::operator==(const char *str) const noexcept
{
    if (m_length != strlen(str)) return false;
    return memcmp(m_data, str, m_length) == 0;
}

bool String::operator==(const String &str) const noexcept
{
    if (m_length != str.m_length) return false;
    return memcmp(m_data, str.m_data, m_length) == 0;
}

bool String::operator!=(const char *str) const noexcept
{
    return !(*this == str);
}

bool String::operator!=(const String &str) const noexcept
{
    return !(*this == str);
}

void String::append(const char *str) noexcept
{
    *this += str;
}

void String::append(const char *str, size_t count) noexcept
{
    if (!str || count == 0) return;

    size_t str_len = strlen(str);
    if (count > str_len) count = str_len;
    size_t new_length = m_length + count;
    m_capacity = calculate_capacity(new_length);
    
    m_data = static_cast<char*>(realloc_raw(m_data, m_capacity + 1));
    memcpy(m_data + m_length, str, count);
    m_length = new_length;
    m_data[m_length] = '\0';
}

void String::append(const String &str) noexcept
{
    *this += str;
}

void String::append(const String &str, size_t pos, size_t count) noexcept
{
    String substr = str.sub_str(pos, count);
    append(substr.m_data, count);
}

void String::clear() noexcept
{
    m_length = 0;
    m_data[0] = '\0';
}

String &String::erase(size_t pos, size_t count) noexcept
{
    if(pos >= m_length) return *this;
    if(count > m_length - pos) count = m_length - pos;

    memmove(m_data + pos, m_data + pos + count, m_length - pos - count);
    m_length -= count;
    m_data[m_length] = '\0';

    return *this;
}

String String::sub_str(size_t pos, size_t count) const noexcept
{
    if (pos >= m_length) return String();
    if (count > m_length - pos) count = m_length - pos;

    size_t new_capacity = calculate_capacity(count);
    String temp(new_capacity);

    memcpy(temp.m_data, m_data + pos, count);
    temp.m_data[count] = '\0';
    temp.m_length = count;

    return temp;
}

bool String::empty() const noexcept
{
    return m_length == 0;
}

size_t String::find(const char *str, size_t pos) const noexcept
{
    if(!str || pos >= m_length) return npos;

    size_t str_len = strlen(str);
    for(size_t i = pos; i <= m_length - str_len; ++i)
    {
        for(size_t j = 0; j < str_len; ++j)
        {
            if (m_data[i + j] != str[j]) break;
            if (j == str_len - 1) return i;
        }
    }

    return npos;
}

size_t String::find(const String &str, size_t pos) const noexcept
{
    return find(str.m_data, pos);
}

size_t String::find_first_of(const char *str, size_t pos) const noexcept
{
    if(!str || pos >= m_length) return npos;

    for(size_t i = pos; i < m_length; ++i)
    {
        for(size_t j = 0; str[j]; ++j)
        {
            if (m_data[i] == str[j]) return i;
        }
    }

    return npos;
}

size_t String::find_first_of(const String &str, size_t pos) const noexcept
{
    return find_first_of(str.m_data, pos);
}

size_t String::find_first_not_of(const char *str, size_t pos) const noexcept
{
    if(!str || pos >= m_length) return npos;

    size_t str_len_sub_1 = strlen(str) - 1;
    for(size_t i = pos; i < m_length; ++i)
    {
        for(size_t j = 0; str[j]; ++j)
        {
            if (m_data[i] == str[j]) break;
            if(j == str_len_sub_1) return i;
        }
    }

    return npos;
}

size_t String::find_first_not_of(const String &str, size_t pos) const noexcept
{
    return find_first_not_of(str.m_data, pos);
}

size_t String::find_last_of(const char *str, size_t pos) const noexcept
{
    if(!str) return npos;
    if(pos >= m_length) pos = m_length - 1;

    for(size_t i = pos + 1; i-- > 0; )
    {
        for(size_t j = 0; str[j]; ++j)
        {
            if (m_data[i] == str[j]) return i;
        }
    }

    return npos;
}

size_t String::find_last_of(const String &str, size_t pos) const noexcept
{
    return find_last_of(str.m_data, pos);
}

size_t String::find_last_not_of(const char *str, size_t pos) const noexcept
{
    if(!str) return npos;
    if(pos >= m_length) pos = m_length - 1;

    size_t str_len_sub_1 = strlen(str) - 1;
    for(size_t i = pos + 1; i-- > 0; )
    {
        for(size_t j = 0; str[j]; ++j)
        {
            if (m_data[i] == str[j]) break;
            if(j == str_len_sub_1) return i;
        }
    }

    return npos;
}

size_t String::find_last_not_of(const String &str, size_t pos) const noexcept
{
    return find_last_not_of(str.m_data, pos);
}

size_t String::calculate_capacity(size_t length) const
{
    if (length <= 15) return 15;
    return ((length + 16) & ~static_cast<size_t>(15)) - 1;
}

String operator+(const char* str1, const String& str2) noexcept
{
    size_t str_len = strlen(str1);
    size_t new_length = str2.m_length + str_len;
    size_t new_capacity = str2.calculate_capacity(new_length);
    String temp(new_capacity);

    temp.m_length = new_length;
    memcpy(temp.m_data, str1, str_len);
    memcpy(temp.m_data + str_len, str2.m_data, str2.m_length + 1);

    return temp;
}

}
