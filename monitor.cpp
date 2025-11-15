#include "monitor.h"

#include <cstdio>
#include <unistd.h>
#include <stdexcept>
#include <limits>
#include <cstddef>  


EventData::EventData()
    : m_string("Not initialized string, or something")
    , m_counter(0)
{
}

EventData::EventData(const std::string& name, std::size_t length)
{
    if (name.length() > length)
    {
        throw std::logic_error("Name`s length must be equal to length");
    }
    m_string = name;
}

bool EventData::Count()
{
    if (m_counter == std::numeric_limits<decltype(m_counter)>::max())
    {
        m_counter = 0;
        return false;
    }
    m_string = (m_counter % 2 == 0 ? "A" : "B");
    ++m_counter;
    return true;
}

int EventData::GetCounter() const
{
    return static_cast<int>(m_counter);
}

std::string EventData::GetString() const
{
    return m_string;
}

