#!/bin/bash

HOST="127.0.0.1"
PORT=2222
PASS="2"

echo "================================================="
echo "   АГРЕССИВНЫЙ ТЕСТ-СЬЮТ ДЛЯ FT_IRC"
echo "================================================="
rm -f client_*.log

# [ТЕСТЫ 1-3 ОСТАЮТСЯ ИЗ ПРЕДЫДУЩЕЙ ВЕРСИИ]
echo -e "\n[ЗАПУСК ТЕСТА 1] Фрагментация (Networking specials)"
(
    echo -ne "PASS $PASS\r\nNICK frag_user\r\nUSER frag 0 * :frag\r\n"
    sleep 1; echo -ne "JOIN #test\r\n"; sleep 1
    echo -n "PRIVMSG #test :This is a "
    sleep 2
    echo -ne "fragmented message!\r\n"
    sleep 1; echo -ne "QUIT\r\n"
) | nc $HOST $PORT > client_frag.log &
sleep 5

echo -e "\n[ЗАПУСК ТЕСТА 2] Права оператора (Channel operator)"
(
    echo -ne "PASS $PASS\r\nNICK admin\r\nUSER admin 0 * :admin\r\n"
    sleep 1; echo -ne "JOIN #ops\r\n"; sleep 2
    echo -ne "MODE #ops +t\r\n"; sleep 2
    echo -ne "TOPIC #ops :Official Admin Topic\r\n"; sleep 1
    echo -ne "KICK #ops norm_user :You have no power here\r\n"; sleep 1
    echo -ne "QUIT\r\n"
) | nc $HOST $PORT > client_admin.log &

(
    echo -ne "PASS $PASS\r\nNICK norm_user\r\nUSER norm 0 * :norm\r\n"
    sleep 2; echo -ne "JOIN #ops\r\n"; sleep 2
    echo -ne "TOPIC #ops :Hacked Topic\r\n"; sleep 4
    echo -ne "QUIT\r\n"
) | nc $HOST $PORT > client_norm.log &
sleep 10

# ---------------------------------------------------------
# НОВЫЕ СТРЕСС-ТЕСТЫ
# ---------------------------------------------------------

echo -e "\n[ЗАПУСК ТЕСТА 4] Попытка обхода авторизации и неверный пароль"
# Цель: Убедиться, что сервер блокирует команды до ввода PASS и NICK.
(
    # Отправляем PRIVMSG без регистрации
    echo -ne "PRIVMSG #test :Sneaky message\r\n"
    sleep 1
    # Отправляем неверный пароль
    echo -ne "PASS WRONG_PASSWORD\r\n"
    sleep 1
    # Пытаемся зарегаться без пароля
    echo -ne "NICK hacker\r\nUSER hacker 0 * :hacker\r\n"
    sleep 1
) | nc $HOST $PORT > client_hack_auth.log &

sleep 4
echo "--- Лог взломщика (Ожидаются ошибки 451 и 464) ---"
cat client_hack_auth.log
echo "-------------------------------------------------"

echo -e "\n[ЗАПУСК ТЕСТА 5] Синтаксический мусор и пустые команды"
# Цель: Проверка парсера на отсутствие Segfault при кривых параметрах.
(
    echo -ne "PASS $PASS\r\nNICK dummy\r\nUSER dummy 0 * :dummy\r\n"
    sleep 1
    echo -ne "JOIN\r\n"                 # Без канала
    echo -ne "JOIN wrongchannel\r\n"      # Канал без #
    echo -ne "PRIVMSG\r\n"                # Без цели и текста
    echo -ne "PRIVMSG #test\r\n"          # Без текста
    echo -ne "KICK #test\r\n"             # Без жертвы
    sleep 1
) | nc $HOST $PORT > client_bad_syntax.log &

sleep 3
echo "--- Лог синтаксиса (Ожидаются ошибки 461, 403, 412) ---"
cat client_bad_syntax.log
echo "-------------------------------------------------"

echo -e "\n[ЗАПУСК ТЕСТА 6] Спам-флуд (Flood Attack)"
# Цель: Проверка устойчивости вызова poll() и буферов при массированной атаке.
(
    echo -ne "PASS $PASS\r\nNICK flooder\r\nUSER f 0 * :f\r\n"
    sleep 1
    # Отправляем 200 команд за долю секунды
    for i in {1..200}; do
        echo -ne "PING :spam$i\r\n"
    done
    sleep 2
) | nc $HOST $PORT > client_flood.log &

sleep 4
echo "--- Флуд завершен. Посчитаем количество ответов PONG (должно быть 200) ---"
grep -c "PONG" client_flood.log
echo "-------------------------------------------------"

echo -e "\n[ЗАПУСК ТЕСТА 7] Переполнение буфера без переноса строки"
# Цель: Имитация зависшего клиента, который шлет данные, но не шлет \r\n. Сервер не должен зависнуть для остальных.
(
    echo -ne "PASS $PASS\r\nNICK buffer_boy\r\nUSER b 0 * :b\r\n"
    sleep 1
    # Генерируем 5000 символов 'A' без \r\n
    python3 -c "print('A' * 5000, end='')"
    sleep 3
    # И только потом завершаем команду
    echo -ne " END\r\n"
    sleep 1
) | nc $HOST $PORT > client_buffer.log &

sleep 6
echo "================================================="
echo "ТЕСТИРОВАНИЕ ЗАВЕРШЕНО. Сервер должен продолжать работать."