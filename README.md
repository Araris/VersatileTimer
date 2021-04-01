# VersatileTimer
Универсальный недельный и/или суточный программируемый таймер 

Протестирован на ESP8266 в исполнении ESP-01 и NodeMCU.
Всё управление - через веб-интерфейс.
Точное время получает из Интернета (NTP), временное пропадание WiFi/Internet на работу не влияет.

Возможности устройства:

Произвольное (1-20) количество каналов управления (вкл/выкл), настраивается в веб-интерфейсе.
Для каждого канала настраивается "включен/отключен", GPIO, прямое/инвертированное управление.

Произвольное (1-100) количество заданий(таймеров), настраивается в веб-интерфейсе.
Для каждого задания настраивается канал, действие (вкл/выкл/задание отключено), условие по времени суток и дню, или группе дней недели.
Точность установки условия по времени - секунда.

Три режима работы:
  - только ручное управление - настройки заданий игнорируются, после пропадания питания восстанавливается последнее состояние каналов;
  - по заданиям и вручную - позволяет вручную переключать каналы, но лишь до наступления действия очередного задания, либо пропадания питания. После пропадания питания - см. режим ниже;
  - только по заданиям - после пропадания питания устанавливается состояние, в котором каналы должны находиться согласно настройкам заданий для текущего дня недели и времени суток;

Логин и пароль авторизации настраиваются в веб-интерфейсе (от https я отказался по причине медленной отрисовки веб-интерфейса и отсутствия особой необходимости).

Индикация количества заданий для канала.

Индикация текущего активного задания для канала.

Индикация дублирующих и конфликтующих заданий.

Индикация дублирующих каналов.

Часовой пояс настраивается в веб-интерфейсе.

Загрузка прошивки через веб-интерфейс.

Перезагрузка через веб-интерфейс.

При наличии Multicast DNS (mDNS) доступен как хост vt.local.
