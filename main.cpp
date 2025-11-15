#include "monitor.h"

#include <iostream>
#include <pthread.h>
#include <csignal>
#include <atomic>

// Глобальный указатель на monitor для signal_handler
Monitor* g_monitor = nullptr;

// Обработчик сигналов (SIGINT для Ctrl+C, SIGTERM)
void signalHandler(int signal)
{
    std::cout << "\n[EXIT] Signal caught (" << signal << "). Shutting down...\n";
    if (g_monitor)
    {
        g_monitor->Shutdown();
    }
}

void* ProviderThread(void* arg)
{
    Monitor* monitor = static_cast<Monitor*>(arg);
    monitor->ProviderAction();
    return nullptr;
}

void* ConsumerThread(void* arg)
{
    Monitor* monitor = static_cast<Monitor*>(arg);
    monitor->ConsumerAction();
    return nullptr;
}

int main()
{
    // Настройка сигналов
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    Monitor monitor;  // Автоматически Init() в ctor
    g_monitor = &monitor;  // Для handler

    pthread_t provider_tid, consumer_tid;
    int ret;

    // Создание потоков: CONSUMER ПЕРВЫМ, чтобы избежать lost signal
    ret = pthread_create(&consumer_tid, nullptr, ConsumerThread, &monitor);
    if (ret != 0) {
        std::cerr << "[ERROR] Failed to create consumer thread: " << ret << std::endl;
        return 1;
    }

    ret = pthread_create(&provider_tid, nullptr, ProviderThread, &monitor);
    if (ret != 0) {
        std::cerr << "[ERROR] Failed to create provider thread: " << ret << std::endl;
        g_monitor->Shutdown();  // Прерываем consumer
        pthread_join(consumer_tid, nullptr);
        return 1;
    }

    // Ожидание завершения потоков
    pthread_join(provider_tid, nullptr);
    pthread_join(consumer_tid, nullptr);

    // Автоматический Destroy() в dtor
    g_monitor = nullptr;
    std::cout << "Execution completed successfully.\n";
    return 0;
}