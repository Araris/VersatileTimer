# VersatileTimer

THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGE, USE OR OTHER LIABILITY WHETHER IN CONTRACT, TORT OR OTHERWISE, ARISING OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER CONCESSIONS OF THE SOFTWARE.

                      (Описание на русском языке - ниже)

Sorry for google-translate...

**Universal multichannel weekly and / or daily programmable timer.**

Tested on ESP8266 in version ESP-01 and NodeMCU.

All management is via the web interface.

The exact time receives from the Internet (NTP), after the first synchronization, the temporary loss of WiFi / Internet does not affect the work.

**Device capabilities:**

Arbitrary (1-12) number of control channels (on / off), configured in the web interface. For each channel, configurable on / off, GPIO, direct / inverse control.

An arbitrary (1-100) number of tasks (timers), configured in the web interface. For each task, a channel, an action (on / off / task disabled), a condition by time of day and day, or a group of days of the week are configured. The accuracy of setting the condition in time is a second.

Channel management is possible by tasks, manually until the next task, only manually;
  - manually until the next task and only manually - after a power failure, the last channel state is restored;
  - by tasks - after a power failure, the state is set in which the channel should be in accordance with the task settings for the current day of the week and time of day;

Device power outage tracking.

Keeping event logs for a specified number of days.
Calculation and output of the total active state time on control channels for a period of dates.

Ability to save / restore all settings to files on the ESP8266, upload / download files via the web interface.

Language selection (English / Russian) in the web interface.

The login and password for authorization (admin and admin by default) are configured in the web interface (I refused https due to the slow rendering of the web interface and the lack of a special need).

Indication of the number of tasks for the channel.

Shows the current active and next tasks for the channel.

Indication of duplicate and conflicting tasks.

Indication of duplicate and conflicting channels.

Configuring a time synchronization server, time zone and daylight saving via the web interface.

Selecting a WiFi access point for connection and the mode of obtaining IP addresses (DHCP/fixed) via the web interface.

When booting in working mode, if it is not possible to connect to WiFi within thirty seconds, the device will reboot into access point mode. In the access point mode, if there are no connections to http://192.168.4.1, after 10 minutes the device will reboot into working mode.

Downloading firmware via web interface.

Reboot to access point mode (SSID: VT_SETUP , PASS: 319319319 , URL: http://192.168.4.1) via the web interface.

Rebooting via the web interface.

With Multicast DNS (mDNS) available as host vt.local.

![WEB-InterfaceE01](https://github.com/Araris/VersatileTimer/blob/main/WEB-InterfaceE01.jpg?raw=true)
![WEB-InterfaceE02](https://github.com/Araris/VersatileTimer/blob/main/WEB-InterfaceE02.jpg?raw=true)
![WEB-InterfaceE03](https://github.com/Araris/VersatileTimer/blob/main/WEB-InterfaceE03.jpg?raw=true)
![WEB-InterfaceE04](https://github.com/Araris/VersatileTimer/blob/main/WEB-InterfaceE04.jpg?raw=true)

Connection diagram for ESP-01 (one channel) :
![AVVersatileTimer_ESP-01](https://github.com/Araris/VersatileTimer/blob/main/AVVersatileTimer_ESP-01.jpg?raw=true)

Connection diagram for NodeMCU (multiple channels) :
![AVVersatileTimer_NodeMCU](https://github.com/Araris/VersatileTimer/blob/main/AVVersatileTimer_NodeMCU.jpg?raw=true)

It is recommended to add RC filters to the control GPIOs to prevent unwanted switching at boot ( https://rabbithole.wwwdotorg.org/2017/03/28/esp8266-gpio.html )
![ESP8266_GPIO_rc](https://github.com/Araris/VersatileTimer/blob/main/ESP8266_GPIO_rc.png?raw=true)

# VersatileTimer

**Универсальный многоканальный недельный и/или суточный программируемый таймер.** 

Протестирован на ESP8266 в исполнении ESP-01 и NodeMCU.

Всё управление - через веб-интерфейс.

Точное время получает из Интернета (NTP), после первой синхронизации временное пропадание WiFi/Internet на работу не влияет.

**Возможности устройства:**

Произвольное (1-12) количество каналов управления (вкл/выкл), настраивается в веб-интерфейсе.
Для каждого канала настраивается "включен/отключен", GPIO, прямое/инвертированное управление.

Произвольное (1-100) количество заданий(таймеров), настраивается в веб-интерфейсе.
Для каждого задания настраивается канал, действие (вкл/выкл/задание отключено), условие по времени суток и дню, или группе дней недели.
Точность установки условия по времени - секунда.

Управление каналами возможно по заданиям, вручную до следующего задания, только вручную;
  - вручную до следующего задания и только вручную - после пропадания питания восстанавливается последнее состояние канала;
  - по заданиям - после пропадания питания устанавливается состояние, в котором канал должен находиться согласно настройкам заданий для текущего дня недели и времени суток;

Отслеживание выключений и пропаданий питания устройства.

Ведение журналов событий за указанное количество дней. 
Подсчет и вывод общего времени активного состояния по каналам управления за период дат. 

Возможность сохранения / восстановления всех настроек в файлы на ESP8266, загрузка / выгрузка файлов через веб-интерфейс.

Выбор языка (английский / русский) в веб-интерфейсе.

Логин и пароль авторизации (admin и admin по умолчанию) настраиваются в веб-интерфейсе (от https я отказался по причине медленной отрисовки веб-интерфейса и отсутствия особой необходимости).

Индикация количества заданий для канала.

Индикация текущего активного и следующего заданий для канала.

Индикация дублирующих и конфликтующих заданий.

Индикация дублирующих и конфликтующих каналов.

Настройка сервера синхронизации времени, часового пояса и автоперехода на летнее-зимнее время через веб-интерфейс.

Выбор точки доступа WiFi для подключения и режима получения IP адресов (DHCP/фиксированные) через веб-интерфейс.

При загрузке в рабочем режиме, в случае невозможности подключиться к WiFi в течение тридцати секунд, устройство перезагружается в режим точки доступа. 
В режиме точки доступа, в случае отсутствия подключений к http://192.168.4.1, через 10 минут устройство перезагружается в рабочий режим.

Загрузка прошивки через веб-интерфейс.

Перезагрузка в режим точки доступа (SSID: VT_SETUP , PASS: 319319319 , URL: http://192.168.4.1) через веб-интерфейс.

Перезагрузка через веб-интерфейс.

При наличии Multicast DNS (mDNS) доступен как хост vt.local.

![WEB-InterfaceR01](https://github.com/Araris/VersatileTimer/blob/main/WEB-InterfaceR01.jpg?raw=true)
![WEB-InterfaceR02](https://github.com/Araris/VersatileTimer/blob/main/WEB-InterfaceR02.jpg?raw=true)
![WEB-InterfaceR03](https://github.com/Araris/VersatileTimer/blob/main/WEB-InterfaceR03.jpg?raw=true)
![WEB-InterfaceR04](https://github.com/Araris/VersatileTimer/blob/main/WEB-InterfaceR04.jpg?raw=true)

Схема подключения для ESP-01 (один канал) :
![AVVersatileTimer_ESP-01](https://github.com/Araris/VersatileTimer/blob/main/AVVersatileTimer_ESP-01.jpg?raw=true)

Схема подключения для NodeMCU (несколько каналов) :
![AVVersatileTimer_NodeMCU](https://github.com/Araris/VersatileTimer/blob/main/AVVersatileTimer_NodeMCU.jpg?raw=true)

Рекомендуется добавить RC-фильтры на управляющие GPIO для предотвращения нежелательных переключений при запуске ( https://rabbithole.wwwdotorg.org/2017/03/28/esp8266-gpio.html )
![ESP8266_GPIO_rc](https://github.com/Araris/VersatileTimer/blob/main/ESP8266_GPIO_rc.png?raw=true)
