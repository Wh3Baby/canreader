#!/bin/bash
# Скрипт проверки статуса сборки Qt6

MXE_DIR="$HOME/mxe"
LOG_FILE="/tmp/qt6_build.log"

echo "Проверка статуса сборки Qt6 через MXE..."
echo ""

if [ ! -d "$MXE_DIR" ]; then
    echo "ОШИБКА: MXE не найден в $MXE_DIR"
    exit 1
fi

# Проверка процесса сборки
if pgrep -f "make qt6" > /dev/null; then
    echo "✓ Сборка Qt6 в процессе..."
    echo ""
    echo "Последние строки лога:"
    tail -10 "$LOG_FILE" 2>/dev/null || echo "Лог еще не создан"
else
    echo "Сборка не запущена или завершена"
    echo ""
    
    # Проверка результата
    if [ -d "$MXE_DIR/usr/x86_64-w64-mingw32.static/qt6" ]; then
        echo "✓ Qt6 успешно собран!"
        echo "Путь: $MXE_DIR/usr/x86_64-w64-mingw32.static/qt6"
        echo ""
        echo "Для использования установите:"
        echo "export PATH=$MXE_DIR/usr/bin:\$PATH"
        echo "export Qt6_DIR=$MXE_DIR/usr/x86_64-w64-mingw32.static/qt6/lib/cmake/Qt6"
    elif [ -d "$MXE_DIR/usr/x86_64-w64-mingw32.shared/qt6" ]; then
        echo "✓ Qt6 успешно собран (shared)!"
        echo "Путь: $MXE_DIR/usr/x86_64-w64-mingw32.shared/qt6"
        echo ""
        echo "Для использования установите:"
        echo "export PATH=$MXE_DIR/usr/bin:\$PATH"
        echo "export Qt6_DIR=$MXE_DIR/usr/x86_64-w64-mingw32.shared/qt6/lib/cmake/Qt6"
    else
        echo "Qt6 еще не собран или произошла ошибка"
        if [ -f "$LOG_FILE" ]; then
            echo ""
            echo "Последние строки лога:"
            tail -20 "$LOG_FILE"
        fi
    fi
fi

echo ""
echo "Для просмотра полного лога: tail -f $LOG_FILE"

