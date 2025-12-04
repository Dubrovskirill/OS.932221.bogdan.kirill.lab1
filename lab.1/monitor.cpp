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


Monitor::Monitor()
    : m_data(nullptr)
    , m_ready(false)
    , m_exit_flag(false)
{
    Init();
}

Monitor::~Monitor()
{
    Destroy();
}

void Monitor::Init()
{
    pthread_mutex_init(&m_mutex, nullptr);
    pthread_cond_init(&m_cond, nullptr);
    m_data = new EventData("Initial Event", 20);  // Пример инициализации с валидными данными
}

void Monitor::Destroy()
{
    delete m_data;
    m_data = nullptr;
    pthread_cond_destroy(&m_cond);
    pthread_mutex_destroy(&m_mutex);
}

void Monitor::Shutdown()
{
    m_exit_flag.store(true);
    pthread_cond_broadcast(&m_cond);  // Будим consumer, если в wait
}

void Monitor::ProviderAction()
{
    while (!m_exit_flag.load())
    {
        pthread_mutex_lock(&m_mutex);
        if (m_ready)
        {
            pthread_mutex_unlock(&m_mutex);
            continue;  // Избегаем спама, если уже готово
        }
        m_data->Count();  // "Работа" — обновление данных
        m_ready = true;
        printf("Provided\n");
        pthread_cond_signal(&m_cond);
        pthread_mutex_unlock(&m_mutex);
        sleep(1);  // Задержка в 1 секунду после сигнала
    }
    printf("[PROVIDER] Stopping...\n");
}

void Monitor::ConsumerAction()
{
    while (!m_exit_flag.load())
    {
        pthread_mutex_lock(&m_mutex);
        while (!m_ready && !m_exit_flag.load())
        {
            pthread_cond_wait(&m_cond, &m_mutex);
            printf("Awoke\n");
        }
        if (m_exit_flag.load())
        {
            pthread_mutex_unlock(&m_mutex);
            break;
        }
        // Обработка события
        printf("Consumed: %s %d\n", m_data->GetString().c_str(), m_data->GetCounter());
        m_ready = false;
        pthread_mutex_unlock(&m_mutex);
    }
    printf("[CONSUMER] Stopping...\n");
}