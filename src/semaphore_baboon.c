// semaphore_baboon.c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>    // Для O_CREAT и других флагов
#include <errno.h>
#include <signal.h>   // Для обработки сигналов

// Структура, представляющая бабуина
typedef struct {
    int id;           // Уникальный идентификатор бабуина
    char direction;   // Направление движения: 'W' (Запад) или 'E' (Восток)
} baboon_t;

// Именованные семафоры
#define SEM_MUTEX_NAME "/sem_mutex_baboon"
#define SEM_COND_WEST_NAME "/sem_cond_west_baboon"
#define SEM_COND_EAST_NAME "/sem_cond_east_baboon"

// Указатели на семафоры
sem_t *sem_mutex = NULL;      // Семафор для взаимного исключения (mutex)
sem_t *sem_cond_west = NULL;  // Семафор для условной переменной "Запад"
sem_t *sem_cond_east = NULL;  // Семафор для условной переменной "Восток"

// Глобальные переменные для текущего направления и количества бабуинов в критической секции
char current_direction = 'N'; // 'N' - нет направления, 'W' - запад, 'E' - восток
int count_baboons = 0;        // Количество бабуинов в критической секции

// Счетчики ожидающих бабуинов для каждого направления
int wait_west = 0;
int wait_east = 0;

// Функция очистки и удаления семафоров
void cleanup_semaphores() {
    if (sem_mutex != NULL) {
        sem_close(sem_mutex);
        sem_unlink(SEM_MUTEX_NAME);
    }
    if (sem_cond_west != NULL) {
        sem_close(sem_cond_west);
        sem_unlink(SEM_COND_WEST_NAME);
    }
    if (sem_cond_east != NULL) {
        sem_close(sem_cond_east);
        sem_unlink(SEM_COND_EAST_NAME);
    }
}

// Обработчик сигналов для корректного завершения программы
void handle_signal() {
    cleanup_semaphores();
    exit(0);
}

// Функция, выполняемая каждым потоком-бабуином
void* baboon_thread(void* arg) {
    baboon_t *baboon = (baboon_t*)arg; // Преобразование аргумента к типу baboon_t*
    char dir = baboon->direction;      // Направление движения бабуина
    int id = baboon->id;               // Идентификатор бабуина

    // Вывод сообщения о попытке войти в критическую секцию
    printf("Бабуин %d пытается зайти в критическую секцию (%s).\n", id, dir == 'W' ? "Запад" : "Восток");

    // Вход в критическую секцию: ожидание и блокировка мьютекса
    sem_wait(sem_mutex);

    // Проверка условий для входа в критическую секцию
    while (current_direction != 'N' && current_direction != dir) {
        if (dir == 'W') {
            wait_west++;                // Увеличиваем количество ожидающих на Запад
            sem_post(sem_mutex);        // Освобождаем мьютекс перед ожиданием
            sem_wait(sem_cond_west);    // Ожидаем сигнала на условной переменной "Запад"
            wait_west--;                // Уменьшаем количество ожидающих после пробуждения
        } else {
            wait_east++;                // Увеличиваем количество ожидающих на Восток
            sem_post(sem_mutex);        // Освобождаем мьютекс перед ожиданием
            sem_wait(sem_cond_east);    // Ожидаем сигнала на условной переменной "Восток"
            wait_east--;                // Уменьшаем количество ожидающих после пробуждения
        }
        // После пробуждения снова блокируем мьютекс для проверки условий
        sem_wait(sem_mutex);
    }

    // Если нет текущего направления, устанавливаем направление текущего бабуина
    if (current_direction == 'N') {
        current_direction = dir;
    }

    // Увеличиваем количество бабуинов в критической секции
    count_baboons++;
    printf("Бабуин %d зашел в критическую секцию (%s). Текущее количество: %d\n", id, dir == 'W' ? "Запад" : "Восток", count_baboons);

    // Разблокируем мьютекс после изменения общих ресурсов
    sem_post(sem_mutex);

    // Симуляция работы в критической секции (задержка 1 секунда)
    sleep(1);

    // Выход из критической секции
    sem_wait(sem_mutex); // Блокируем мьютекс для доступа к общим ресурсам
    count_baboons--;      // Уменьшаем количество бабуинов в критической секции
    printf("Бабуин %d вышел из критической секции (%s). Текущее количество: %d\n", id, dir == 'W' ? "Запад" : "Восток", count_baboons);

    // Если больше нет бабуинов в критической секции, сбрасываем направление и сигнализируем ожидающим потокам противоположного направления
    if (count_baboons == 0) {
        current_direction = 'N'; // Сбрасываем текущее направление

        // Сигнализируем всем ожидающим потокам противоположного направления
        if (dir == 'W') {
            for (int i = 0; i < wait_east; i++) {
                sem_post(sem_cond_east); // Разбудить поток, ожидающий на Восток
            }
        } else {
            for (int i = 0; i < wait_west; i++) {
                sem_post(sem_cond_west); // Разбудить поток, ожидающий на Запад
            }
        }
    }

    // Разблокируем мьютекс после изменения общих ресурсов
    sem_post(sem_mutex);

    free(baboon); // Освобождаем память, выделенную для структуры baboon_t
    return NULL;   // Завершаем поток
}

int main() {
    // Установка обработчика сигналов для корректного завершения
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Создание и инициализация именованных семафоров
    // Параметры sem_open: имя, флаги, права доступа, начальное значение
    sem_mutex = sem_open(SEM_MUTEX_NAME, O_CREAT | O_EXCL, 0644, 1);
    if (sem_mutex == SEM_FAILED) {
        if (errno == EEXIST) {
            // Если семафор уже существует, открыть существующий
            sem_mutex = sem_open(SEM_MUTEX_NAME, 0);
            if (sem_mutex == SEM_FAILED) {
                perror("Не удалось открыть существующий семафор mutex");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("Не удалось создать семафор mutex");
            exit(EXIT_FAILURE);
        }
    }

    sem_cond_west = sem_open(SEM_COND_WEST_NAME, O_CREAT | O_EXCL, 0644, 0);
    if (sem_cond_west == SEM_FAILED) {
        if (errno == EEXIST) {
            sem_cond_west = sem_open(SEM_COND_WEST_NAME, 0);
            if (sem_cond_west == SEM_FAILED) {
                perror("Не удалось открыть существующий семафор cond_west");
                cleanup_semaphores();
                exit(EXIT_FAILURE);
            }
        } else {
            perror("Не удалось создать семафор cond_west");
            cleanup_semaphores();
            exit(EXIT_FAILURE);
        }
    }

    sem_cond_east = sem_open(SEM_COND_EAST_NAME, O_CREAT | O_EXCL, 0644, 0);
    if (sem_cond_east == SEM_FAILED) {
        if (errno == EEXIST) {
            sem_cond_east = sem_open(SEM_COND_EAST_NAME, 0);
            if (sem_cond_east == SEM_FAILED) {
                perror("Не удалось открыть существующий семафор cond_east");
                cleanup_semaphores();
                exit(EXIT_FAILURE);
            }
        } else {
            perror("Не удалось создать семафор cond_east");
            cleanup_semaphores();
            exit(EXIT_FAILURE);
        }
    }

    pthread_t threads[10]; // Массив идентификаторов потоков

    // Создание 10 потоков-бабуинов с разными направлениями
    for (int i = 0; i < 10; i++) {
        baboon_t *baboon = malloc(sizeof(baboon_t)); // Выделение памяти для структуры baboon_t
        if (baboon == NULL) {                        // Проверка успешности выделения памяти
            perror("Не удалось выделить память для бабуина");
            cleanup_semaphores();
            exit(EXIT_FAILURE);
        }
        baboon->id = i + 1;                          // Установка идентификатора бабуина
        baboon->direction = (i % 2 == 0) ? 'W' : 'E';// Установка направления: четные - 'W', нечетные - 'E'

        // Создание потока-бабуина
        if (pthread_create(&threads[i], NULL, baboon_thread, baboon) != 0) {
            perror("Не удалось создать поток");
            free(baboon); // Освобождаем память в случае ошибки
            cleanup_semaphores();
            exit(EXIT_FAILURE);
        }

        usleep(100000); // Задержка 100 миллисекунд для разнообразия порядка входа
    }

    // Ожидание завершения всех потоков
    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
    }

    // Разрушение и удаление именованных семафоров после завершения работы
    cleanup_semaphores();

    return 0; // Завершение программы
}
