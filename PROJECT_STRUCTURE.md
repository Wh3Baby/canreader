# Структура проекта CAN Reader

```
canreader/
├── CMakeLists.txt          # Конфигурация сборки CMake
├── .gitignore              # Исключения для Git
├── PROJECT_STRUCTURE.md    # Этот файл
│
├── src/                    # Исходные файлы (.cpp)
│   ├── main.cpp           # Точка входа приложения
│   ├── mainwindow.cpp     # Реализация главного окна
│   └── caninterface.cpp   # Реализация CAN интерфейса
│
├── include/                # Заголовочные файлы (.h)
│   ├── mainwindow.h       # Заголовок главного окна
│   └── caninterface.h     # Заголовок CAN интерфейса
│
└── docs/                   # Документация
    └── README.md          # Основная документация проекта
```

## Описание компонентов

### src/main.cpp
Точка входа приложения. Создает QApplication и главное окно.

### src/mainwindow.cpp / include/mainwindow.h
Главное окно приложения с GUI элементами:
- Окно логирования
- Кнопки подключения и отправки
- Выпадающие списки для выбора порта и скорости
- Поля ввода CAN ID и данных

### src/caninterface.cpp / include/caninterface.h
Класс для работы с CAN адаптером Scanmatic 2 Pro:
- Подключение через последовательный порт
- Отправка CAN сообщений
- Прием CAN сообщений
- Парсинг протокола адаптера

### CMakeLists.txt
Конфигурация сборки для Windows и Linux с использованием Qt6.

