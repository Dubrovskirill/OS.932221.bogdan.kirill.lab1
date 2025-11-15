#pragma once

#include <cstdint>
#include <string>
#include <pthread.h>

class EventData
{
public:
    EventData();
    EventData(const std::string& name, std::size_t length);
    ~EventData() = default;

    bool Count();
    int GetCounter() const;
    std::string GetString() const;

private:
    std::string m_string;
    std::uint64_t m_counter;
};

class Monitor
{
public:
    Monitor();
    ~Monitor();

    void Init();
    void Destroy();

    void ProviderAction();
    void ConsumerAction();

private:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
    EventData* m_data;
    bool m_ready;
    int m_iterations;  
};