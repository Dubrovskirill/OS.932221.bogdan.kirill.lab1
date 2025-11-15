# Лабораторная работа 1: Реализация монитора для синхронизации потоков

## Структура проекта

- **Monitor.h**: Заголовочный файл с классами `EventData` и `Monitor`.
- **Monitor.cpp**: Реализация классов.
- **main.cpp**: Точка входа — создание потоков, обработка сигналов (Ctrl+C для graceful shutdown).

Компиляция требует `-lpthread` для линковки pthread.

## Классы и их назначение

### Класс `EventData`

- **Назначение**: Симулирует **несериализуемые данные** (экземпляр класса по указателю). Содержит разделяемые данные, которые передаёт поставщик потребителю без сериализации.
- **Поля** (private):
  - `std::string m_string`: Строка, меняющаяся на "A" (чётный счётчик) или "B" (нечётный).
  - `std::uint64_t m_counter`: Счётчик итераций (с overflow на 0).
- **Методы** (public):
  - Конструкторы: Default (инициализация дефолтной строкой) и с параметрами (name, length — с throw на ошибку длины).
  - `bool Count()`: "Работа" — инкремент счётчика, смена строки; возвращает false на overflow.
  - `int GetCounter() const`: Геттер счётчика (как int).
  - `std::string GetString() const`: Геттер строки.
- **Использование**: Создаётся динамически (`new`), указатель хранится в `Monitor`. Потребитель выводит актуальные данные после обработки.

### Класс `Monitor`

- **Назначение**: Инкапсулирует весь монитор — разделяемые данные, мьютекс, условную переменную и логику потоков. Обеспечивает thread-safety и уведомления.
- **Поля** (private):
  - `pthread_mutex_t m_mutex`: Мьютекс для защиты критической секции (доступ к `m_ready` и `m_data`).
  - `pthread_cond_t m_cond`: Условная переменная для уведомления (signal/broadcast).
  - `EventData* m_data`: Указатель на данные (nullptr после destroy).
  - `bool m_ready`: Флаг готовности события (0 — ждать, 1 — обработать).
  - `std::atomic<bool> m_exit_flag`: Thread-safe флаг для graceful shutdown (избегает race на выходе).
- **Методы** (public):
  - `Monitor()` / `~Monitor()`: Конструктор вызывает `Init()`, деструктор — `Destroy()`.
  - `Init()`: Инициализация mutex/cond, создание `EventData`.
  - `Destroy()`: Освобождение ресурсов (delete data, destroy cond/mutex).
  - `Shutdown()`: Установка флага выхода + `broadcast()` (будит потребителя для выхода).
  - `ProviderAction()`: Логика поставщика (цикл: lock → if ready continue → Count() → ready=true → printf("Provided") → signal → unlock → sleep(1)).
  - `ConsumerAction()`: Логика потребителя (цикл: lock → while(!ready && !exit) wait (с "Awoke") → if exit break → printf("Consumed: ...") → ready=false → unlock).
- **Использование**: Глобальный экземпляр в main, передаётся в потоки как arg.

## Логика работы

### В main.cpp

1. **Настройка сигналов**: `std::signal(SIGINT/SIGTERM, signalHandler)` — ловит Ctrl+C, вызывает `Shutdown()` (set flag + broadcast).
2. **Создание монитора**: `Monitor monitor;` — авто-Init().
3. **Создание потоков**:
   - Сначала consumer (`pthread_create(..., ConsumerThread, &monitor)`), чтобы избежать lost signal (потребитель ждёт первым).
   - Затем provider.
   - Проверка ошибок: Если create fails — shutdown и join.
4. **Ожидание**: `pthread_join` на оба потока (блокирует main до завершения).
5. **Завершение**: Авто-Destroy() в деструкторе.

### В потоках

- **ProviderThread**: Вызывает `monitor->ProviderAction()` — бесконечный цикл до `exit_flag`, генерирует событие каждую секунду.
- **ConsumerThread**: Вызывает `monitor->ConsumerAction()` — ждёт в `wait()`, обрабатывает при signal, сбрасывает ready.
- **Синхронизация**: Всё под mutex; while в consumer против spurious wakeups; if в provider против спама.

## Компиляция и запуск

### Компиляция

```bash
g++ -std=c++17 -Wall -Wextra main.cpp Monitor.cpp -o monitor -lpthread
```

### Запуск

```bash
./monitor
```

- Программа работает бесконечно, выводя чередующиеся сообщения.
- Остановка: Ctrl+C — graceful shutdown с сообщением "[EXIT] Signal caught".

## Примеры логов

### Нормальный запуск (чередование сообщений)

```
Provided
Awoke
Consumed: A 1
Provided
Awoke
Consumed: B 2
Provided
Awoke
Consumed: A 3
Provided
Awoke
Consumed: B 4
Provided
Awoke
Consumed: A 5
Provided
Awoke
Consumed: B 6
Provided
Awoke
Consumed: A 7
Provided
Awoke
Consumed: B 8
Provided
Awoke
Consumed: A 9
Provided
Awoke
Consumed: B 10
Provided
Awoke
Consumed: A 11
Provided
Awoke
Consumed: B 12
Provided
Awoke
Consumed: A 13
Provided
Awoke
Consumed: B 14
```

- **Анализ**: Каждое событие: "Provided" (генерация) → "Awoke" (пробуждение от signal) → "Consumed" (обработка с данными). Задержка ~1с. Данные чередуются ("A"/"B", счётчик растёт).

### Graceful shutdown (Ctrl+C после ~14 итераций)

```
^C
[EXIT] Signal caught (2). Shutting down...
Awoke
[CONSUMER] Stopping...
[PROVIDER] Stopping...
Execution completed successfully.
```

- **Анализ**: Ctrl+C → Shutdown() → broadcast будит consumer ("Awoke" в конце) → проверка флага → выход. Provider выходит на следующей итерации. Нет зависания.
