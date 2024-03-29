// Tested on Arduino IDE 1.8.16
// Tested on NodeMCU 1.0 (ESP-12E Module) and on Generic ESP8266 Module

//#define DEBUG                                      1

#define DEBUGRINGCOUNTER                       false
#define VERSION                            "22.1218"

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <EEPROMRingCounter.h>
#include <LittleFS.h>   // https://github.com/esp8266/Arduino/tree/master/libraries/LittleFS
#include <coredecls.h>  // source of settimeofday_cb();
//
#include "Secrets.h"
// Create a Secrets.h file with Wi-Fi connection settings as follows:
//   #define AP_SSID   "yourSSID"
//   #define AP_PASS   "yourPASSWORD"
//
#define MDNSHOST                                "VT" // mDNS host (+ ".local")
#define APMODE_SSID                       "VT_SETUP" // AP SSID in AP mode
#define APMODE_PASSWORD                  "319319319" // AP password in AP mode
#define DEFAULT_LOGIN_NAME                   "admin"
#define DEFAULT_LOGIN_PASSWORD               "admin"
/////////////////////////////////////////////////////// EEPROM map BEGIN
#define EEPROMSIZE                              4096
#define FIRST_RUN_SIGNATURE_EEPROM_ADDRESS         0 // byte FIRST_RUN_SIGNATURE
#define ACCESS_POINT_SIGNATURE_EEPROM_ADDRESS      1 // byte ACCESS_POINT_SIGNATURE
                                               //  2 // not used
#define CHANNELLISTCOLLAPSED_EEPROM_ADDRESS        3 // boolean ChannelListCollapsed
#define LANGUAGE_EEPROM_ADDRESS                    4 // boolean Language
#define LOG_DAYSTOKEEP_EEPROM_ADDRESS              5 // byte log_DaysToKeep
#define LOG_VIEWSTEP_EEPROM_ADDRESS                6 // byte log_ViewStep
#define TASKLISTCOLLAPSED_EEPROM_ADDRESS           7 // boolean TaskListCollapsed
#define COUNTER_OF_REBOOTS_EEPROM_ADDRESS          8 // int counterOfReboots
#define NUMBER_OF_TASKS_EEPROM_ADDRESS            10 // byte numberOfTasks
#define NTP_TIME_ZONE_EEPROM_ADDRESS              11 // byte 1...24 (int ntpTimeZone + 12)
#define NUMBER_OF_CHANNELS_EEPROM_ADDRESS         12 // byte numberOfChannels
#define NTP_DAYLIGHTSAVEZONE_EEPROM_ADDRESS       13 // byte ntpDaylightSaveZone
#define NTP_DAYLIGHT_SAVING_ENABLED_ADDRESS       14 // boolean ntpAutoDaylightSavingEnabled
#define WIFI_MANUALLY_SET_EEPROM_ADDRESS          15 // boolean wifiManuallySetAddresses  0-use DHCP, 1-fixed addresses
#define LOGIN_NAME_EEPROM_ADDRESS                 16 // 16..26 - String loginName (max 10 chars)
#define LOGIN_PASS_EEPROM_ADDRESS                 27 // 27..39 - String loginPass (max 12 chars)
#define CHANNELLIST_EEPROM_ADDRESS                40 // 40..88 - byte ChannelList from CHANNELLIST_EEPROM_ADDRESS to CHANNELLIST_EEPROM_ADDRESS + CHANNELLIST_MAX_NUMBER * CHANNEL_NUM_ELEMENTS
                                          //      89 // not used
#define BUILD_HOUR_EEPROM_ADDRESS                 90 // byte FW_H
#define BUILD_MIN_EEPROM_ADDRESS                  91 // byte FW_M
#define BUILD_SEC_EEPROM_ADDRESS                  92 // byte FW_S
                                          //  93..99 // not used
#define TASKLIST_EEPROM_ADDRESS                  100 // 100..700 - byte TaskList from TASKLIST_EEPROM_ADDRESS to TASKLIST_EEPROM_ADDRESS + numberOfTasks * (TASK_NUM_ELEMENTS - 1)
#define AP_SSID_EEPROM_ADDRESS                   701 // 701..764 - String AP_name (max 63 chars)
#define AP_PASS_EEPROM_ADDRESS                   765 // 765..797 - String AP_pass (max 32 chars)
#define NTP_TIME_SERVER1_EEPROM_ADDRESS          798 // 798..830 - String ntpServerName1 (max 32 chars)
#define WIFI_MANUALLY_SET_IP_EEPROM_ADDRESS      831 // 831..834 - IPAddress wifiManuallySetIP
#define WIFI_MANUALLY_SET_DNS_EEPROM_ADDRESS     835 // 835..838 - IPAddress wifiManuallySetDNS
#define WIFI_MANUALLY_SET_GW_EEPROM_ADDRESS      839 // 839..842 - IPAddress wifiManuallySetGW
#define WIFI_MANUALLY_SET_SUBNET_EEPROM_ADDRESS  843 // 843..846 - IPAddress wifiManuallySetSUBNET
#define NTP_TIME_SERVER2_EEPROM_ADDRESS          847 // 847..880 - String ntpServerName2 (max 32 chars)
#define MAX_EEPROM_ADDRESS                       900 // total amount EEPROM used
#define RINGCOUNTER_FIRST_ADDRESS               1024
#define RINGCOUNTER_NUM_CELLS                     60 // i.e. last address = 1024 + 60 * 4 = 1264
#define RINGCOUNTER_SAVE_PERIOD              60000UL // let each cell be overwritten once per hour (8760 rewrites per year)
/////////////////////////////////////////////////////// EEPROM map END
#define WIFICONNECTIONTRYPERIOD             600000UL // msec
#define NTP_SERVER_NAME        "europe.pool.ntp.org" // default value
#define NTP_DEFAULT_TIME_ZONE                      2 // default value
#define NTP_POLLING_INTERVAL               3600000UL
#define NTP_MINIMUM_EPOCHTIME           1669000000UL
#define FIRST_RUN_SIGNATURE                      139 // signature to detect first run on the device and prepare EEPROM (max 255)
#define ACCESS_POINT_SIGNATURE                   138 // signature to set AccessPointMode after start
#define FW_H_0                   (__TIME__[0] - '0')
#define FW_H_1                   (__TIME__[1] - '0')
#define FW_M_0                   (__TIME__[3] - '0')
#define FW_M_1                   (__TIME__[4] - '0')
#define FW_S_0                   (__TIME__[6] - '0')
#define FW_S_1                   (__TIME__[7] - '0')
#define FW_H                  (FW_H_0 * 10 + FW_H_1)
#define FW_M                  (FW_M_0 * 10 + FW_M_1)
#define FW_S                  (FW_S_0 * 10 + FW_S_1)
#define SECS_PER_MIN                            60UL
#define SECS_PER_HOUR                         3600UL
#define SECS_PER_DAY                         86400UL
#define LEAP_YEAR(Y)                         (((1970+(Y))>0) && !((1970+(Y))%4) && (((1970+(Y))%100) || !((1970+(Y))%400)))
#define RSSI_THERESHOLD                          -80 // (dB)
#define ACCESS_POINT_MODE_TIMER_INTERVAL    600000UL // (msec)
#define GPIO_MAX_NUMBER                           16
#define CHANNELLIST_MIN_NUMBER                     1
#define CHANNELLIST_MAX_NUMBER                    12
#define CHANNEL_NUM_ELEMENTS                       4
#define CHANNEL_GPIO                               0
#define CHANNEL_INVERTED                           1
#define CHANNEL_LASTSTATE                          2 // LASTSTATE_OFF_BY_TASK, LASTSTATE_ON_BY_TASK,
                                                     // LASTSTATE_OFF_MANUALLY, LASTSTATE_ON_MANUALLY,
                                                     //  50..149 - OFF_UNTIL_NEXT_TASK  (task = CHANNEL_LASTSTATE - 50)
                                                     // 150..250 - ON_UNTIL_NEXT_TASK   (task = CHANNEL_LASTSTATE - 150)
#define CHANNEL_ENABLED                            3
#define LASTSTATE_OFF_BY_TASK                      0
#define LASTSTATE_ON_BY_TASK                       1
#define LASTSTATE_OFF_MANUALLY                     2
#define LASTSTATE_ON_MANUALLY                      3
#define TASKLIST_MIN_NUMBER                        1
#define TASKLIST_MAX_NUMBER                      100
#define TASK_NUM_ELEMENTS                          7
#define TASK_ACTION                                0
#define TASK_HOUR                                  1
#define TASK_MIN                                   2
#define TASK_SEC                                   3
#define TASK_DAY                                   4
#define TASK_CHANNEL                               5
#define TASK_NUMBER                                6 // Not written to EEPROM !
#define TASK_DAY_WORKDAYS                          7
#define TASK_DAY_WEEKENDS                          8
#define TASK_DAY_EVERYDAY                          9
#define ACTION_NOACTION                            0
#define ACTION_TURN_OFF                           10
#define ACTION_TURN_ON                            11 
#define LOG_DIR                               "/log"
#define LOG_BUFFER_SIZE                           20
#define LOG_VIEWSTEP_DEF                          20
#define LOG_VIEWSTEP_MIN                           5
#define LOG_VIEWSTEP_MAX                          50
#define LOG_POLLING                              255
//
#define LOG_EVENT_START                            0
#define LOG_EVENT_TASK_SWITCHING_MANUALLY          1
#define LOG_EVENT_TASK_SWITCHING_BY_TASK           2
#define LOG_EVENT_CHANNEL_MANUALLY                 3
#define LOG_EVENT_CHANNEL_UNTIL_NEXT_TASK          4
#define LOG_EVENT_CHANNEL_BY_TASK                  5
#define LOG_EVENT_DAYLIGHT_SAVING_ON               6
#define LOG_EVENT_DAYLIGHT_SAVING_OFF              7
#define LOG_EVENT_RESTART_TO_AP_MODE               8
#define LOG_EVENT_TASK_SETTINGS_CHANGED            9
#define LOG_EVENT_CHANNEL_SETTINGS_CHANGED        10
#define LOG_EVENT_NUMBER_OF_TASKS_CHANGED         11
#define LOG_EVENT_NUMBER_OF_CHANNELS_CHANGED      12
#define LOG_EVENT_INTERFACE_LANGUAGE_CHANGED      13
#define LOG_EVENT_TIME_ZONE_CHANGED               14
#define LOG_EVENT_AUTO_DAYLIGHT_SAVING_CHANGED    15
#define LOG_EVENT_DAYLIGHT_SAVING_ZONE_CHANGED    16
#define LOG_EVENT_TIME_SERVER_CHANGED             17
#define LOG_EVENT_LOGIN_NAME_CHANGED              18
#define LOG_EVENT_LOGIN_PASSWORD_CHANGED          19
#define LOG_EVENT_WIFI_SETTINGS_CHANGED           20
#define LOG_EVENT_FILE_SAVE                       21
#define LOG_EVENT_FILE_RESTORE                    22
#define LOG_EVENT_FILE_RENAME                     23
#define LOG_EVENT_FILE_DELETE                     24
#define LOG_EVENT_FILE_DOWNLOAD                   25
#define LOG_EVENT_FILE_UPLOAD                     26
#define LOG_EVENT_FIRMWARE_UPDATED                27
#define LOG_EVENT_CLEAR_TASKLIST                  28
#define LOG_EVENT_MANUAL_RESTART                  29
#define LOG_EVENT_LOG_ENTRIES_PER_PAGE_CHANGED    30
#define LOG_EVENT_LOG_DAYS_TO_KEEP_CHANGED        31
#define LOG_EVENT_NTP_TIME_SYNC_ERROR             32
#define LOG_EVENT_NTP_TIME_SYNC_SUCCESS           33
#define LOG_EVENT_WIFI_CONNECTION_ERROR           34
#define LOG_EVENT_WIFI_CONNECTION_SUCCESS         35
#define LOG_EVENT_RESTART_REASON_0                36
#define LOG_EVENT_RESTART_REASON_1                37
#define LOG_EVENT_RESTART_REASON_2                38
#define LOG_EVENT_RESTART_REASON_3                39
#define LOG_EVENT_RESTART_REASON_4                40
#define LOG_EVENT_RESTART_REASON_5                41
#define LOG_EVENT_RESTART_REASON_6                42
#define LOG_EVENT_RESTART_REASON_UNKNOWN          43
#define LOG_EVENT_RTCMEM_TIME_SYNC_SUCCESS        44
#define LOG_EVENT_RTCMEM_READ_ERROR               45
#define LOG_EVENT_RTCMEM_WRITE_ERROR              46
#define LOG_EVENT_NEWDAYLOG_CREATED               47
#define LOG_EVENT_POWER_OFF                       48
#define LOG_EVENT_DAYLOG_FILE_DOWNLOAD            49
//
#define NAMESOFEVENTSMAXNUMBER                    50
const String namesOfEvents[][NAMESOFEVENTSMAXNUMBER] = 
  {{"Start"
   ,"Manual <b>switching</b>","<b>Switching</b> by task"
   ,"manually","until next task","by tasks"
   ,"Daylight saving ON","Daylight saving OFF"
   ,"Doing restart to access point mode"
   ,"Task settings changed","Channel settings changed"
   ,"Number of tasks changed","Number of channels changed"
   ,"Interface language changed"
   ,"Time Zone changed","Auto daylight saving time changed"
   ,"Daylight saving time zone changed","Time synchronization server changed"
   ,"Login name changed","Login password changed"
   ,"Wifi settings changed"
   ,"Settings saved to file","Settings restored from file","Settings file renamed"
   ,"Settings file deleted","Settings file downloaded","Settings file uploaded"
   ,"Firmware updated","Tasklist cleared","Doing manual restart"
   ,"Log entries per page changed","Number of days to keep the log changed"
   ,"Time synchronization <font color='red'>error</font> (NTP)"
   ,"Time synchronization successful (NTP)"
   ,"<font color='red'>No</font> Wi-Fi connection"
   ,"Connected to Wi-Fi"
   ,"Normal startup by power on"
   ,"Restart after hardware watch dog reset"
   ,"Restart after exception reset"
   ,"Restart after software watch dog reset"
   ,"Restart after software restart"
   ,"Wake up from deep-sleep"
   ,"Power on or external system reset"
   ,"Restart (reason unknown)"   
   ,"Time synchronization successful (RTCMEM)"
   ,"RTCMEM read <font color='red'>error</font>"
   ,"RTCMEM write <font color='red'>error</font>"
   ,"Start of daylog"
   ,"<font color='red'>Power supply has been turned off</font>"
   ,"Daylog file downloaded"
   },
   {"Старт"
   ,"<b>Переключение</b> вручную","<b>Переключение</b> по заданию"
   ,"вручную","до след. задания","по заданиям"
   ,"Летнее время активно","Зимнее время активно"
   ,"Перезагрузка в режим точки доступа"
   ,"Настройки задания изменены","Настройки канала изменены"
   ,"Количество заданий изменено","Количество каналов изменено"
   ,"Язык интерфейса изменен"
   ,"Часовой пояс изменен","Автопереход на летнее-зимнее время изменен"
   ,"Зона перехода на летнее-зимнее время изменена","Сервер синхронизации времени изменен"
   ,"Имя авторизации изменено","Пароль авторизации изменен"
   ,"Настройки Wi-Fi изменены"
   ,"Настройки сохранены в файл","Настройки восстановлены из файла","Файл настроек переименован"
   ,"Файл настроек удален","Файл настроек выгружен","Файл настроек загружен"
   ,"Прошивка обновлена","Список заданий очищен","Ручная перезагрузка"
   ,"Количество записей журнала на странице изменено","Количество дней хранения журнала изменено"
   ,"<font color='red'>Ошибка</font> синхронизации времени (NTP)"
   ,"Синхронизация времени успешна (NTP)"
   ,"Подключение к Wi-Fi <font color='red'>отсутствует</font>"
   ,"Подключено к Wi-Fi"
   ,"Начало работы (подача питания)"
   ,"Перезагрузка аппаратным сторожевым таймером"
   ,"Перезагрузка после сброса по исключению"
   ,"Перезагрузка программным сторожевым таймером"
   ,"Программная перезагрузка"
   ,"Пробуждение из режима глубокого сна"
   ,"Включение питания, или внешний сброс системы"
   ,"Перезагрузка (причина неизвестна)"   
   ,"Синхронизация времени успешна (RTCMEM)"
   ,"RTCMEM <font color='red'>ошибка</font> чтения"
   ,"RTCMEM <font color='red'>ошибка</font> записи"
   ,"Начало журнала"
   ,"<font color='red'>Подача питания была отключена</font>"
   ,"Файл журнала выгружен"
   }};
//                    GPIO      0     1    2    3    4    5     6     7     8     9    10    11   12   13   14   15   16
const String NodeMCUpins[] = {"D3","D10","D4","D9","D2","D1","N/A","N/A","N/A","D11","D12","N/A","D6","D7","D5","D8","D0"};
const String namesOfDays[][10] = {{"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Workdays","Weekends","Every day"},
                                  {"Воскресенье","Понедельник","Вторник","Среда","Четверг","Пятница","Суббота","Рабочие дни","Выходные дни","Каждый день"}};
const uint8_t monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
//
String loginName;
String loginPass;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
EEPROMRingCounter RingCounter(RINGCOUNTER_FIRST_ADDRESS, RINGCOUNTER_NUM_CELLS, DEBUGRINGCOUNTER);
unsigned long RingCounterSaveTimer = 0;
//
struct Log_Data
 {
 time_t   utc; 
 uint8_t  event;  // Event number, see #define LOG_EVENT_*** and namesOfEvents
 uint8_t  ch;     // Channel number (CHANNELLIST_MIN_NUMBER...CHANNELLIST_MAX_NUMBER), 0 if none
 uint8_t  task;   // Task number (TASKLIST_MIN_NUMBER...TASKLIST_MAX_NUMBER), 0 if none
 uint8_t  act;    // Action (0 / 1)
 };
boolean wifiManuallySetAddresses = false;
IPAddress wifiManuallySetIP;
IPAddress wifiManuallySetDNS;
IPAddress wifiManuallySetGW;
IPAddress wifiManuallySetSUBNET;
String AP_name; // default AP_SSID
String AP_pass; // default AP_PASS
boolean AccessPointMode = false;
unsigned long AccessPointModeTimer = 0;
size_t littleFStotalBytes = 0;
size_t littleFSblockSize = 0;
boolean littleFS_OK = false;
int numCFGfiles = 0;
int numLOGfiles = 0;
byte log_DaysToKeep;
byte log_ViewStep;
int log_NumRecords = 0;
Log_Data log_delayed_write_buffer[LOG_BUFFER_SIZE];
time_t log_ViewDate;
String log_CurDate;
String log_MinDate;
String log_MaxDate;
int log_StartRecord;
int log_Day = 0;
int log_Month = 0;
int log_Year = 0;
File log_FileHandle;
File uploadFileHandle;
char log_curPath[32];
String logstat_BegDate = "";
String logstat_EndDate = "";
//
byte numberOfTasks; // TASKLIST_MIN_NUMBER...TASKLIST_MAX_NUMBER
bool thereAreEnabledTasks = false;
uint8_t** TaskList;
// uint8_t TaskList[numberOfTasks][TASK_NUM_ELEMENTS]
                                // 0 - 0 no action, 10 off, 11 on - action 
                                // 1 - 0...23 - hour
                                // 2 - 0...59 - minute
                                // 3 - 0...59 - second
                                // 4 - 0...9  - 0 - Sunday, 1...6 - Monday...Saturday, 7 - workdays, 8 - weekends, 9 - every day
                                // 5 - CHANNELLIST_MIN_NUMBER...CHANNELLIST_MAX_NUMBER - channel
                                // 6 - TASK_NUMBER !! Not written to EEPROM !!
byte numberOfChannels; // CHANNELLIST_MIN_NUMBER...CHANNELLIST_MAX_NUMBER
uint8_t** ChannelList;
// uint8_t ChannelList[numberOfChannels][CHANNEL_NUM_ELEMENTS]
                                // 0 - 0...GPIO_MAX_NUMBER - GPIO
                                // 1 - 0...1 - channel controls noninverted/inverted
                                // 2 - 0...1 - channel last saved state
                                // 3 - 0...1 - channel enabled
struct LastChannelsStates
 {
 uint8_t  firstpart;   // each of bits corresponds to the status of channel 0...7
 uint8_t  lastpart;    // each of bits corresponds to the status of channel 8...15
 };
byte ActiveNowTasksList[CHANNELLIST_MAX_NUMBER];  
byte NextTasksList[CHANNELLIST_MAX_NUMBER];
byte NumEnabledTasks[CHANNELLIST_MAX_NUMBER];                         
int counterOfReboots = 0; 
String ntpServerName1 = "";
String ntpServerName2 = "";
byte ntpDaylightSaveZone = 0; // 0 - EU, 1 - USA
boolean ntpAutoDaylightSavingEnabled = true;
boolean curIsDaylightSave = true;
boolean previousIsDaylightSave = false;
int ntpTimeZone = NTP_DEFAULT_TIME_ZONE;  // -11...12
struct { uint32_t RTCMEMcrc32; time_t RTCMEMEpochTime; bool RTCMEMcurIsDaylightSave; } RTCMEMData;
boolean Language = false;       // false - English, true - Russian
boolean TaskListCollapsed = false;
boolean ChannelListCollapsed = false;
time_t ntpcurEpochTime = 0;
time_t lastPowerOff = 0;
struct tm *ntpTimeInfo;
bool ntpTimeIsSynkedFirst = true;
bool ntpTimeIsSynked = false;
bool rtcmemTimeIsSynced = false;
bool isSoftRestart = false;
bool ntpLastPollingSuccess = false;
unsigned long ntpLastSynkedTimer = 0;
bool statusWiFi = false;
bool previousstatusWiFi = false;
unsigned long everyMinuteTimer = 0;
unsigned long everyHalfSecondTimer = 0;
                               
/////////////////////////////////////////////////////////

uint32_t sntp_update_delay_MS_rfc_not_less_than_15000() { return NTP_POLLING_INTERVAL; }

time_t DateOfDateTime(time_t dt) { return (dt / SECS_PER_DAY * SECS_PER_DAY); } // remove the time part

void saveGMTtimeToRingCounter()
{
if ( !ntpTimeIsSynked ) { return; }
time_t GMT0epochtime;
time(&GMT0epochtime);
if ( GMT0epochtime > NTP_MINIMUM_EPOCHTIME
  && GMT0epochtime > lastPowerOff - (ntpTimeZone * 3600) - (curIsDaylightSave * 3600) )
 {
 RingCounter.set(GMT0epochtime);
 lastPowerOff = GMT0epochtime + (ntpTimeZone * 3600) + (curIsDaylightSave * 3600); // back to GMT0
 }
}

boolean log_getdaystat(time_t date, long *daystatarray, bool clearArray = true) // long daystatarray[numberOfChannels]
{
time_t prevUTC[numberOfChannels];
time_t curUTC[numberOfChannels];
boolean prevState[numberOfChannels];
boolean curState[numberOfChannels];
if ( clearArray )
 { for ( int chNum = 0; chNum < numberOfChannels; chNum++ ) { daystatarray[chNum] = 0; } }
for ( int chNum = 0; chNum < numberOfChannels; chNum++ ) 
 { prevUTC[chNum] = 0; prevState[chNum] = 0; curUTC[chNum] = 0; curState[chNum] = 0; }
if ( ntpcurEpochTime < NTP_MINIMUM_EPOCHTIME ) { return false; } 
date = DateOfDateTime(date);
time_t today = DateOfDateTime(ntpcurEpochTime);
struct Log_Data rl;
char path[32];
log_pathFromDate(path, date);
if ( !LittleFS.exists(path) ) { return false; }
File fileHandle = LittleFS.open(path, "r");
#ifdef DEBUG
 Serial.println();
 Serial.println(F("---BEGIN read stat."));
#endif
if ( !fileHandle ) { fileHandle.close(); return false; }
// read first daylog record
if ( log_readRow(&rl, 0, fileHandle) )
 {
 if ( rl.event == LOG_EVENT_NEWDAYLOG_CREATED )
  {
  #ifdef DEBUG
   Serial.print(F("1 NEWDAYLOG_CREATED "));
  #endif 
  for ( byte i = 0; i < numberOfChannels; i++ )
   {
   prevUTC[i] = rl.utc;
   curUTC[i] = rl.utc;
   if ( i < 8 ) { prevState[i] = bitRead(rl.ch, i); } else { prevState[i] = bitRead(rl.task, i - 8); }
   #ifdef DEBUG
    Serial.print(F("CH"));
    Serial.print(i + 1);
    Serial.print(F("="));
    Serial.print(prevState[i]);
    Serial.print(F(" "));
   #endif 
   }
  #ifdef DEBUG
   Serial.println();
  #endif 
  }
 else
  {
  #ifdef DEBUG
   Serial.println(F("! First record is not LOG_EVENT_NEWDAYLOG_CREATED !"));
  #endif
  fileHandle.close();
  return false;
  }
 }
else
 {
 #ifdef DEBUG
  Serial.println(F("! log_readRow returns false !"));
 #endif
 fileHandle.close();
 return false;
 }
int numrec = fileHandle.size() / sizeof(Log_Data); 
int currec = 1;
// read next daylog records
while ( log_readRow(&rl, currec, fileHandle) )
 { 
 if ( rl.event == LOG_EVENT_POWER_OFF
   || rl.event == LOG_EVENT_START
   || rl.event == LOG_EVENT_TASK_SWITCHING_MANUALLY 
   || rl.event == LOG_EVENT_TASK_SWITCHING_BY_TASK )
  {
  if ( rl.event == LOG_EVENT_POWER_OFF )
   {
   #ifdef DEBUG
    Serial.print(currec + 1);
    Serial.print(F(" POWER_OFF "));
   #endif 
   for ( byte i = 0; i < numberOfChannels; i++ )
    {
    curState[i] = false;
    if ( curState[i] != prevState[i] ) { curUTC[i] = rl.utc; }
    #ifdef DEBUG
     Serial.print(F("CH"));
     Serial.print(i + 1);
     Serial.print(F("="));
     Serial.print(curState[i]);
     Serial.print(F("("));
     Serial.print(prevState[i]);
     Serial.print(F(") "));
    #endif 
    }    
   #ifdef DEBUG
    Serial.println();
   #endif 
   }
  else if ( rl.event == LOG_EVENT_START )
   {
   #ifdef DEBUG
    Serial.print(currec + 1);
    Serial.print(F(" START "));
   #endif 
   for ( byte i = 0; i < numberOfChannels; i++ )
    {
    if ( i < 8 ) { curState[i] = bitRead(rl.ch, i); } else { curState[i] = bitRead(rl.task, i - 8); }
    if ( curState[i] != prevState[i] ) { curUTC[i] = rl.utc; }
    #ifdef DEBUG
     Serial.print(F("CH"));
     Serial.print(i + 1);
     Serial.print(F("="));
     Serial.print(curState[i]);
     Serial.print(F(" "));
    #endif 
    }    
   #ifdef DEBUG
    Serial.println();
   #endif 
   }
  else if ( rl.event == LOG_EVENT_TASK_SWITCHING_MANUALLY 
         || rl.event == LOG_EVENT_TASK_SWITCHING_BY_TASK )
   {
   curState[rl.ch] = rl.act;
   if ( curState[rl.ch] != prevState[rl.ch] ) { curUTC[rl.ch] = rl.utc; }
   #ifdef DEBUG
    Serial.print(currec + 1);
    Serial.print(F(" SWITCHING "));
    Serial.print(F("CH"));
    Serial.print(rl.ch + 1);
    Serial.print(F("="));
    Serial.print(curState[rl.ch]);
    Serial.print(F("("));
    Serial.print(prevState[rl.ch]);
    Serial.println(F(") "));
   #endif
   }
  for ( byte i = 0; i < numberOfChannels; i++ )
   { 
   if ( prevState[i] && !curState[i] )
    {
    daystatarray[i] += curUTC[i] - prevUTC[i];
    #ifdef DEBUG
     Serial.print(F("CH"));
     Serial.print(i + 1);
     Serial.print(F(" added "));
     Serial.println(convertMStoDHMS((curUTC[i] - prevUTC[i]) * 1000UL));
    #endif
    }
   prevState[i] = curState[i]; 
   prevUTC[i] = curUTC[i];
   }
  }
 currec++;
 if ( currec == numrec ) { break; }
 yield();
 }
fileHandle.close();
yield();
#ifdef DEBUG
 Serial.println(F("---stat before last record"));
 for ( byte i = 0; i < numberOfChannels; i++ ) 
  {
  Serial.print(F("CH"));
  Serial.print(i + 1);
  Serial.print(F(" "));
  Serial.print(convertMStoDHMS(daystatarray[i] * 1000UL));
  Serial.println();
  } 
 Serial.println(F("---adding period after last record"));
#endif
// period of time after last daylog record
for ( byte i = 0; i < numberOfChannels; i++ ) 
 {
 if ( curState[i] )
  {
  if ( date == today ) // period of time from the last entry to the current time
   { 
   daystatarray[i] += ntpcurEpochTime - curUTC[i]; 
   #ifdef DEBUG
    Serial.print(F("CH"));
    Serial.print(i + 1);
    Serial.print(F(" added1 "));
    Serial.println(convertMStoDHMS((ntpcurEpochTime - curUTC[i]) * 1000UL));
   #endif
   }
  else                 // period of time period from the last entry to the end of the day
   { 
   daystatarray[i] += date + SECS_PER_DAY - curUTC[i]; 
   #ifdef DEBUG
    Serial.print(F("CH"));
    Serial.print(i + 1);
    Serial.print(F(" added2 "));
    Serial.println(convertMStoDHMS((date + SECS_PER_DAY - curUTC[i]) * 1000UL));
   #endif
   }
  } 
 }
#ifdef DEBUG
 Serial.println(F("---stat after last record"));
 for ( byte i = 0; i < numberOfChannels; i++ ) 
  {
  Serial.print(F("CH"));
  Serial.print(i + 1);
  Serial.print(F(" "));
  Serial.print(convertMStoDHMS(daystatarray[i] * 1000UL));
  Serial.println();
  } 
 Serial.println(F("---END read stat"));
#endif
return true;  
}

boolean log_getperiodstat(time_t begdate, time_t enddate, long *daystatarray)
{
boolean ret = false;  
#ifdef DEBUG
 Serial.print(F("log_getperiodstat() begdate = "));
 Serial.print(begdate);
 Serial.print(F(" enddate = "));
 Serial.println(enddate);
#endif
for ( int chNum = 0; chNum < numberOfChannels; chNum++ ) { daystatarray[chNum] = 0; }
for ( time_t date = begdate; date <= enddate; date += SECS_PER_DAY )
 { if ( log_getdaystat(date, daystatarray, false) ) { ret = true; } }
return ret;
}

String convertMStoDHMS(unsigned long millisec)
{
String DHMS = "";
long secs = millisec / 1000;
long mins = secs / 60;
long hours = mins / 60;
long days = hours / 24;
secs = secs - ( mins * 60 );
mins = mins - ( hours * 60 );
hours = hours - ( days * 24 );
if ( days > 0 ) 
 {
 DHMS += (days/10 > 0 ? String(days/10) : "&nbsp;") + String(days%10);
 DHMS += (Language ? F("д ") : F("d "));
 }
if ( hours > 0 ) 
 {
 DHMS += (hours/10 > 0 ? String(hours/10) : "&nbsp;") + String(hours%10);
 DHMS += (Language ? F("ч ") : F("h "));
 if ( days > 0 ) 
  {
  DHMS += (F(" ("));
  DHMS += String(hours + ( days * 24 ));
  DHMS += (Language ? F("ч) ") : F("h) "));
  }
 }
if ( mins > 0 ) 
 {
 DHMS += (mins/10 > 0 ? String(mins/10) : "&nbsp;") + String(mins%10);
 DHMS += (Language ? F("м ") : F("m "));
 }
if ( secs > 0 ) 
 {
 DHMS += (secs/10 > 0 ? String(secs/10) : "&nbsp;") + String(secs%10);
 DHMS += (Language ? F("с ") : F("s "));
 }
return DHMS;
}

LastChannelsStates read_lastchannelsstates()
{
struct LastChannelsStates ret;
ret.firstpart = 0; // each of bits corresponds to the status of channel 0...7
ret.lastpart = 0;  // each of bits corresponds to the status of channel 8...15
#ifdef DEBUG 
 Serial.print(F("Reading last channels states : "));
#endif
for ( int chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 if ( ChannelList[chNum][CHANNEL_ENABLED] )
  {
       if (  ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_BY_TASK 
         ||  ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_MANUALLY 
         || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 50 && ChannelList[chNum][CHANNEL_LASTSTATE] < 150) )
   { 
   if ( chNum < 8 ) { bitWrite(ret.firstpart, chNum, 0); }
               else { bitWrite(ret.lastpart, chNum - 8, 0); }
   }
  else if (  ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK 
         ||  ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_MANUALLY 
         || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 150 && ChannelList[chNum][CHANNEL_LASTSTATE] < 250) )
   {
   if ( chNum < 8 ) { bitWrite(ret.firstpart, chNum, 1); }
               else { bitWrite(ret.lastpart, chNum - 8, 1); }
   }
  } 
 }
#ifdef DEBUG 
 Serial.print(ret.firstpart, BIN);
 Serial.print(F(" "));
 Serial.println(ret.lastpart, BIN);
#endif
return ret; 
}

void log_Append(uint8_t ev, uint8_t ch = 0, uint8_t ts = 0, uint8_t act = 0 )
{
if ( !littleFS_OK ) { return; }
if ( ntpcurEpochTime < NTP_MINIMUM_EPOCHTIME )
 {
 if ( ev != LOG_POLLING ) 
  {
  for ( int i = 0; i < LOG_BUFFER_SIZE; i++ ) 
   {
   if ( log_delayed_write_buffer[i].event == LOG_POLLING )
    {
    log_delayed_write_buffer[i] = { millis(), ev, ch, ts, act }; 
    break;
    }
   }
  } 
 return;
 }
check_DaylightSave();
checkNewDayLogCreate();
struct Log_Data logdata;
if ( log_delayed_write_buffer[0].event != LOG_POLLING )
 {
 log_process();
 File f = LittleFS.open(log_curPath, "a");
 if ( f ) 
  {
  for ( int i = 0; i < LOG_BUFFER_SIZE; i++ ) 
   {
   yield(); delay(1); yield();  
   if ( log_delayed_write_buffer[i].event != LOG_POLLING )
    {
    if ( log_delayed_write_buffer[i].event == LOG_EVENT_START ) 
     { 
     // check lastPowerOff before write LOG_EVENT_START
     if ( lastPowerOff )
      {
      // convert GMT0 epoch time to local epoch time 
      lastPowerOff = lastPowerOff + (ntpTimeZone * 3600) + (curIsDaylightSave * 3600);
      time_t todayDate = DateOfDateTime(ntpcurEpochTime);
      time_t lastPowerOffDate = DateOfDateTime(lastPowerOff);
      if ( lastPowerOffDate == todayDate )
       {
       if ( !isSoftRestart )
        {
        if ( ntpcurEpochTime < lastPowerOff ) { lastPowerOff = lastPowerOff - (millis() / 1000); }
        logdata = { lastPowerOff, LOG_EVENT_POWER_OFF, 0, 0, 0 };
        f.write((uint8_t *)&logdata, sizeof(logdata));
        yield(); delay(100); yield();  
        }
       }
      else // lastPowerOffDate == todayDate
       {
       char path[32];
       log_pathFromDate(path, lastPowerOffDate);
       if ( LittleFS.exists(path) )
        {
        File fprev = LittleFS.open(path, "a");
        if ( fprev )
         {
         logdata = { lastPowerOff, LOG_EVENT_POWER_OFF, 0, 0, 0 };
         fprev.write((uint8_t *)&logdata, sizeof(logdata));
         fprev.close();
         }
        }
       }
      }
     }
    logdata = { ntpcurEpochTime - ((millis() - log_delayed_write_buffer[i].utc) / 1000)
               ,log_delayed_write_buffer[i].event, log_delayed_write_buffer[i].ch
               ,log_delayed_write_buffer[i].task, logdata.act };
    f.write((uint8_t *)&logdata, sizeof(logdata));
    yield(); delay(100); yield();  
    log_delayed_write_buffer[i].event = LOG_POLLING; 
    }
   else 
    { break; }    
   }
  f.close(); 
  }
 }
if ( ev != LOG_POLLING )
 { 
 log_process();
 logdata = { ntpcurEpochTime, ev, ch, ts, act };
 File f = LittleFS.open(log_curPath, "a");
 if ( f ) { f.write((uint8_t *)&logdata, sizeof(logdata)); f.close(); }
 }
}

void getLastResetReason()
{
rst_info *resetInfo;
byte reason;
resetInfo = ESP.getResetInfoPtr();
reason = (*resetInfo).reason;
#ifdef DEBUG
 Serial.print(F("Last reset reason code = "));
 Serial.println(reason);
#endif
     if ( reason == REASON_DEFAULT_RST ) { log_Append(LOG_EVENT_RESTART_REASON_0); } // 0 normal startup by power on
else if ( reason == REASON_WDT_RST ) { log_Append(LOG_EVENT_RESTART_REASON_1); } // 1 hardware watch dog reset
else if ( reason == REASON_EXCEPTION_RST ) { log_Append(LOG_EVENT_RESTART_REASON_2); } // 2 exception reset, GPIO status won’t change
else if ( reason == REASON_SOFT_WDT_RST ) { log_Append(LOG_EVENT_RESTART_REASON_3); } // 3 software watch dog reset, GPIO status won’t change
else if ( reason == REASON_SOFT_RESTART ) { log_Append(LOG_EVENT_RESTART_REASON_4); } // 4 software restart , system_restart , GPIO status won’t change
else if ( reason == REASON_DEEP_SLEEP_AWAKE ) { log_Append(LOG_EVENT_RESTART_REASON_5); } // 5 wake up from deep-sleep
else if ( reason == REASON_EXT_SYS_RST ) { log_Append(LOG_EVENT_RESTART_REASON_6); } // 6 external system reset
else { log_Append(LOG_EVENT_RESTART_REASON_UNKNOWN); }
}

void checkFirmwareUpdated()
{
if ( EEPROM.read(BUILD_HOUR_EEPROM_ADDRESS) != FW_H || EEPROM.read(BUILD_MIN_EEPROM_ADDRESS)  != FW_M || EEPROM.read(BUILD_SEC_EEPROM_ADDRESS)  != FW_S ) 
 {
 EEPROM.write(BUILD_HOUR_EEPROM_ADDRESS, FW_H);
 EEPROM.write(BUILD_MIN_EEPROM_ADDRESS, FW_M);
 EEPROM.write(BUILD_SEC_EEPROM_ADDRESS, FW_S);
 EEPROM.commit();
 log_Append(LOG_EVENT_FIRMWARE_UPDATED); 
 }
}

void ntpTimeSynked() 
{  
bool appLog = !ntpTimeIsSynked;
ntpTimeIsSynked = true;  
ntpLastPollingSuccess = true;
check_DaylightSave();
if ( appLog || rtcmemTimeIsSynced ) 
 { 
 ntpGetTime(); 
 log_Append(LOG_EVENT_NTP_TIME_SYNC_SUCCESS); 
 rtcmemTimeIsSynced = false;
 }
ntpTimeIsSynked = ( ntpcurEpochTime > NTP_MINIMUM_EPOCHTIME );
ntpLastSynkedTimer = millis();
}

uint32_t calculateCRC32(const uint8_t *data, size_t length) 
{
uint32_t crc = 0xffffffff;
while (length--) 
 {
 uint8_t c = *data++;
 for ( uint32_t i = 0x80; i > 0; i >>= 1 )
  {
  bool bit = crc & 0x80000000;
  if ( c & i ) { bit = !bit; }
  crc <<= 1;
  if ( bit ) { crc ^= 0x04c11db7; }
  }
 }
return crc;
}

void RTCMEMread()
{
#ifdef DEBUG 
 Serial.println(F("Read RTCMEM:"));
#endif
if ( ESP.rtcUserMemoryRead(0, (uint32_t *)&RTCMEMData, sizeof(RTCMEMData)) ) 
 {
 uint32_t crcOfRTCMEMEpochTime = calculateCRC32((uint8_t *)&RTCMEMData.RTCMEMEpochTime, sizeof(RTCMEMData.RTCMEMEpochTime));
 if ( crcOfRTCMEMEpochTime == RTCMEMData.RTCMEMcrc32 )
  {
  ntpcurEpochTime = RTCMEMData.RTCMEMEpochTime + (millis() / 1000) + 1;
  ntpTimeInfo = gmtime(&ntpcurEpochTime); 
  curIsDaylightSave = RTCMEMData.RTCMEMcurIsDaylightSave;
  #ifdef DEBUG 
   Serial.println(F("CRC OK, set datetime from RTCMEM"));
  #endif
  timeval tv = { ntpcurEpochTime, 0 };
  settimeofday(&tv, nullptr);
  ntpTimeIsSynked = true;
  yield(); delay(1); yield();
  ntpGetTime();
  log_Append(LOG_EVENT_RTCMEM_TIME_SYNC_SUCCESS);
  rtcmemTimeIsSynced = true;
  isSoftRestart = true;
  } 
 }
else { log_Append(LOG_EVENT_RTCMEM_READ_ERROR); }
}

void checkNewDayLogCreate()
{
static time_t previous_day = 0;
if ( !littleFS_OK ) { return; }
if ( ntpcurEpochTime < NTP_MINIMUM_EPOCHTIME ) { return; }
if ( !ntpTimeIsSynked ) { return; }
time_t today = DateOfDateTime(ntpcurEpochTime);
if ( today != previous_day ) // we have switched to another day
 {
 char log_curPath[32];
 log_pathFromDate(log_curPath, today);
 if ( !LittleFS.exists(log_curPath) )
  {
  struct LastChannelsStates lastchannelsstates = read_lastchannelsstates();
  struct Log_Data logdata;
  time_t logtime = ntpcurEpochTime;
  if ( !isSoftRestart && previous_day == 0 ) { logtime = ntpcurEpochTime - (millis() / 1000); }
  logdata = { logtime, LOG_EVENT_NEWDAYLOG_CREATED, lastchannelsstates.firstpart, lastchannelsstates.lastpart, 0 };
  File f = LittleFS.open(log_curPath, "a");
  if ( f ) 
   { 
   f.write((uint8_t *)&logdata, sizeof(logdata)); 
   f.close(); 
   #ifdef DEBUG 
    Serial.print(F("New daylog file "));
    Serial.print(log_curPath);
    Serial.print(F(" was created"));
    Serial.println();
   #endif
   }
  }
 previous_day = today;
 }  
}

void ntpGetTime() 
{
static unsigned long everySecondTimer = 0;  
static bool writeErrorToLogOnlyOnce = false;
if ( !ntpTimeIsSynked ) { return; }
time_t GMT0epochtime;
yield(); delay(1); yield();
time(&GMT0epochtime); // get GMT0 epoch time (this function calls the NTP server every NTP_POLLING_INTERVAL ms)
if ( GMT0epochtime < NTP_MINIMUM_EPOCHTIME ) { return; }
// convert GMT0 epoch time to local epoch time 
ntpcurEpochTime = GMT0epochtime + (ntpTimeZone * 3600) + (curIsDaylightSave * 3600);
ntpTimeInfo = gmtime(&ntpcurEpochTime); 
yield(); delay(1); yield();
if ( millis() - everySecondTimer > 1000UL )
 {
 RTCMEMData.RTCMEMEpochTime = GMT0epochtime;
 RTCMEMData.RTCMEMcrc32 = calculateCRC32((uint8_t *)&RTCMEMData.RTCMEMEpochTime, sizeof(RTCMEMData.RTCMEMEpochTime));
 RTCMEMData.RTCMEMcurIsDaylightSave = curIsDaylightSave;
 if ( !ESP.rtcUserMemoryWrite(0, (uint32_t *)&RTCMEMData, sizeof(RTCMEMData)) )
  { 
  if ( !writeErrorToLogOnlyOnce ) 
   {
   log_Append(LOG_EVENT_RTCMEM_WRITE_ERROR);
   writeErrorToLogOnlyOnce = true;
   }
  }
 checkNewDayLogCreate();
 everySecondTimer = millis(); 
 }
if ( millis() - ntpLastSynkedTimer > NTP_POLLING_INTERVAL + 60000 )
 {
 if ( ntpLastPollingSuccess ) 
  { 
  log_Append(LOG_EVENT_NTP_TIME_SYNC_ERROR); 
  ntpLastPollingSuccess = false;
  }
 ntpLastSynkedTimer = millis(); 
 }
}

uint8_t calcOldChMode(int chNum)
{
     if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_MANUALLY || ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_MANUALLY )
 { return LOG_EVENT_CHANNEL_MANUALLY; }
else if ( ChannelList[chNum][CHANNEL_LASTSTATE] >= 50 && ChannelList[chNum][CHANNEL_LASTSTATE] < 250 )
 { return LOG_EVENT_CHANNEL_UNTIL_NEXT_TASK; }
else if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK || ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_BY_TASK )
 { return LOG_EVENT_CHANNEL_BY_TASK; }
return 0;
}

boolean ipEmpty(IPAddress ip)
{
uint8_t cnt0 = 0;
uint8_t cnt255 = 0;
for ( int i = 0; i < 4; i++ ) 
 { 
 if ( ip[i] != 0 && ip[i] != 255 ) { return false; }
 if ( ip[i] == 0 ) { cnt0++; }
 if ( ip[i] == 255 ) { cnt255++; }
 }
if ( cnt0 != 4 && cnt255 != 4 ) { return false; } // e.g. subnet mask 255.255.0.0
return true;
}

void read_manually_set_addresses()
{
for ( int cnt = 0; cnt < 4; cnt++ )
 { 
 wifiManuallySetIP[cnt]     = EEPROM.read(cnt + WIFI_MANUALLY_SET_IP_EEPROM_ADDRESS);
 wifiManuallySetDNS[cnt]    = EEPROM.read(cnt + WIFI_MANUALLY_SET_DNS_EEPROM_ADDRESS);
 wifiManuallySetGW[cnt]     = EEPROM.read(cnt + WIFI_MANUALLY_SET_GW_EEPROM_ADDRESS);
 wifiManuallySetSUBNET[cnt] = EEPROM.read(cnt + WIFI_MANUALLY_SET_SUBNET_EEPROM_ADDRESS); 
 }
}

void save_manually_set_addresses()
{
for ( int cnt = 0; cnt < 4; cnt++ )
 { 
 EEPROM.write(cnt + WIFI_MANUALLY_SET_IP_EEPROM_ADDRESS,     wifiManuallySetIP[cnt]);
 EEPROM.write(cnt + WIFI_MANUALLY_SET_DNS_EEPROM_ADDRESS,    wifiManuallySetDNS[cnt]);
 EEPROM.write(cnt + WIFI_MANUALLY_SET_GW_EEPROM_ADDRESS,     wifiManuallySetGW[cnt]);
 EEPROM.write(cnt + WIFI_MANUALLY_SET_SUBNET_EEPROM_ADDRESS, wifiManuallySetSUBNET[cnt]); 
 }
EEPROM.commit();  
}

bool isSummerTimeNow()
{
if ( !ntpAutoDaylightSavingEnabled ) { return false; }
time_t GMT0ET;
time(&GMT0ET);
struct tm *GMT0ETinfo;
GMT0ETinfo = gmtime(&GMT0ET);
if ( ntpDaylightSaveZone == 0 ) // EU
 {
 if ( (GMT0ETinfo->tm_mon + 1) < 3 || (GMT0ETinfo->tm_mon + 1) > 10 ) return false; // Jan, Feb, Nov, Dec
 if ( (GMT0ETinfo->tm_mon + 1) > 3 && (GMT0ETinfo->tm_mon + 1) < 10 ) return true;  // Apr, May, Jun, Jul, Aug, Sep
 return ( ((GMT0ETinfo->tm_mon + 1) == 3 && ((GMT0ETinfo->tm_hour + 24 * GMT0ETinfo->tm_mday) >= (1 + 24 * (31 - (5 * (1900 + GMT0ETinfo->tm_year) / 4 + 4) % 7)))) 
      || (((GMT0ETinfo->tm_mon + 1) == 10 && (GMT0ETinfo->tm_hour + 24 * GMT0ETinfo->tm_mday) <  (1 + 24 * (31 - (5 * (1900 + GMT0ETinfo->tm_year) / 4 + 1) % 7)))) );
 }
if ( ntpDaylightSaveZone == 1 ) // USA
 {
 if ( (GMT0ETinfo->tm_mon + 1) < 3 || (GMT0ETinfo->tm_mon + 1) > 11 ) return false;
 if ( (GMT0ETinfo->tm_mon + 1) > 3 && (GMT0ETinfo->tm_mon + 1) < 11 ) return true;
 uint8_t first_sunday = (7 + GMT0ETinfo->tm_mday - GMT0ETinfo->tm_wday) % 7 + 1; // first sunday of current month
 if ( (GMT0ETinfo->tm_mon + 1) == 3 ) // Starts at 2:00 am on the second sunday of Mar
  {
  if ( GMT0ETinfo->tm_mday < 7 + first_sunday ) return false;
  if ( GMT0ETinfo->tm_mday > 7 + first_sunday ) return true;
  return ( GMT0ETinfo->tm_hour > 2 );
  }
 if ( GMT0ETinfo->tm_mday < first_sunday ) return true; // Ends a 2:00 am on the first sunday of Nov. We are only getting here if its Nov
 if ( GMT0ETinfo->tm_mday > first_sunday ) return false;
 return (GMT0ETinfo->tm_hour < 2);
 }
return false;
}

void check_DaylightSave()
{ 
curIsDaylightSave = isSummerTimeNow();
ntpGetTime();
if ( curIsDaylightSave != previousIsDaylightSave )
 { 
 ntpGetTime();
 if ( millis() > 60000 ) 
  { 
  log_Append( (curIsDaylightSave ? LOG_EVENT_DAYLIGHT_SAVING_ON : LOG_EVENT_DAYLIGHT_SAVING_OFF) ); 
  }
 if ( previousIsDaylightSave ) // time is set back an hour
  {
  RingCounter.clear();
  RingCounter.init();
  saveGMTtimeToRingCounter();
  }
 previousIsDaylightSave = curIsDaylightSave; 
 }
} 

void begin_WiFi_STA()
{
WiFi.mode(WIFI_STA);
WiFi.softAPdisconnect(true);
if ( wifiManuallySetAddresses ) { WiFi.config(wifiManuallySetIP, wifiManuallySetGW, wifiManuallySetSUBNET, wifiManuallySetDNS); }
#ifdef DEBUG 
 Serial.print(F("Connecting to "));
 Serial.print(AP_name);
#endif
WiFi.begin(AP_name, AP_pass);
unsigned long bt = millis();
while ( millis() - bt < WIFICONNECTIONTRYPERIOD )
 {
 yield(); delay(500);
 statusWiFi = (WiFi.status() == WL_CONNECTED);
 #ifdef DEBUG 
  Serial.print(F("."));
 #endif
 if ( statusWiFi ) 
  { 
  #ifdef DEBUG 
   Serial.print(F("OK"));
  #endif
  break; 
  }
 }
#ifdef DEBUG 
 Serial.println();
#endif
log_Append(statusWiFi ? LOG_EVENT_WIFI_CONNECTION_SUCCESS : LOG_EVENT_WIFI_CONNECTION_ERROR); 
previousstatusWiFi = statusWiFi; 
#ifdef DEBUG 
 yield(); delay(500);
 if ( statusWiFi ) 
  {
  Serial.print(F("Connected to    ")); Serial.println(AP_name);
  Serial.print(F("MAC address     ")); Serial.println(WiFi.macAddress());
  Serial.print(F("IP address      ")); Serial.println(WiFi.localIP()); 
  Serial.print(F("Gateway address ")); Serial.println(WiFi.gatewayIP());
  Serial.print(F("DNS address     ")); Serial.println(WiFi.dnsIP());
  Serial.print(F("Subnet mask     ")); Serial.println(WiFi.subnetMask());
  Serial.print(F("MDNS access     http://")); Serial.print(MDNSHOST); Serial.println(F(".local"));
  }
 else 
  { Serial.println(F("Can't connect to WiFi.")); }
#endif
if ( !statusWiFi )
 {
 #ifdef DEBUG 
  Serial.println(F("Going to access point mode after restart."));
  Serial.print(F("Access point SSID = "));
  Serial.print(APMODE_SSID);
  Serial.print(F(" password = "));
  Serial.print(APMODE_PASSWORD);
  Serial.println(F(" URL = http://192.168.4.1"));
 #endif
 EEPROM.write(ACCESS_POINT_SIGNATURE_EEPROM_ADDRESS, ACCESS_POINT_SIGNATURE); 
 EEPROM.commit();
 log_Append(LOG_EVENT_RESTART_TO_AP_MODE); 
 ESP.restart(); 
 }
if ( !WiFi.getAutoConnect() )
 {
 WiFi.setAutoConnect(true);
 WiFi.setAutoReconnect(true);
 }
WiFi.persistent(true);
}

void begin_WiFi_AP() 
{
WiFi.mode(WIFI_AP);
WiFi.softAP(APMODE_SSID, APMODE_PASSWORD);
#ifdef DEBUG 
 Serial.println(F("Starting in access point mode."));
 Serial.print(F("SSID = "));
 Serial.print(APMODE_SSID);
 Serial.print(F(" Password = "));
 Serial.print(APMODE_PASSWORD);
 Serial.println(F(" URL = http://192.168.4.1"));
#endif
statusWiFi = true;
previousstatusWiFi = statusWiFi; 
}

void ServerSendMessageAndRefresh( int interval = 0, String url = "/", String mess1 = "", String mess2 = "", String mess3 = "", String mess4 = "" )
{
String ans = F("<META http-equiv='refresh' content='");
ans += String(interval) + F(";URL=") + url + F("'>") + mess1 + F("&nbsp;") + mess2 + F("&nbsp;") + mess3 + F("&nbsp;") + mess4;
server.send(200, F("text/html; charset=utf-8"), ans);
}

void ServerSendMessageAndReboot()
{
String mess = (Language ? F(" Перезагрузка...") : F(" Rebooting..."));
ServerSendMessageAndRefresh( 10, "/", mess );
delay(500);
server.stop();
ESP.restart(); 
}

int calcCFGfiles()
{
if ( !littleFS_OK ) { return 0; }
Dir dir = LittleFS.openDir("/");
int ret = 0;
while ( dir.next() )
 {
 if ( dir.isFile() && dir.fileSize() ) { ret++; } 
 yield();
 }
return ret; 
}

void handleFileUpload()
{
if ( !littleFS_OK ) { return; }
if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
if ( server.uri() != "/uploadfile" ) return;
HTTPUpload& upload = server.upload();
if ( upload.status == UPLOAD_FILE_START )
 {
 String filename = upload.filename;
 if ( !filename.startsWith("/") ) filename = "/" + filename;
 uploadFileHandle = LittleFS.open(filename, "w");
 filename = String();
 } 
else if ( upload.status == UPLOAD_FILE_WRITE ) 
 { if ( uploadFileHandle ) { uploadFileHandle.write(upload.buf, upload.currentSize); } }
else if ( upload.status == UPLOAD_FILE_END ) 
 { if ( uploadFileHandle ) { uploadFileHandle.close(); } }
}

void log_process()
{
static time_t log_today = 0;
static bool log_processAfterStart = true;
static unsigned long log_lastProcess = 0;
if ( !littleFS_OK ) { return; }
if ( ntpcurEpochTime < NTP_MINIMUM_EPOCHTIME ) { return; }
unsigned long currentMillis = millis();
if  ( currentMillis - log_lastProcess > 1000UL || log_processAfterStart )
 {
 time_t today = DateOfDateTime(ntpcurEpochTime);
 if ( log_today != today ) // we have switched to another day
  { 
  log_today = today;
  log_pathFromDate(log_curPath, log_today);
  Dir dir = LittleFS.openDir(LOG_DIR);
  while ( dir.next() )
   {
   time_t midnight = log_filenameToDate(dir.fileName());
   if ( midnight <= (today - log_DaysToKeep * 86400) )
    {
    LittleFS.remove(String(LOG_DIR) + "/" + String(dir.fileName())); 
    } 
   }
  }
 log_lastProcess = currentMillis;
 log_processAfterStart = false;
 }
} 

void log_pathFromDate(char *output, time_t date)
{
if ( ntpcurEpochTime < NTP_MINIMUM_EPOCHTIME ) { return; }
if ( date == 0 ) { date = DateOfDateTime(ntpcurEpochTime); }
struct tm *tinfo = gmtime(&date);
sprintf_P(output, "%s/%d%02d%02d", LOG_DIR, 1900 + tinfo->tm_year, tinfo->tm_mon + 1, tinfo->tm_mday);
}

time_t log_filenameToDate(String filename) 
{
struct tm tm = {0,0,0,1,0,100,0,0,0};
struct tm start2000 = {0,0,0,1,0,100,0,0,0};
String ss;
ss = filename.substring(0,4); tm.tm_year = ss.toInt() - 1900;
ss = filename.substring(4,6); tm.tm_mon = ss.toInt() - 1;
ss = filename.substring(6,8); tm.tm_mday = ss.toInt();
return ( mktime(&tm) - (mktime(&start2000) - 946684800) ) / SECS_PER_DAY * SECS_PER_DAY;
}

boolean log_readRow(Log_Data *output, size_t rownumber, File fileHandle)
{
if ( ((int32_t)(fileHandle.size() / sizeof(Log_Data)) - (int32_t)rownumber) <= 0 ) { return false; }
fileHandle.seek(rownumber * sizeof(Log_Data));
fileHandle.read((uint8_t *)output, sizeof(Log_Data));
return true;
}

size_t log_FileSizeForDate(time_t date)
{
char path[32];
log_pathFromDate(path, date);
if ( !LittleFS.exists(path) ) return 0;
File f = LittleFS.open(path, "r");
size_t fs = f.size();
f.close();
return fs;
}

time_t makeTime(int Year, int Month, int Day, int Hour, int Minute, int Second)
{   
int i;
uint32_t seconds = Year * (SECS_PER_DAY * 365);
for ( i = 0; i < Year; i++ )
 { if ( LEAP_YEAR(i) ) { seconds += SECS_PER_DAY; } }
for ( i = 1; i < Month; i++ )
 { 
 if ( (i == 2) && LEAP_YEAR(Year) ) { seconds += SECS_PER_DAY * 29; }
                               else { seconds += SECS_PER_DAY * monthDays[i-1]; }
 }
seconds += ( Day - 1 ) * SECS_PER_DAY;
seconds += Hour * SECS_PER_HOUR;
seconds += Minute * SECS_PER_MIN;
seconds += Second;
return (time_t)seconds; 
}

void log_CalcForViewDate()
{ 
struct tm *ptm = gmtime((time_t *)&log_ViewDate); 
log_Day = ptm->tm_mday;
log_Month = ptm->tm_mon + 1;
log_Year = ptm->tm_year + 1900 ;
log_NumRecords = log_FileSizeForDate(log_ViewDate) / sizeof(Log_Data); 
log_StartRecord = log_NumRecords;
log_CurDate = String(log_Year) + ( log_Month < 10 ? F("0") : F("") ) + String(log_Month) + ( log_Day < 10 ? F("0") : F("") ) + String(log_Day);
}

int drawFSinfo()
{
String content = "";
FSInfo fs_info;
LittleFS.info(fs_info);
content += (Language ? F("<i>Всего файловых блоков: <b>") : F("<i>Total file blocks: <b>"));
content += String(littleFStotalBytes / littleFSblockSize);
content += (Language ? F("</b>&emsp;Занято блоков: <b>") : F("</b>&emsp;Used blocks: <b>"));
content += String(fs_info.usedBytes / littleFSblockSize);
content += F("</b>&emsp;");
if ( littleFStotalBytes - fs_info.usedBytes <= littleFSblockSize ) { content += F("<font color='red'>"); }
content += (Language ? F("Доступно блоков: <b>") : F("Free blocks: <b>"));
content += String((littleFStotalBytes - fs_info.usedBytes) / littleFSblockSize);
content += F("</b></font>&emsp;");
content += (Language ? F("Файлов с настройками: <b>") : F("Files with settings: <b>"));
content += String(numCFGfiles);
content += F("</b>&emsp;");
content += (Language ? F("Файлов журналов: <b>") : F("Log files: <b>"));
content += String(numLOGfiles);
content += F("</b></i>");
if ( log_DaysToKeep - numLOGfiles > int((littleFStotalBytes - fs_info.usedBytes) / littleFSblockSize) )
 {
 content += F("<br><br><font color='red'><b>");
 content += (Language ? F("! НЕДОСТАТОЧНО МЕСТА ДЛЯ ФАЙЛОВ ЖУРНАЛОВ ЗА ") : F("! NOT ENOUGH SPACE FOR JOURNAL FILES IN "));
 content += String(log_DaysToKeep);
 content += (Language ? F(" ДН. !") : F(" DAYS !"));
 content += F("</b></font>");
 }
server.sendContent(content); content = "";
yield();
return fs_info.usedBytes;
}

void drawHeader(byte index) // 0 - homepage, 1 - logview page, 2 - access point page
{
numCFGfiles = calcCFGfiles();
String content = F("<link rel='shortcut icon' href='data:image/x-icon;base64,AAABAAEAEBAAAAEAIABoBAAAFgAAACgAAAAQAAAAIAAAAAEAIAAAAAAAQAQAAAAAAAAAAAAAAAAAAAAAAADCt6pgwrep/cK4qv/CuKr/wriq/8K4qv/CuKr/wriq/8K4qv/CuKr/wriq/8K4qv/CuKr/wriq/8K3qf3DuaxiwrmqisK4qv/CuKr/wriq/8K4qv/CuKr/vrSm/7WrnP+1q5z/vrSm/8K4qv/CuKr/wriq/8K4qv/CuKr/w7ipjMK5qorCuKr/wriq/8K4qv+zqZr/kYd4/5CHev+ln5T/pZ+U/5CHev+Rh3j/s6ma/8K4qv/CuKr/wriq/8O4qYzCuaqKwriq/8K4qv+jmYv/lo6C/9/d2f//////t7Wz/7e1s///////393Z/5aOgv+jmYv/wriq/8K4qv/DuKmMwrmqisK4qv+qoJH/npaL/6Cem//8/Pv//////9/f3v/f397///////z8+/+gnpv/npaL/6qgkf/CuKr/w7ipjMK5qoq/tab/i4Jz//Py8P+fnZr/7ezs///////////////////////t7Ov/n52a//Py8P+LgnP/v7Wm/8O4qYzCuaqKqZ+R/7awqP//////////////////////////////////////////////////////trCo/6mfkf/DuKmMwrmqipqQgv/X1M///////////////////////////////////////////////////////9fUz/+akIL/w7ipjMK5qoqWjH3/tbKu/1tYU///////+fj+/7ev8v9oVuP/xb70/////////////////1tYU/+1sq7/lox9/8K5q4vCt6t5m5GD/9XSzf/9/f7/npPt/1xI4f+RhOv/4d75////////////////////////////1dLN/5uRg//Ct6t5wrqrQ6uhk/+xq6H//fz+/7mx8v/z8v3//////////////////////////////////////7Grof+roZP/wrqrQ7+/vwS/tqfiioFy/+3s6f+IhoL/6urp///////////////////////q6un/iIaC/+3s6f+KgXL/v7an4r+/vwQAAAAAw7iqXa+llv+WjoL/sa+s//7+/v//////1dTT/9XU0////////v7+/7GvrP+WjoL/r6WW/8O4ql0AAAAAAAAAAAAAAADDuauYqqCS/4+Gef/Rzsn/+/v7/8HAvv/BwL7/+/v7/9HOyf+Phnn/qqCS/8O5q5gAAAAAAAAAAAAAAAAAAAAAgICAAsG3qYC5rqD6mY+A/4l/cf+Xj4P/l4+D/4l/cf+Zj4D/ua6g+sG3qYCAgIACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAw7WoJsO5q5HBuKrXvLKk+byypPnBuKrXw7mrkcO1qCYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==' />");
content += F("<head><title>");
content += (Language ? F("Универсальный таймер") : F("Versatile timer"));
content += F("</title></head><html><style>");
content += F(".dac{width:100%;text-align:center}");
content += F(".subdiv{width:600px;padding-left: 4px;}");
content += F(".maindiv,.subdiv{margin-left:auto;margin-right:auto;}");
content += F(".maindiv{padding:6px;width:900px;border: 1px grey solid;}");
content += F("input[type=number]{width:38px;text-align: right;}");
content += F("hr {border:none; background:grey; height:1px;}");
content += F("</style>");
content += F("<script>function warn(){if (confirm('");
content += (Language ? F("Вы уверены ?") : F("Are you sure ?"));
content += F("'))f.submit();}</script>");
content += F("<body style='background: #87CEFA'><div class='maindiv'><div class='dac'><span style='font-size:18pt'>");
server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
server.sendHeader("Pragma", "no-cache");
server.sendHeader("Expires", "-1");
server.setContentLength(CONTENT_LENGTH_UNKNOWN);
server.send(200, F("text/html; charset=utf-8"), "");
server.sendContent(content);
yield();
content = "";
if ( index == 0 )
 {
 content += (Language ? F("Универсальный программируемый таймер, версия ") : F("Versatile timer, version "));
 content += VERSION;
 content += (Language ? F("</span><p>Время: <b>") : F("</span><p>Time: <b>"));
 if ( ntpcurEpochTime > NTP_MINIMUM_EPOCHTIME )
  {
  content += String(ntpTimeInfo->tm_hour) + F(":")  + ( ntpTimeInfo->tm_min < 10 ? "0" : "" ) + String(ntpTimeInfo->tm_min) + F(":") 
          + ( ntpTimeInfo->tm_sec < 10 ? "0" : "" ) + String(ntpTimeInfo->tm_sec) + F("</b>,&nbsp;<b>");
  content += namesOfDays[Language][ntpTimeInfo->tm_wday];
  }
 else
  {
  content += F("&emsp;<font color='red'>"); 
  content += (Language ? F("Ошибка синхронизации времени") : F("Time sync error"));
  content += F("</font>"); 
  }
 content += (Language ? F("</b>&emsp;Время работы: <b>") : F("</b>&emsp;Uptime: <b>"));
 content += convertMStoDHMS(millis());
 content += (Language ? F("</b>&emsp;Прошивка загружена: <b>") : F("</b>&emsp;Firmware uploaded: <b>"));
 content += String(__DATE__);
 content += (Language ? F("</b>&emsp;Перезагрузок: <b>") : F("</b>&emsp;Reboots: <b>"));
 content += String(counterOfReboots);
 content += F("</p><hr>");
 }
else if ( index == 1 )
 {
 Dir dir = LittleFS.openDir(LOG_DIR);
 numLOGfiles = 0;
 log_CurDate = F("00000000"); 
 log_MinDate = F("99999999");
 log_MaxDate = F("00000000");
 while ( dir.next() )
  {
  if ( dir.isFile() && dir.fileSize() )
   {
   log_CurDate = dir.fileName();
   if ( log_CurDate.compareTo(log_MaxDate) > 0 ) { log_MaxDate = log_CurDate; }
   if ( log_CurDate.compareTo(log_MinDate) < 0 ) { log_MinDate = log_CurDate; }
   numLOGfiles++;
   }
  yield();
  }
 content += (Language ? F("Журнал на ") : F("Daylog for "));
 content += String(log_Day) + F(".");
 if ( log_Month < 10 ) { content += F("0"); }
 content += String(log_Month) + F(".");
 content += String(log_Year) + F("</span><p>");
 server.sendContent(content); content = "";
 yield();
 drawFSinfo();
 content += F("</p><p>");
 content += (Language ? F("Найдены журналы на даты: <b>") : F("Found logs for dates: <b>"));
 content += log_MinDate.substring(6,8) + F(".");
 content += log_MinDate.substring(4,6) + F(".");
 content += log_MinDate.substring(0,4) + F("<b> - </b>");
 content += log_MaxDate.substring(6,8) + F(".");
 content += log_MaxDate.substring(4,6) + F(".");
 content += log_MaxDate.substring(0,4);
 }
else if ( index == 2 )
 {
 content += (Language ? F("Универсальный программируемый таймер</span><p>") : F("Versatile timer</span><p>"));
 content += (Language ? F("Версия: <b>") : F("Version: <b>"));
 content += String(__DATE__);
 content += (Language ? F("</b>&emsp;Перезагрузок: <b>") : F("</b>&emsp;Reboots: <b>"));
 content += String(counterOfReboots);
 content += F("</p><hr>");
 }
content += F("</b></p></div>");
server.sendContent(content);
yield();
}

void drawLOG()
{
String content = F("<hr><table><tr>");
content += F("<th>№</th>");
content += (Language ? F("<th>Дата и время</th>") : F("<th>Date&time</th>"));
content += (Language ? F("<th>Событие</th>") : F("<th>Event</th>"));
content += (Language ? F("<th>Задание</th>") : F("<th>Task</th>"));
content += (Language ? F("<th>&emsp;Канал</th>") : F("<th>&emsp;Channel</th>"));
content += (Language ? F("<th>&emsp;Новое состояние</th>") : F("<th>&emsp;New state</th>"));
content += F("</tr>");
int counter = 0;
int ls = log_StartRecord;
if ( ls < 1 ) { ls = log_ViewStep; }
struct Log_Data rl;
char path[32];
log_pathFromDate(path, log_ViewDate);
if ( LittleFS.exists(path) )
 {
 log_FileHandle = LittleFS.open(path, "r");
 if ( log_FileHandle )
  {
  while ( log_readRow(&rl, ls - 1, log_FileHandle) )
   { 
   content += F("<tr><td align='center'>");
   content += String(ls) + F("</td><td>&emsp;");
   struct tm *ptm = gmtime((time_t *)&rl.utc); 
   content += (rl.event == LOG_EVENT_START ? F("<u>") : F(""));
   content += String(ptm->tm_mday) + F(".") 
            + (ptm->tm_mon + 1 < 10 ? F("0") : F("")) + String(ptm->tm_mon + 1) + F(".") 
            + String(ptm->tm_year + 1900) + F("&emsp;")
            + (ptm->tm_hour < 10 ? F("0") : F("")) + String(ptm->tm_hour) + F(":") 
            + (ptm->tm_min  < 10 ? F("0") : F("")) + String(ptm->tm_min) + F(":") 
            + (ptm->tm_sec  < 10 ? F("0") : F("")) + String(ptm->tm_sec) 
            + (rl.event == LOG_EVENT_START ? F("</u>") : F(""))
            + F("</td><td>&emsp;");
   if ( rl.event == LOG_EVENT_START || rl.event == LOG_EVENT_RESTART_TO_AP_MODE
     || rl.event == LOG_EVENT_DAYLIGHT_SAVING_ON || rl.event == LOG_EVENT_DAYLIGHT_SAVING_OFF 
     || rl.event >= LOG_EVENT_NUMBER_OF_TASKS_CHANGED
     || (rl.event >= LOG_EVENT_RESTART_REASON_0 && rl.event <= LOG_EVENT_RESTART_REASON_UNKNOWN)
      )
    {
    content += (rl.event == LOG_EVENT_START ? F("<u>") : F("")) + namesOfEvents[Language][rl.event];
    if ( rl.event == LOG_EVENT_START || rl.event == LOG_EVENT_NEWDAYLOG_CREATED )
     {
     content += (rl.event == LOG_EVENT_START ? F("</u>") : F(""));
     content += (Language ? F(" (состояния каналов: ")  : F(" (channels states: "));
     for ( byte i = 0; i < numberOfChannels; i++ )
      {
      if ( i < 8 ) { content += String(bitRead(rl.ch, i)); }
              else { content += String(bitRead(rl.task, i - 8)); }
      if ( i != numberOfChannels - 1 ) { content += F("-"); }        
      }
     content += F(")");
     }  
    }
   else if ( rl.event == LOG_EVENT_TASK_SWITCHING_MANUALLY || rl.event == LOG_EVENT_TASK_SWITCHING_BY_TASK ) 
    {
    content += namesOfEvents[Language][rl.event];
    content += F("</td><td align='right'>");
    content += (rl.event == LOG_EVENT_TASK_SWITCHING_BY_TASK ? String(rl.task + 1) : F("&nbsp;"));
    content += F("</td><td align='right'>");
    content += String(rl.ch + 1);
    content += F("</td><td align='left'>&emsp;");
    if ( rl.act ) { content += (Language ? F("ВКЛ")  : F("ON")); } 
             else { content += (Language ? F("ВЫКЛ") : F("OFF")); }
    }
   else if ( rl.event == LOG_EVENT_CHANNEL_MANUALLY || rl.event == LOG_EVENT_CHANNEL_UNTIL_NEXT_TASK || rl.event == LOG_EVENT_CHANNEL_BY_TASK ) 
    {
    content += (Language ? F("Режим канала изменён с '") : F("Channel mode changed from '"));
    content += namesOfEvents[Language][rl.event];
    content += F("'</td><td>&nbsp;</td><td align='right'>");
    content += String(rl.ch + 1);
    content += F("</td><td align='left'>&emsp;'");
    content += namesOfEvents[Language][rl.act];
    content += F("'");
    }
   else if ( rl.event == LOG_EVENT_TASK_SETTINGS_CHANGED || rl.event == LOG_EVENT_CHANNEL_SETTINGS_CHANGED ) 
    {
    content += namesOfEvents[Language][rl.event];
    content += F("</td><td align='right'>");
    content += (rl.event == LOG_EVENT_TASK_SETTINGS_CHANGED ? String(rl.task + 1) : F("&nbsp;"));
    content += F("</td><td align='right'>");
    content += (rl.event == LOG_EVENT_CHANNEL_SETTINGS_CHANGED ? String(rl.ch + 1) : F("&nbsp;"));
    }
   content += F("</td></tr>");
   ls--;
   counter++;
   if ( ls < 1 ) { break; }
   if ( counter >= log_ViewStep ) { break; }
   yield();
   }
  log_FileHandle.close();
  }
 }
content += F("</tr></table><p>");
log_CurDate = String(log_Year) + ( log_Month < 10 ? F("0") : F("") ) + String(log_Month) + ( log_Day < 10 ? F("0") : F("") ) + String(log_Day);
if ( logstat_BegDate == "" ) { logstat_BegDate = log_CurDate; }
if ( logstat_EndDate == "" ) { logstat_EndDate = log_CurDate; }
if ( log_NumRecords > 0 )
 {
 content += (Language ? F("Записи с <b>") : F("Records from <b>"));
 content += String(log_StartRecord);
 content += (Language ? F("</b> по <b>") : F("</b> to <b>"));
 content += String(ls + 1);
 content += (Language ? F("</b> из <b>") : F("</b> of <b>"));
 content += String(log_NumRecords);
 }
else 
 { content += (Language ? F("</b><i>Записи на указанную дату не найдены</i>") : F("</b><i>No records found for the specified date</i>")); }
content += F("</b></p><form>");
content += F("<input formaction='/log_first' formmethod='get' type='submit' value='   |<   '");
if ( log_NumRecords <= log_ViewStep || log_StartRecord == log_NumRecords ) 
 { content += F("' disabled />"); } else { content += F("' />"); }
content += F("&emsp;<input formaction='/log_previous' formmethod='get' type='submit' value='   <<   '");
if ( log_NumRecords <= log_ViewStep || log_StartRecord == log_NumRecords ) 
 { content += F("' disabled />"); } else { content += F("' />"); }
content += F("&emsp;<input formaction='/log_next' formmethod='get' type='submit' value='   >>   '");
if ( log_NumRecords <= log_ViewStep || ls == 0 ) 
 { content += F("' disabled />"); } else { content += F("' />"); }
content += F("&emsp;<input formaction='/log_last' formmethod='get' type='submit' value='   >|   '");
if ( log_NumRecords <= log_ViewStep || ls == 0 ) 
 { content += F("' disabled />"); } else { content += F("' />"); }
content += F("&emsp;&emsp;<input formaction='/log_decdate' formmethod='get' type='submit' value='");
content += (Language ? F(" Предыдущий день ") : F(" Previous Day "));
if ( log_CurDate.compareTo(log_MinDate) <= 0 ) { content += F("' disabled />"); } else { content += F("' />"); }
content += F("&emsp;<input formaction='/log_incdate' formmethod='get' type='submit' value='");
content += (Language ? F(" Следующий день ") : F(" Next Day "));
if ( log_CurDate.compareTo(log_MaxDate) >= 0 ) { content += F("' disabled />"); } else { content += F("' />"); }
content += F("&emsp;&emsp;<input formaction='/log_return' formmethod='get' type='submit' value='");
content += (Language ? F("Назад на главную страницу") : F("Return to Home Page"));
content += F("' /></form>");
if ( log_MinDate != log_MaxDate )
 {
 content += F("<form action='/log_setdate'><p>");
 content += (Language ? F("Выберите дату журнала") : F("Select daylog date"));
 content += F(":&emsp;<input name='calendar' type='date' value='");
 content += String(log_Year) + F("-");
 content += ( log_Month < 10 ? F("0") : F("") ) + String(log_Month) + F("-");
 content += ( log_Day < 10 ? F("0") : F("") ) + String(log_Day);
 content += F("' max='");
 content += log_MaxDate.substring(0,4) + F("-");
 content += log_MaxDate.substring(4,6) + F("-");
 content += log_MaxDate.substring(6,8);
 content += F("' min='");
 content += log_MinDate.substring(0,4) + F("-");
 content += log_MinDate.substring(4,6) + F("-");
 content += log_MinDate.substring(6,8);
 content += F("'>&emsp;<input type='submit' value='");
 content += (Language ? F("Установить") : F("Set"));
 content += F("' /></p></form>");
 // logstat_BegDate
 content += F("<hr>");
 content += F("<form action='/log_setstatperiod'><p>");
 content += (Language ? F("Статистика за период с") : F("Statistics period from"));
 content += F(":&emsp;<input name='calendar' type='date' value='");
 content += logstat_BegDate.substring(0,4) + F("-");
 content += logstat_BegDate.substring(4,6) + F("-");
 content += logstat_BegDate.substring(6,8);
 content += F("' max='");
 content += log_MaxDate.substring(0,4) + F("-");
 content += log_MaxDate.substring(4,6) + F("-");
 content += log_MaxDate.substring(6,8);
 content += F("' min='");
 content += log_MinDate.substring(0,4) + F("-");
 content += log_MinDate.substring(4,6) + F("-");
 content += log_MinDate.substring(6,8);
 content += F("'>");
 // logstat_EndDate
 content += (Language ? F("&emsp;по") : F("&emsp;to"));
 content += F(":&emsp;<input name='calendar' type='date' value='");
 content += logstat_EndDate.substring(0,4) + F("-");
 content += logstat_EndDate.substring(4,6) + F("-");
 content += logstat_EndDate.substring(6,8);
 content += F("' max='");
 content += log_MaxDate.substring(0,4) + F("-");
 content += log_MaxDate.substring(4,6) + F("-");
 content += log_MaxDate.substring(6,8);
 content += F("' min='");
 content += log_MinDate.substring(0,4) + F("-");
 content += log_MinDate.substring(4,6) + F("-");
 content += log_MinDate.substring(6,8);
 content += F("'>&emsp;");
 content += F("<input type='submit' value='");
 content += (Language ? F("Установить") : F("Set"));
 content += F("' /></p></form>");
 }
yield(); 
boolean StatAvailable;
boolean noStat = false;
long daystatarray[numberOfChannels]; 
time_t begdate = log_filenameToDate(logstat_BegDate);
time_t enddate = log_filenameToDate(logstat_EndDate);
if ( logstat_BegDate != logstat_EndDate ) { StatAvailable = log_getperiodstat(begdate, enddate, daystatarray); }
                                     else { StatAvailable = log_getdaystat(log_ViewDate, daystatarray); }
if ( StatAvailable )
 {
 content += F("<hr>");
 boolean StatisticsAvailable = false;
 for ( byte i = 0; i < numberOfChannels; i++ ) 
  { if ( daystatarray[i] > 0 ) { StatisticsAvailable = true; break; } }
 if ( StatisticsAvailable )
  {
  content += F("<table><tr>");
  content += (Language ? F("<th>Статистика за ") : F("<th>Statistics for "));
  content += logstat_BegDate.substring(6,8) + F(".");
  content += logstat_BegDate.substring(4,6) + F(".");
  content += logstat_BegDate.substring(0,4);
  if ( logstat_BegDate != logstat_EndDate )
   {
   content += F(" - ");
   content += logstat_EndDate.substring(6,8) + F(".");
   content += logstat_EndDate.substring(4,6) + F(".");
   content += logstat_EndDate.substring(0,4);
   }
  content += F(" :&emsp;</th>");
  content += (Language ? F("<th>Канал</th>") : F("<th>Channel</th>"));
  content += (Language ? F("<th>Общее время включения</th>") : F("<th>Total time ON</th>"));
  content += F("</tr>");
  for ( byte i = 0; i < numberOfChannels; i++ ) 
   {
   if ( daystatarray[i] > 0 )
    {  
    content += F("<tr><td align='center'>&emsp;</td>");
    content += F("<td align='right'>");
    content += String(i + 1) + F("</td>");
    content += F("<td align='right'>&emsp;");
    content += (convertMStoDHMS(daystatarray[i] * 1000UL));
    content += F("&emsp;</td></tr>");
    }
   }  
  content += F("</table>");
  }
 else { noStat = true; }
 }
else { noStat = true; }
if ( noStat )
 {
 content += (Language ? F("Статистика за ") : F("Statistics for  "));
 content += ( log_Day < 10 ? F("0") : F("") ) + String(log_Day) + "." + 
            ( log_Month < 10 ? F("0") : F("") ) + String(log_Month) + "." + String(log_Year) + "<b>";
 content += (Language ? F(" : отсутствует или пустая") : F(" : no stats or empty"));
 }
content += F("</b><hr>");
content += F("<form method='get' form action='/setlog_ViewStep'><p>");
content += (Language ? F("Записей на странице") : F("Entries per page"));
content += F(":&emsp;<input name='vs' type='number' min='");
content += String(LOG_VIEWSTEP_MIN);
content += F("' max='");
content += String(LOG_VIEWSTEP_MAX);
content += F("' value='");
content += String(log_ViewStep) + F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить") : F("Save"));
content += F("' /></p></form>");
content += F("<form method='get' form action='/setlog_DaysToKeep'><p>");
content += (Language ? F("Дней хранения") : F("Days to keep"));
content += F(":&emsp;<input name='dk' type='number' min='1' max='");
content += String((littleFStotalBytes / littleFSblockSize) - 4);
content += F("' value='");
content += String(log_DaysToKeep) + F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить и перезагрузить") : F("Save and reboot"));
content += F("' /></p></form>");
#ifdef DEBUG 
 content += F("<form action='/log_deletefile' form method='get' onsubmit='warn();return false;'><input name='logdel' type='submit' value='");
 content += (Language ? F("Удалить журнал и перезагрузить") : F("Delete daylog and reboot"));
 content += F("' /></form>");
#endif 
content += F("</body></html>\r\n");
yield();
server.sendContent(content);
server.sendContent(F("")); // transfer is done
server.client().stop();
}

void drawChannelList()
{
String content = "";  
for ( int chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 content += ( ChannelList[chNum][CHANNEL_ENABLED] ? F("<font color='black'>") : F("<font color='dimgrey'>") );
 content += F("<form method='get' form action='/setchannelparams'><p>");
 if ( numberOfChannels > 9 && (chNum + 1) <  10 ) { content += F("&nbsp;&nbsp;"); }
 content += (ChannelList[chNum][CHANNEL_ENABLED] ? F("<b>") : F(""));
 content += (Language ? F("Канал") : F("Channel"));
 content += F("&nbsp;");
 if ( numberOfChannels > 9 && (chNum + 1) <  10 ) { content += F("&nbsp;&nbsp;"); }
 content += String(chNum + 1) + F("</b>&nbsp;<select name='e");
 if ( chNum < 10 ) { content += F("0"); }
 content += String(chNum) + F("' size='1'><option ");
 if ( ChannelList[chNum][CHANNEL_ENABLED] )
      { content += (Language ? F("selected='selected' value='11'>доступен</option><option value='10'>") : F("selected='selected' value='11'>enabled</option><option value='10'>")); }
 else { content += (Language ? F("value='11'>доступен</option><option selected='selected' value='10'>") : F("value='11'>enabled</option><option selected='selected' value='10'>")); }
 content += (Language ? F("недоступен") : F("disabled"));
 content += F("</option></select>");
 content += F("&nbsp;GPIO&nbsp;<input name='r' type='number' min='0' max='");
 content += String(GPIO_MAX_NUMBER) + F("' value='");
 content += String(ChannelList[chNum][CHANNEL_GPIO]) + F("' /> (") + NodeMCUpins[ChannelList[chNum][CHANNEL_GPIO]] + F(")&nbsp;");
 content += (Language ? F("Управление") : F("Controls"));
 content += F("&nbsp;<select name='i' size='1'><option ");
 if ( ChannelList[chNum][CHANNEL_INVERTED] ) 
      { content += (Language ? F("selected='selected' value='11'>инвертированное</option><option value='10'>") : F("selected='selected' value='11'>inverted</option><option value='10'>")); }
 else { content += (Language ? F("value='11'>инвертированное</option><option selected='selected' value='10'>") : F("value='11'>inverted</option><option selected='selected' value='10'>")); }
 content += (Language ? F("прямое") : F("noninverted"));
 content += F("</option></select>&nbsp;<input type='submit' value='");
 content += (Language ? F("Сохранить' />") : F("Save' />"));
 int duplicateChannelNumber = find_duplicate_or_conflicting_channel(chNum);
 if ( duplicateChannelNumber >= 0 )
  {
  content += F("<font color='red'>&emsp;");  
  if ( duplicateChannelNumber >= 1000 ) { content += (Language ? F("<b>конфликт с ") : F("<b>conflicts with ")); duplicateChannelNumber -= 1000; }
                                   else { content += (Language ? F("<b>дублирует ") : F("<b>duplicates ")); }
  content += String(duplicateChannelNumber + 1) + F(" !</b></font>");
  }
 content += F("</p></form></font>");
 }
content += (Language ? F("&emsp;<i>GPIO может быть от 0") : F("&emsp;<i>GPIO can be from 0"));
content += (Language ? F(" до ") : F(" to "));
content += String(GPIO_MAX_NUMBER);
content += (Language ? F(", исключая 6,7,8 и 11. Использование GPIO 1 нежелательно (конфликт с Serial TX).</i>") 
                     : F(", exclude 6,7,8 and 11. Using GPIO 1 is unwanted (conflict with Serial TX).</i>"));
server.sendContent(content); content = "";
yield();
}

void drawTaskList()
{
String content = "";  
int curChNum = TaskList[0][TASK_CHANNEL];
boolean curTaskEnabled = ( TaskList[0][TASK_ACTION] != ACTION_NOACTION );
for (int taskNum = 0; taskNum < numberOfTasks; taskNum++)
 {
 yield(); 
 if ( numberOfChannels > 1 
  && ( (curChNum != TaskList[taskNum][TASK_CHANNEL] && (TaskList[taskNum][TASK_ACTION] != ACTION_NOACTION)) 
     || curTaskEnabled != (TaskList[taskNum][TASK_ACTION] != ACTION_NOACTION) )
    )
  {
  content += F("<p style='line-height:1%'> <br></p>");
  curChNum = TaskList[taskNum][TASK_CHANNEL];
  curTaskEnabled = (TaskList[taskNum][TASK_ACTION] != ACTION_NOACTION);
  }
 content += ( (TaskList[taskNum][TASK_ACTION] != ACTION_NOACTION) && ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_ENABLED]
            ? F("<font color='black'><b>") : F("<font color='dimgrey'>") );
 content += F("<form method='get' form action='/settasksettings'><p>");
 // Task number 
 content += (Language ? F("Задание") : F("Task"));
 content += F("&nbsp;");
 if ( numberOfTasks >  9 && (TaskList[taskNum][TASK_NUMBER] + 1) <  10 ) { content += F("&nbsp;&nbsp;"); }
 if ( numberOfTasks > 99 && (TaskList[taskNum][TASK_NUMBER] + 1) < 100 ) { content += F("&nbsp;&nbsp;"); }
 content += String(TaskList[taskNum][TASK_NUMBER] + 1) + F("</b>&nbsp;");
 // Channel
 content += (Language ? F("канал") : F("channel"));
 content += F("&nbsp;<select name='c");
 if ( taskNum <  10 ) { content += F("0"); }
 if ( taskNum < 100 ) { content += F("0"); }
 content += String(taskNum) + F("' size='1'><option ");
 for (int t = 0; t <= numberOfChannels - 1; t++)
  {
  if ( TaskList[taskNum][TASK_CHANNEL] == t ) { content += F("selected='selected' "); } 
  content += F("value='"); content += String(t) + F("'>") + String(t + 1) + F("</option>");       
  if ( t < numberOfChannels - 1 ) { content += F("<option "); }
  }
 content += F("</select>&nbsp;");
 // Action
 content += F("&nbsp;<select name='a' size='1'>");
 if ( TaskList[taskNum][TASK_ACTION] == ACTION_NOACTION ) 
  { content += (Language ? F("<option selected='selected' value='0'>нет действия</option><option value='11'>включить</option><option")
                         : F("<option selected='selected' value='0'>no action</option><option value='11'>set ON</option><option")); }
 if ( TaskList[taskNum][TASK_ACTION] == ACTION_TURN_OFF ) 
  { content += (Language ? F("<option value='0'>нет действия</option><option value='11'>включить</option><option selected='selected'")
                         : F("<option value='0'>disabled</option><option value='11'>set ON</option><option selected='selected'")); }
 if ( TaskList[taskNum][TASK_ACTION] == ACTION_TURN_ON )  
  { content += (Language ? F("<option value='0'>нет действия</option><option selected='selected' value='11'>включить</option><option")
                         : F("<option value='0'>disabled</option><option selected='selected' value='11'>set ON</option><option")); }
 content += (Language ? F(" value='10'>выключить</option></select>&nbsp;") : F(" value='10'>set OFF</option></select>&nbsp;"));
 // Hour, Minute, Second
 content += (Language ? F("в") : F("at"));
 content += F("&nbsp;<input name='h' type='number' min='0' max='23' value='");
 content += String(TaskList[taskNum][TASK_HOUR]) + F("' />&nbsp;");
 content += F(":&nbsp;<input name='m' type='number' min='0' max='59' value='");
 content += String(TaskList[taskNum][TASK_MIN]) + F("' />&nbsp;");
 content += F(":&nbsp;<input name='s' type='number' min='0' max='59' value='");
 content += String(TaskList[taskNum][TASK_SEC]) + F("' />&nbsp;");
 // Day(s)
 content += (Language ? F("день(дни)") : F("day(s)"));
 content += F("&nbsp;<select name='d' size='1'><option ");
 for (int t = 0; t <= TASK_DAY_EVERYDAY; t++)
  {
  if ( TaskList[taskNum][TASK_DAY] == t ) { content += F("selected='selected' "); } 
  content += F("value='"); content += String(t) + F("'>") + namesOfDays[Language][t] + F("</option>");       
  if ( t < 9 ) { content += F("<option "); }
  }
 content += F("</select>&nbsp;<input type='submit' value='");
 content += (Language ? F("Сохранить' />") : F("Save' />"));
 if ( TaskList[taskNum][TASK_ACTION] != ACTION_NOACTION && ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_ENABLED] )
  {
  int duplicateTaskNumber = find_duplicate_or_conflicting_task(taskNum);
  if ( duplicateTaskNumber >= 0 )
   {
   content += F("<font color='red'>&nbsp;");  
   if ( duplicateTaskNumber >= 1000 ) { content += (Language ? F("<b>конфликт с ") : F("<b>conflicts with ")); duplicateTaskNumber -= 1000; }
                                 else { content += (Language ? F("<b>дублирует ") : F("<b>duplicates ")); }
   content += String(duplicateTaskNumber + 1) + F(" !</b></font>");
   }
  if ( ActiveNowTasksList[TaskList[taskNum][TASK_CHANNEL]] == taskNum 
   && ( (TaskList[taskNum][TASK_ACTION] == ACTION_TURN_OFF && ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_LASTSTATE] == LASTSTATE_OFF_BY_TASK) 
     || (TaskList[taskNum][TASK_ACTION] == ACTION_TURN_ON && ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK)
      )
     )
   {
   content += F("<font color='darkblue'>&nbsp;");  
   content += (Language ? F("активно") : F("active"));
   }
  if ( NextTasksList[TaskList[taskNum][TASK_CHANNEL]] == taskNum )
   {
   content += F("<font color='DarkCyan'>&nbsp;");  
   content += (Language ? F("следующее") : F("next"));
   }
  } 
 content += F("</p></form></font></b>");
 server.sendContent(content); content = "";
 } // end of for (int taskNum = 0; taskNum < numberOfTasks; taskNum++)
yield(); 
}

void drawWiFiSettings()
{
String content = F("<hr>");
String lip = wifiManuallySetIP.toString();
if ( lip == "" || lip == "(IP unset)" ) { lip = "192.168.0.0"; }
String dns = wifiManuallySetDNS.toString();
if ( dns == "" || dns == "(IP unset)" ) { dns = "192.168.0.0"; }
String gwy = wifiManuallySetGW.toString();
if ( gwy == "" || gwy == "(IP unset)" ) { gwy = "192.168.0.0"; }
String net = wifiManuallySetSUBNET.toString();
if ( net == "" || net == "(IP unset)" ) { net = "255.255.255.0"; }
int APtotalnumber = WiFi.scanNetworks();
yield();
if ( APtotalnumber > 0 )
 {
 struct AP
  {
  String ssid;
  uint8_t enc;
  int32_t rssi;
  uint8_t* bssid;
  int32_t ch;
  bool hidden;
  };
 AP listAP[APtotalnumber];
 AP tmp;
 const uint8_t *myBSSID = WiFi.BSSID();
 uint8_t targetBSSID[6];
 for (byte i = 0; i < 6; i++) { targetBSSID[i] = *myBSSID++; }
 delay(10);
 for ( int i = 0; i < APtotalnumber; ++i )
  { 
  WiFi.getNetworkInfo(i, listAP[i].ssid, listAP[i].enc, listAP[i].rssi, listAP[i].bssid, listAP[i].ch, listAP[i].hidden);
  yield();
  } 
 for (int i = 1; i < APtotalnumber; i++) // sorting on RSSI
  {
  for (int j = i; j > 0 && (listAP[j-1].rssi < listAP[j].rssi); j--) 
   {
   tmp = listAP[j-1];
   listAP[j-1] = listAP[j];
   listAP[j] = tmp;
   yield();
   }
  }
 content += F("<form method='get' action='/wifisetting'><table><tr><td>");
 content += (Language ? F("Выберите точку доступа") : F("Select access point"));
 yield();
 String st = ":</td><td>";
 boolean isMy = false;
 for (int i = 0; i < APtotalnumber; i++)
  {
  isMy = ( listAP[i].bssid[0] == targetBSSID[0] && listAP[i].bssid[1] == targetBSSID[1] 
        && listAP[i].bssid[2] == targetBSSID[2] && listAP[i].bssid[3] == targetBSSID[3]
        && listAP[i].bssid[4] == targetBSSID[4] && listAP[i].bssid[5] == targetBSSID[5] );  
  if ( listAP[i].rssi >= RSSI_THERESHOLD || isMy )
   { 
   if ( i > 0 ) { st += F("</td> <td>"); }
   st += ( isMy ? F("<b>") : F("") );
   st += F("<input name='ssid' type='radio' value='");
   st += String(listAP[i].ssid);
   st += F("'");
   st += ( isMy ? F(" checked") : F("") );
   st += F("> ");
   st += String(listAP[i].ssid);
   st += ( isMy ? F("</b>") : F("") );
   st += F(" (");
   st += String(listAP[i].rssi);
   st += F("db, ");
   st += String(listAP[i].ch);
   st += F("ch");
   st += (listAP[i].enc == ENC_TYPE_NONE ? ", *open" : "");
   st += (listAP[i].enc == ENC_TYPE_WEP ? ", WEP" : "");
   //st += (listAP[i].enc == ENC_TYPE_TKIP ? "WPA/PSK" : "");
   //st += (listAP[i].enc == ENC_TYPE_CCMP ? "WPA2/PSK" : "");
   //st += (listAP[i].enc == ENC_TYPE_AUTO ? "WPA/WPA2/PSK" : "");
   st += F(")</td></tr><tr><td>");
   }
  yield();
  }
 content += st;
 content += F("</td></tr><tr><td>");
 content += (Language ? F("Введите пароль для выбранной точки доступа") : F("Enter password for selected access point"));
 content += F(":</td><td><input maxlength='32' name='pass' size='32' type='text'/>");
 }
yield();
content += F("</td></tr><tr><td>");
content += (Language ? F("Получение IP адресов") : F("Obtaining IP addresses"));
content += F(":</td><td><input name='dhcp' type='radio' value='0'");
content += ( !wifiManuallySetAddresses ? F(" checked> ") : F("> ") );
content += (Language ? F("Автоматически по DHCP") : F("Automatically via DHCP"));
content += F("&emsp;</td></tr><tr><td> </td><td><input name='dhcp' type='radio' value='1'");
content += ( wifiManuallySetAddresses ? F(" checked> ") : F("> ") );
content += (Language ? F("Использовать указанные ниже") : F("Use the following IP addresses"));
content += F("</td></tr><tr><td>");
content += (Language ? F("IP-адрес (") : F("IP address ("));
content += WiFi.localIP().toString() + F("):</td><td><input name='lip' required");
content += F(" type='ip' pattern='^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$' value='");
content += lip + F("' /></td></tr><tr><td>");
content += (Language ? F("Маска подсети (") : F("Subnet mask ("));
content += WiFi.subnetMask().toString() + F("):</td><td><input name='net' required");
content += F(" type='ip' pattern='^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$' value='");
content += net + F("' /></td></tr><tr><td>");
content += (Language ? F("Основной шлюз (") : F("Gateway ("));
content += WiFi.gatewayIP().toString() + F("):</td><td><input name='gwy' required");
content += F(" type='ip' pattern='^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$' value='");
content += gwy + F("' /></td></tr><tr><td>");
content += (Language ? F("DNS-сервер") : F("DNS server ("));
content += WiFi.dnsIP().toString() + F("):</td><td><input name='dns' required");
content += F(" type='ip' pattern='^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$' value='");
content += dns + F("' /></td><td>");
content += F("<input type='submit' value='");
content += (Language ? F("Сохранить и перезагрузить") : F("Save and reboot"));
content += F("'></form></td></tr></table>");
server.sendContent(content); content = "";
yield(); 
WiFi.scanDelete();
}

void drawFilesSettings()
{
if ( !littleFS_OK ) { return; }
if ( ntpcurEpochTime < NTP_MINIMUM_EPOCHTIME ) { return; }
String content = F("<hr>");
server.sendContent(content); content = "";
yield();
int usedBytes = drawFSinfo();
// Save
if ( littleFStotalBytes - usedBytes >= littleFSblockSize )
 {
 content += F("<p><form method='get' form action='/savesettings'>");
 content += (Language ? F("Сохранить все настройки в файл") : F("Save all settings to file"));
 content += F(":&emsp;<input maxlength='30' name='fn' size='30' type='text' pattern='[a-zA-Z0-9-_.]+' required value='");
 ntpTimeInfo = gmtime(&ntpcurEpochTime); 
 content += String(1900 + ntpTimeInfo->tm_year) + F("-")
         + ( ntpTimeInfo->tm_mon + 1 < 10 ? F("0") : F("") ) + String(ntpTimeInfo->tm_mon + 1) + F("-")
         + ( ntpTimeInfo->tm_mday < 10 ? F("0") : F("") ) + String(ntpTimeInfo->tm_mday) + F("-")
         + ( ntpTimeInfo->tm_hour < 10 ? F("0") : F("") ) + String(ntpTimeInfo->tm_hour) + F("-")
         + ( ntpTimeInfo->tm_min < 10 ? F("0") : F("") ) + String(ntpTimeInfo->tm_min) + F(".VTcfg");
 content += F("' />&emsp;<input type='submit' value='");
 content += (Language ? F("Сохранить") : F("Save"));
 content += F("' /></form></p>");
 }
if ( numCFGfiles > 0 )
 {
 Dir dir = LittleFS.openDir("/");
 String arrFileNames[numCFGfiles];
 int fileNum = 0;
 do
  {
  if ( dir.fileName() != "" ) { arrFileNames[fileNum] = dir.fileName(); fileNum++; }
  if ( fileNum >= numCFGfiles ) { break; }
  yield();
  }
 while ( dir.next() ); 
 // Restore
 content += F("<p><form method='get' form action='/restoresettings' onsubmit='warn();return false;'>");
 content += (Language ? F("Восстановить настройки из файла") : F("Restore settings from file"));
 content += F(":&emsp;<select name='file' size='1'>");
 for ( int fileNum = 0; fileNum < numCFGfiles; fileNum++ )
  {
  content += F("<option required value='"); 
  content += arrFileNames[fileNum] + F("'>") + arrFileNames[fileNum] + F("</option>");       
  yield();
  }
 content += F("</select>&emsp;<input type='submit' value='");
 content += (Language ? F("Восстановить (будьте осторожны!) и перезагрузить") : F("Restore (be careful!) and reboot"));
 content += F("' /></form></p>");
 // Rename
 content += F("<p><form method='get' form action='/renamefile'>");
 content += (Language ? F("Переименовать файл") : F("Rename settings file"));
 content += F(":&emsp;<select name='file' size='1'>");
 for ( int fileNum = 0; fileNum < numCFGfiles; fileNum++ )
  {
  content += F("<option required value='"); 
  content += arrFileNames[fileNum] + F("'>") + arrFileNames[fileNum] + F("</option>");       
  yield();
  }
 content += F("</select>&emsp;");
 content += (Language ? F("Новое имя") : F("New name"));
 content += F(":&emsp;<input maxlength='30' name='nn' size='30' type='text' pattern='[a-zA-Z0-9-_.]+' required");
 content += F("/>&emsp;<input type='submit' value='");
 content += (Language ? F("Переименовать") : F("Rename"));
 content += F("' /></form></p>");
 // Delete
 content += F("<p><form method='get' form action='/deletefile' onsubmit='warn();return false;'>");
 content += (Language ? F("Удалить файл с настройками") : F("Delete settings file"));
 content += F(":&emsp;<select name='file' size='1'>");
 for ( int fileNum = 0; fileNum < numCFGfiles; fileNum++ )
  {
  content += F("<option required value='"); 
  content += arrFileNames[fileNum] + F("'>") + arrFileNames[fileNum] + F("</option>");       
  yield();
  }
 content += F("</select>&emsp;<input type='submit' value='");
 content += (Language ? F("Удалить (будьте осторожны!)") : F("Delete (be careful!)"));
 content += F("' /></form></p>");
 // Download
 content += F("<p><form method='post' form action='/downloadfile'>");
 content += (Language ? F("Выгрузить файл с настройками") : F("Download settings file"));
 content += F(":&emsp;<select name='file' size='1'>");
 for ( int fileNum = 0; fileNum < numCFGfiles; fileNum++ )
  {
  content += F("<option required value='"); 
  content += arrFileNames[fileNum] + F("'>") + arrFileNames[fileNum] + F("</option>");       
  yield();
  }
 content += F("</select>&emsp;<input type='submit' value='");
 content += (Language ? F("Выгрузить") : F("Download"));
 content += F("' /></form></p>");
 } // end of if ( numCFGfiles > 0 )
// Upload
if ( littleFStotalBytes - usedBytes >= littleFSblockSize )
 {
 content += F("<p><form method='post' form action='/uploadfile' enctype='multipart/form-data'>");
 content += (Language ? F("Загрузить файл с настройками") : F("Upload settings file"));
 content += F(":&emsp;<input type='file' name='upload'><input type='submit' value='");
 content += (Language ? F("Загрузить") : F("Upload"));
 content += F("' /></form></p>");
 }
server.sendContent(content); content = "";
yield(); 
}

void drawHomePage()
{
String content = "";
boolean isEnabledChannels = false;
for ( int chNum = 0; chNum < numberOfChannels; chNum++ )
 { if ( ChannelList[chNum][CHANNEL_ENABLED] ) { isEnabledChannels = true; break; } }
if ( isEnabledChannels )
 {
 content += F("<table><tr>");
 }
else
 {
 content += F("<br><font color='red'><b>");
 content += (Language ? F("Нет доступных каналов") : F("No channels available"));
 content += F("</b></font><br><br>");
 }
for ( int chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 if ( !ChannelList[chNum][CHANNEL_ENABLED] ) { continue; }
 content += F("<tr><td>");
 content += (Language ? F("Канал <b>") : F("Channel <b>"));
 if ( numberOfChannels > 9 && (chNum + 1) <  10 ) { content += F("&nbsp;&nbsp;"); }
 content += String(chNum + 1);
 content += (Language ? F(":</b>&nbsp;<font color='") : F(":</b>&nbsp;<font color='"));
 String onoff;
 if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_BY_TASK )
  {
  onoff = "on";  
  if ( NumEnabledTasks[chNum] > 0 )
   { content += ( Language ? F("red'><b>ВЫКЛЮЧЕН </font></b></td><td>(по заданиям)") : F("red'><b>OFF </font></b></td><td>(by tasks)") ); }
  else 
   { content += ( Language ? F("red'><b>ВЫКЛЮЧЕН </font></b></td><td>(вручную)") : F("red'><b>OFF </font></b></td><td>(manually)") ); }
  }
 else if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK )
  {
  onoff = "off";  
  if ( NumEnabledTasks[chNum] > 0 )
   { content += ( Language ? F("green'><b>ВКЛЮЧЕН </font></b></td><td>(по заданиям)") : F("green'><b>ON </font></b></td><td>(by tasks)") ); }
  else 
   { content += ( Language ? F("green'><b>ВКЛЮЧЕН </font></b></td><td>(вручную)") : F("green><b>ON </font></b></td><td>(manually)") ); }
  }
 else if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_MANUALLY )
  { onoff = "on"; content += ( Language ? F("red'><b>ВЫКЛЮЧЕН </font></b></td><td>(вручную)") : F("red'><b>OFF </font></b></td><td>(manually)") ); }
 else if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_MANUALLY )
  { onoff = "off"; content += ( Language ? F("green'><b>ВКЛЮЧЕН </font></b></td><td>(вручную)") : F("green'><b>ON </font></b></td><td>(manually)") ); }
 else if ( ChannelList[chNum][CHANNEL_LASTSTATE] >= 50 && ChannelList[chNum][CHANNEL_LASTSTATE] < 150 )
  { onoff = "onuntil"; content += ( Language ? F("red'><b>ВЫКЛЮЧЕН </font></b></td><td>(до след. задания)") : F("red'><b>OFF </font></b></td><td>(until next task)") ); }
 else if ( ChannelList[chNum][CHANNEL_LASTSTATE] >= 150 && ChannelList[chNum][CHANNEL_LASTSTATE] < 250 )
  { onoff = "offuntil"; content += ( Language ? F("green'><b>ВКЛЮЧЕН </font></b></td><td>(до след. задания)") : F("green'><b>ON </font></b></td><td>(until next task)") ); }
 content += F("</b></font></td><td>&emsp;<a href='/setchannelstate");   
 content += onoff + F("?s");   
 if ( chNum < 10 ) { content += F("0"); }
 content += String(chNum) + F("'><input type='button' value='");   
 if ( Language ) 
  { 
  content += ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK 
            || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 150 && ChannelList[chNum][CHANNEL_LASTSTATE] < 250)
            ||  ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_MANUALLY ? F("Выключить") : F("Включить") ); 
  }
 else
  {
  content += ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK 
            || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 150 && ChannelList[chNum][CHANNEL_LASTSTATE] < 250)
            ||  ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_MANUALLY ? F("Set OFF") : F("Turn ON") ); 
  }
 content += F("'></a></td><td><font color='darkblue'>&emsp;");
 if ( NumEnabledTasks[chNum] > 0 )
  {
  content += (Language ? F("заданий: ") : F("tasks: "));
  content += String(NumEnabledTasks[chNum]);
  }
 else { content += (Language ? F("нет заданий") : F("no tasks")); }
 // manually
 if ( NumEnabledTasks[chNum] > 0 
   && ChannelList[chNum][CHANNEL_LASTSTATE] != LASTSTATE_ON_MANUALLY 
   && ChannelList[chNum][CHANNEL_LASTSTATE] != LASTSTATE_OFF_MANUALLY
    )
  {
  content += F("</td><td>&emsp;<a href='/setchannelmanually?a");   
  content += ( chNum < 10 ? F("0"): F("") );
  content += String(chNum) + F("'><input type='button' value='");   
  content += ( Language ? F("Вручную") : F("Manually") ); 
  content += F("'></a>");
  }
 if ( NumEnabledTasks[chNum] > 0 &&
     (ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_MANUALLY 
   || ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_MANUALLY)
    )
  { content += F("</td><td>&emsp;"); }
 // until next task 
 if ( NumEnabledTasks[chNum] > 0 && ChannelList[chNum][CHANNEL_LASTSTATE] < 50 )
  {
  content += F("</td><td>&nbsp;<a href='/setchanneluntil?a");   
  content += ( chNum < 10 ? F("0"): F("") );
  content += String(chNum) + F("'><input type='button' value='");   
  content += ( Language ? F("Вручную до след. задания") : F("Manually until next task") ); 
  content += F("'></a>");
  }
 if ( NumEnabledTasks[chNum] > 0 && ChannelList[chNum][CHANNEL_LASTSTATE] >= 50 )
  {
  content += F("</td><td>&emsp;");   
  }
 // by tasks
 if ( NumEnabledTasks[chNum] > 0 && 
     ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_MANUALLY 
    || ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_MANUALLY
    || ChannelList[chNum][CHANNEL_LASTSTATE] >= 50 )
    )
  {
  content += F("</td><td>&nbsp;<a href='/setchannelbytasks?a");   
  content += ( chNum < 10 ? F("0"): F("") );
  content += String(chNum) + F("'><input type='button' value='");   
  content += ( Language ? F("По заданиям") : F("By tasks") ); 
  content += F("'></a>");
  }
 content += F("</font></td></tr>"); 
 } // end of for ( int chNum = 0; chNum < numberOfChannels; chNum++ )
if ( isEnabledChannels ) { content += F("</tr></table>"); } 
if ( littleFS_OK )
 {
 content += F("<hr>");
 content += F("<center><p><form method='get' form action='/viewlog'><input name='vwl' type='submit' value='&emsp;&emsp;&emsp;");
 content += (Language ? F("Просмотр суточных журналов и статистики") : F("View daily logs and statistics"));
 content += F("&emsp;&emsp;&emsp;' /></form></p></center>");
 }
content += F("<hr>");
// list tasks
if ( numberOfTasks <= 5 ) { TaskListCollapsed = false; }
else
 {
 content += F("<form method='get' form action='/settasklistcollapsed'><input name='stlc' type='submit' value='");
 if ( !TaskListCollapsed ) { content += (Language ? F("Свернуть список заданий") : F("Collapse task list")); }
                      else { content += (Language ? F("Развернуть список заданий") : F("Expand task list")); }
 content += F("' /></form>");
 }
server.sendContent(content); content = "";
if ( !TaskListCollapsed ) { drawTaskList(); }
content += F("<hr>");
// list channels
if ( numberOfChannels <= 5 ) { ChannelListCollapsed = false; }
else
 {
 content += F("<form method='get' form action='/setchannellistcollapsed'><input name='stlc' type='submit' value='");
 if ( !ChannelListCollapsed ) { content += (Language ? F("Свернуть список каналов") : F("Collapse channel list")); }
                         else { content += (Language ? F("Развернуть список каналов") : F("Expand channel list")); }
 content += F("' /></form>");
 }
server.sendContent(content); content = "";
if ( !ChannelListCollapsed ) { drawChannelList(); }
content += F("<hr>");
// Settings
content += F("<form method='get' form action='/setnumberOfTasks'><p>");
content += (Language ? F("Количество заданий (") : F("Number of tasks ("));
content += String(TASKLIST_MIN_NUMBER) + F("...") + String(TASKLIST_MAX_NUMBER) 
        + F("):&emsp;<input name='nt' type='number' min='") + String(TASKLIST_MIN_NUMBER) + F("' max='") + String(TASKLIST_MAX_NUMBER) + F("' value='");
content += String(numberOfTasks) + F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить и перезагрузить") : F("Save and reboot"));
content += F("' /></p></form>");
content += F("<form method='get' form action='/setnumberOfChannels'><p>");
content += (Language ? F("Количество каналов (") : F("Number of channels ("));
content += String(CHANNELLIST_MIN_NUMBER) + F("...") + String(CHANNELLIST_MAX_NUMBER) 
        + F("):&emsp;<input name='nc' type='number' min='") + String(CHANNELLIST_MIN_NUMBER) + F("' max='") + String(CHANNELLIST_MAX_NUMBER) + F("' value='");
content += String(numberOfChannels) + F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить и перезагрузить") : F("Save and reboot"));
content += F("' /></p></form><form method='get' form action='/setlanguage'><p>");
content += (Language ? F("Язык интерфейса") : F("Interface language"));
content += F(":&emsp;<select name='lang' size='1'><option ");
content += ( Language ? F("selected='selected' value='11'>Русский</option><option value='10'>English</option></select>")
                      : F("value='11'>Русский</option><option selected='selected' value='10'>English</option></select>"));
content += F("&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить") : F("Save"));
content += F("' /></p></form><form method='get' form action='/setntpTimeZone'><p>");
content += (Language ? F("Часовой пояс") : F("Time Zone"));
content += F(" (-12...12):&emsp;<input name='tz' type='number' min='-11' max='12' value='");
content += String(ntpTimeZone) + F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить и перезагрузить") : F("Save and reboot"));
content += F("' /></p></form>");
content += F("<form method='get' form action='/setntpdaylightsave'><p>");
content += (Language ? F("Автопереход на летнее-зимнее время") : F("Auto daylight saving time"));
content += F(":&emsp;<select name='dls' size='1'><option ");
if ( ntpAutoDaylightSavingEnabled ) { content += (Language ? F("selected='selected' value='11'>включен</option><option value='10'>") 
                                              : F("selected='selected' value='11'>enabled</option><option value='10'>")); }
                  else { content += (Language ? F("value='11'>включен</option><option selected='selected' value='10'>") 
                                              : F("value='11'>enabled</option><option selected='selected' value='10'>")); }
content += (Language ? F("отключен") : F("disabled"));
content += F("</option></select>&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить") : F("Save"));
content += F("' />&emsp;<font color='darkblue'>");
if ( ntpAutoDaylightSavingEnabled )
 { if ( isSummerTimeNow() ) { content += (Language ? F("Сейчас активно летнее время") : F("Daylight saving active now")); }
                       else { content += (Language ? F("Сейчас активно зимнее время") : F("Daylight saving inactive now")); } }
content += F("</font></p></form><form method='get' form action='/setntpdaylightsavezone'><p>");
content += (Language ? F("Зона перехода на летнее-зимнее время") : F("Daylight saving time zone"));
content += F(":&emsp;<select name='dlsz' size='1'><option ");
if ( ntpDaylightSaveZone ) { content += (Language ? F("selected='selected' value='11'>США</option><option value='10'>") 
                                                  : F("selected='selected' value='11'>USA</option><option value='10'>")); }
                      else { content += (Language ? F("value='11'>США</option><option selected='selected' value='10'>") 
                                                  : F("value='11'>USA</option><option selected='selected' value='10'>")); }
content += (Language ? F("Европа") : F("Europe"));
content += F("</option></select>&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить") : F("Save"));
content += F("' /><br><i>");
content += (Language ? F("Европа в 4:00 в последнее воскресенье октября и в 3:00 в последнее воскресенье марта; ")
                     : F("Europe at 4:00 am on the last sunday in October and at 3:00 am on the last sunday in March; "));
content += (Language ? F("США в 2:00 во второе воскресенье марта и в 2:00 в первое воскресенье ноября.")
                     : F("USA at 2:00 am on the second sunday of March and at 2:00 am on the first sunday of November."));
content += F("</i></p></form>");
content += F("<form method='get' form action='/setntpservername1'><p>");
content += (Language ? F("Сервер синхронизации времени 1") : F("Time synchronization server 1"));
content += F(":&emsp;<input maxlength='32' name='sn' required size='32' type='text' value='");
content += ntpServerName1;
content += F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить") : F("Save"));
content += F("' /></p></form>");
content += F("<form method='get' form action='/setntpservername2'><p>");
content += (Language ? F("Сервер синхронизации времени 2") : F("Time synchronization server 2"));
content += F(":&emsp;<input maxlength='32' name='sn' required size='32' type='text' value='");
content += ntpServerName2;
content += F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить") : F("Save"));
content += F("' /></p></form>");
content += F("<form method='get' form action='/setlogin'><p>");
content += (Language ? F("Имя авторизации") : F("Login name"));
content += F(":&emsp;<input maxlength='10' name='ln' required size='10' type='text' value='");
content += loginName + F("' /></p><p>");
content += (Language ? F("Новый пароль") : F("New password"));
content += F(":&emsp;<input maxlength='12' name='np' required size='12' type='password' />&emsp;");
content += (Language ? F("Подтвердите пароль") : F("Confirm password"));
content += F(":&emsp;<input maxlength='12' name='npc' required size='12' type='password' />&emsp;");
content += (Language ? F("Старый пароль") : F("Old password"));
content += F(":&emsp;<input maxlength='12' name='op' required size='12' type='password'");
content += F(" />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить") : F("Save"));
content += F("' /></p></form>");
server.sendContent(content); content = "";
// Settings WiFi
drawWiFiSettings();
// Settings files
drawFilesSettings();
content += F("<hr>");
// Actions
content += F("<form action='/firmware' form method='get'><input name='fwu' type='submit' value='");
content += (Language ? F("Обновление прошивки") : F("Firmware update"));
content += F("' /></form><form action='/cleartasklist' form method='get' onsubmit='warn();return false;'><input name='ctl' type='submit' value='");
content += (Language ? F("Очистить и отключить все задания (будьте осторожны!)") : F("Clear and disable all tasks (be careful!)"));
content += F("' /></form><form action='/format' form method='get' onsubmit='warn();return false;'><input name='fmt' type='submit' value='");
content += (Language ? F("Удалить все журналы и файлы настроек (будьте осторожны!)") : F("Delete all logs and settings files (be careful!)"));
content += F("' /></form><form action='/reset' form method='get' onsubmit='warn();return false;'><input name='rst' type='submit' value='");
content += (Language ? F("Сброс всех настроек (будьте осторожны!)") : F("Reset to defaults (be careful!)"));
content += F("' /></form>");
content += F("<form action='/restartapmode' form method='get' onsubmit='warn();return false;'><input name='rstrtap' type='submit' value='");
content += (Language ? F("Перезагрузка в режим точки доступа") : F("Reboot to access point mode"));
content += F("' />");
content += F("&emsp;SSID: ");
content += APMODE_SSID;
content += F("&emsp;PASS: ");
content += APMODE_PASSWORD;
content += F("&emsp;URL: http://192.168.4.1");
content += F("<br><i>");
content += (Language ? F("При загрузке в рабочем режиме, в случае невозможности подключиться к WiFi в течение десяти минут, устройство перезагружается в режим точки доступа. ")
                     : F("When booting in working mode, if it is not possible to connect to WiFi within ten minutes, the device will reboot into access point mode. "));
content += (Language ? F("В режиме точки доступа, в случае отсутствия подключений к http://192.168.4.1, через десять минут устройство перезагружается в рабочий режим.")
                     : F("In the access point mode, if there are no connections to http://192.168.4.1, after ten minutes the device will reboot into working mode."));
content += F("</i></form>");
content += F("<form action='/restart' form method='get' onsubmit='warn();return false;'><input name='rstrt' type='submit' value='");
content += (Language ? F("Перезагрузка") : F("Reboot"));
content += F("' /></form></body></html>\r\n");
yield();
server.sendContent(content);
server.sendContent(F("")); // transfer is done
server.client().stop();
}

bool working_day_of_week(int findDOW) { return (findDOW >= 1 && findDOW <= 5); }

bool weekend_day_of_week(int findDOW) { return (findDOW == 0 || findDOW == 6); }

void EEPROMWriteInt(int p_address, int p_value)
{
if ( EEPROMReadInt(p_address) == p_value ) { return; }  
byte lowByte = ((p_value >> 0) & 0xFF);
byte highByte = ((p_value >> 8) & 0xFF);
EEPROM.write(p_address, lowByte); EEPROM.write(p_address + 1, highByte); EEPROM.commit(); 
}

int EEPROMReadInt(int p_address)
{
byte lowByte = EEPROM.read(p_address);
byte highByte = EEPROM.read(p_address + 1);
if ( (((lowByte << 0) & 0xFF) == 0xFF) && (((highByte << 0) & 0xFF) == 0xFF) ) { return 0; } else { return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00); }
}

void read_settings_from_EEPROM()
{
ChannelListCollapsed = EEPROM.read(CHANNELLISTCOLLAPSED_EEPROM_ADDRESS); 
if ( ChannelListCollapsed != 0 && ChannelListCollapsed != 1 ) { ChannelListCollapsed = 0; } 
TaskListCollapsed = EEPROM.read(TASKLISTCOLLAPSED_EEPROM_ADDRESS); 
if ( TaskListCollapsed != 0 && TaskListCollapsed != 1 ) { TaskListCollapsed = 0; }
Language = EEPROM.read(LANGUAGE_EEPROM_ADDRESS);
if ( Language != 0 && Language != 1 ) { Language = 0; }
log_DaysToKeep = EEPROM.read(LOG_DAYSTOKEEP_EEPROM_ADDRESS);
if ( log_DaysToKeep > (littleFStotalBytes / littleFSblockSize) || log_DaysToKeep < 1 ) { log_DaysToKeep = 3; }
log_ViewStep = EEPROM.read(LOG_VIEWSTEP_EEPROM_ADDRESS);
if ( log_ViewStep > LOG_VIEWSTEP_MAX || log_ViewStep < LOG_VIEWSTEP_MIN ) { log_ViewStep = LOG_VIEWSTEP_DEF; }
ntpTimeZone = EEPROM.read(NTP_TIME_ZONE_EEPROM_ADDRESS);
ntpTimeZone = ntpTimeZone - 12;
if ( ntpTimeZone > 24 ) { ntpTimeZone = NTP_DEFAULT_TIME_ZONE; }
ntpDaylightSaveZone = EEPROM.read(NTP_DAYLIGHTSAVEZONE_EEPROM_ADDRESS);
if ( ntpDaylightSaveZone != 0 && ntpDaylightSaveZone != 1 ) { ntpDaylightSaveZone = 0; } 
ntpAutoDaylightSavingEnabled = EEPROM.read(NTP_DAYLIGHT_SAVING_ENABLED_ADDRESS);
if ( ntpAutoDaylightSavingEnabled != 0 && ntpAutoDaylightSavingEnabled != 1 ) { ntpAutoDaylightSavingEnabled = 1; } 
wifiManuallySetAddresses = EEPROM.read(WIFI_MANUALLY_SET_EEPROM_ADDRESS);
if ( wifiManuallySetAddresses != 0 && wifiManuallySetAddresses != 1 ) { wifiManuallySetAddresses = 0; } 
numberOfTasks = EEPROM.read(NUMBER_OF_TASKS_EEPROM_ADDRESS);
if ( numberOfTasks < TASKLIST_MIN_NUMBER || numberOfTasks > TASKLIST_MAX_NUMBER ) { numberOfTasks = 5; }
TaskList = new uint8_t*[numberOfTasks];
for ( int taskNum = 0; taskNum < numberOfTasks; taskNum++ ) 
 { yield(); TaskList[taskNum] = new uint8_t[TASK_NUM_ELEMENTS]; }
numberOfChannels = EEPROM.read(NUMBER_OF_CHANNELS_EEPROM_ADDRESS);
if ( numberOfChannels < CHANNELLIST_MIN_NUMBER || numberOfChannels > CHANNELLIST_MAX_NUMBER ) { numberOfChannels = CHANNELLIST_MIN_NUMBER; }
}

void read_and_set_ntpservers_from_EEPROM()
{
ntpServerName1 = "";
ntpServerName2 = "";
uint8_t b;
for ( int i = NTP_TIME_SERVER1_EEPROM_ADDRESS; i < NTP_TIME_SERVER1_EEPROM_ADDRESS + 32; ++i) 
 { b = EEPROM.read(i); if ( b > 0 && b < 255 ) { ntpServerName1 += char(b); } }
if ( ntpServerName1 == "" ) { ntpServerName1 = NTP_SERVER_NAME; }
for ( int i = NTP_TIME_SERVER2_EEPROM_ADDRESS; i < NTP_TIME_SERVER2_EEPROM_ADDRESS + 32; ++i) 
 { b = EEPROM.read(i); if ( b > 0 && b < 255 ) { ntpServerName2 += char(b); } }
if ( ntpServerName2 == "" ) { ntpServerName2 = NTP_SERVER_NAME; }
configTime("GMT0",ntpServerName1.c_str(),ntpServerName2.c_str(),NTP_SERVER_NAME);
ntpGetTime();
}

void read_login_pass_from_EEPROM()
{
loginName = "";
loginPass = "";
uint8_t b;
for ( int i = LOGIN_NAME_EEPROM_ADDRESS; i < LOGIN_NAME_EEPROM_ADDRESS + 10; ++i) 
 { b = EEPROM.read(i); if ( b > 0 && b < 255 ) { loginName += char(b); } }
for ( int i = LOGIN_PASS_EEPROM_ADDRESS; i < LOGIN_PASS_EEPROM_ADDRESS + 12; ++i) 
 { b = EEPROM.read(i); if ( b > 0 && b < 255 ) { loginPass += char(b); } }
if ( loginName == "" || loginPass == "" ) { loginName = DEFAULT_LOGIN_NAME; loginPass = DEFAULT_LOGIN_PASSWORD; }
httpUpdater.setup(&server, "/firmware", loginName, loginPass);
}

void read_AP_name_pass_from_EEPROM()
{
AP_name = "";
AP_pass = "";
uint8_t b;
for ( int i = AP_SSID_EEPROM_ADDRESS; i < AP_SSID_EEPROM_ADDRESS + 63; ++i) 
 { b = EEPROM.read(i); if ( b > 0 && b < 255 ) { AP_name += char(b); } }
for ( int i = AP_PASS_EEPROM_ADDRESS; i < AP_PASS_EEPROM_ADDRESS + 32; ++i) 
 { b = EEPROM.read(i); if ( b > 0 && b < 255 ) { AP_pass += char(b); } }
if ( AP_name == "" || AP_pass == "" )
 {
 AP_name = AP_SSID; 
 AP_pass = AP_PASS;
 }
}

void save_AP_name_pass_to_EEPROM()
{
for ( int i = AP_SSID_EEPROM_ADDRESS; i < AP_SSID_EEPROM_ADDRESS + 63; ++i) { EEPROM.write(i, 0); } // fill with 0
for ( int i = AP_PASS_EEPROM_ADDRESS; i < AP_PASS_EEPROM_ADDRESS + 32; ++i) { EEPROM.write(i, 0); } // fill with 0
for ( unsigned int i = 0; i < AP_name.length(); ++i ) { EEPROM.write(i + AP_SSID_EEPROM_ADDRESS, AP_name[i]); }
for ( unsigned int i = 0; i < AP_pass.length(); ++i ) { EEPROM.write(i + AP_PASS_EEPROM_ADDRESS, AP_pass[i]); }
EEPROM.commit();
read_AP_name_pass_from_EEPROM();
}

int find_duplicate_or_conflicting_task(int findNUM)
{
int found = -1;
for ( int taskNum = 0; taskNum < numberOfTasks - 1; taskNum++ )
 {
 yield(); 
 if ( taskNum != findNUM 
   && ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_ENABLED]
   && TaskList[taskNum][TASK_ACTION]  != ACTION_NOACTION
   && TaskList[taskNum][TASK_HOUR]    == TaskList[findNUM][TASK_HOUR] 
   && TaskList[taskNum][TASK_MIN]     == TaskList[findNUM][TASK_MIN]
   && TaskList[taskNum][TASK_SEC]     == TaskList[findNUM][TASK_SEC]
   && TaskList[taskNum][TASK_CHANNEL] == TaskList[findNUM][TASK_CHANNEL]
    )
  {
  if ( TaskList[taskNum][TASK_DAY] == TaskList[findNUM][TASK_DAY]
   || (TaskList[taskNum][TASK_DAY] == TASK_DAY_WORKDAYS && working_day_of_week(TaskList[findNUM][TASK_DAY]))
   || (TaskList[taskNum][TASK_DAY] == TASK_DAY_WEEKENDS && weekend_day_of_week(TaskList[findNUM][TASK_DAY]))
   ||  TaskList[taskNum][TASK_DAY] == TASK_DAY_EVERYDAY )
   {
   found = ( TaskList[taskNum][TASK_ACTION] == TaskList[findNUM][TASK_ACTION] ? TaskList[taskNum][TASK_NUMBER] // duplicate
                                                                              : TaskList[taskNum][TASK_NUMBER] + 1000 ); // conflicting
   break; 
   }
  }
 }
return found;
}

int find_duplicate_or_conflicting_channel(int findNUM)
{
int found = -1;
for ( int chNum = 0; chNum < numberOfChannels - 1; chNum++ )
 {
 yield(); 
 if ( chNum != findNUM
   && ChannelList[findNUM][CHANNEL_ENABLED] 
   && ChannelList[chNum][CHANNEL_ENABLED] 
   && ChannelList[chNum][CHANNEL_GPIO] == ChannelList[findNUM][CHANNEL_GPIO] )
  {
  found = ( ChannelList[chNum][CHANNEL_INVERTED] == ChannelList[findNUM][CHANNEL_INVERTED] ? chNum           // duplicate
                                                                                           : chNum + 1000 ); // conflicting
  break; 
  }
 }
return found;
}

byte find_next_tasks_within_day(int findChannel, int curDOW, int findDOW, int hh, int mm, int ss)
{
int taskNum;  
byte found = TASKLIST_MAX_NUMBER;
if ( findDOW != curDOW ) { hh = 0; mm = 0; ss = 0; }
for ( taskNum = 0; taskNum <= numberOfTasks - 1; taskNum++ )
 {
 yield(); 
 if ( TaskList[taskNum][TASK_CHANNEL] == findChannel 
   && ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_ENABLED]
   && TaskList[taskNum][TASK_ACTION] != ACTION_NOACTION
   && (
       TaskList[taskNum][TASK_HOUR] > hh 
   || (TaskList[taskNum][TASK_HOUR] == hh && TaskList[taskNum][TASK_MIN] > mm)
   || (TaskList[taskNum][TASK_HOUR] == hh && TaskList[taskNum][TASK_MIN] == mm && TaskList[taskNum][TASK_SEC] >= ss)
      )
    )
  {
  if ( (TaskList[taskNum][TASK_DAY] == findDOW)
    || (TaskList[taskNum][TASK_DAY] == TASK_DAY_WORKDAYS && working_day_of_week(findDOW))
    || (TaskList[taskNum][TASK_DAY] == TASK_DAY_WEEKENDS && weekend_day_of_week(findDOW))
    || (TaskList[taskNum][TASK_DAY] == TASK_DAY_EVERYDAY)
     )
   { found = taskNum; break; }
  }
 }
return found;
}

void find_next_tasks()  
{
if ( !ntpTimeIsSynked ) { return; }  
if ( ntpcurEpochTime < NTP_MINIMUM_EPOCHTIME ) { return; }
if ( !thereAreEnabledTasks ) { return; }  
int chNum, findDOW;
byte foundTask, DaysCounter;
for ( chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 if ( NumEnabledTasks[chNum] == 0 ) { continue; }
 foundTask = TASKLIST_MAX_NUMBER;  
 findDOW = ntpTimeInfo->tm_wday;
 DaysCounter = 0;
 while ( DaysCounter < 7 ) 
  {
  foundTask = find_next_tasks_within_day(chNum, ntpTimeInfo->tm_wday, findDOW, ntpTimeInfo->tm_hour, ntpTimeInfo->tm_min, ntpTimeInfo->tm_sec);
  if ( foundTask < TASKLIST_MAX_NUMBER ) { break; }
  DaysCounter++;
  if ( findDOW == 6 ) { findDOW = 0; } else { findDOW++; }
  }
 NextTasksList[chNum] = ( foundTask < TASKLIST_MAX_NUMBER ? foundTask : 255 ); 
 }
}

byte check_previous_tasks_within_day(int findChannel, int curDOW, int findDOW, int hh, int mm, int ss)
{
int taskNum;  
byte found = TASKLIST_MAX_NUMBER;
if ( findDOW != curDOW ) { hh = 23; mm = 59; ss = 95; }
for ( taskNum = numberOfTasks - 1; taskNum >= 0; taskNum-- )
 {
 yield(); 
 if ( TaskList[taskNum][TASK_CHANNEL] == findChannel 
   && ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_ENABLED]
   && TaskList[taskNum][TASK_ACTION] != ACTION_NOACTION
   && (
       TaskList[taskNum][TASK_HOUR] < hh 
   || (TaskList[taskNum][TASK_HOUR] == hh && TaskList[taskNum][TASK_MIN] < mm)
   || (TaskList[taskNum][TASK_HOUR] == hh && TaskList[taskNum][TASK_MIN] == mm && TaskList[taskNum][TASK_SEC] <= ss)
      )
    )
  {
  if ( (TaskList[taskNum][TASK_DAY] == findDOW)
    || (TaskList[taskNum][TASK_DAY] == TASK_DAY_WORKDAYS && working_day_of_week(findDOW))
    || (TaskList[taskNum][TASK_DAY] == TASK_DAY_WEEKENDS && weekend_day_of_week(findDOW))
    || (TaskList[taskNum][TASK_DAY] == TASK_DAY_EVERYDAY)
     )
   { found = taskNum; break; }
  }
 }
return found;
}

void check_previous_tasks()  
{
if ( !ntpTimeIsSynked ) { return; }  
if ( ntpcurEpochTime < NTP_MINIMUM_EPOCHTIME ) { return; }
if ( !thereAreEnabledTasks ) { return; }  
bool needSave = false;  
int chNum, findDOW;
byte foundTask, DaysCounter;
for ( chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 if ( NumEnabledTasks[chNum] == 0 ) 
  {
       if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_BY_TASK
        || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 50 && ChannelList[chNum][CHANNEL_LASTSTATE] < 150)
          )
   {
   ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_OFF_MANUALLY; 
   needSave = true;
   }  
  else if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK
        || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 150 && ChannelList[chNum][CHANNEL_LASTSTATE] < 250)
          )
   {
   ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_ON_MANUALLY; 
   needSave = true;
   }  
  continue;
  }
 findDOW = ntpTimeInfo->tm_wday;
 foundTask = TASKLIST_MAX_NUMBER;  
 DaysCounter = 0;
 while ( DaysCounter < 7 ) 
  {
  foundTask = check_previous_tasks_within_day(chNum, ntpTimeInfo->tm_wday, findDOW, ntpTimeInfo->tm_hour, ntpTimeInfo->tm_min, ntpTimeInfo->tm_sec);
  if ( foundTask < TASKLIST_MAX_NUMBER ) { break; }
  DaysCounter++;
  if ( findDOW == 0 ) { findDOW = 6; } else { findDOW--; }
  }
 ActiveNowTasksList[chNum] = ( foundTask < TASKLIST_MAX_NUMBER ? foundTask : 255 ); 
 if ( foundTask < TASKLIST_MAX_NUMBER )
  {
  if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_BY_TASK && TaskList[foundTask][TASK_ACTION] == ACTION_TURN_ON ) 
   {      
   ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_ON_BY_TASK;
   log_Append(LOG_EVENT_TASK_SWITCHING_BY_TASK, (uint8_t)chNum, TaskList[foundTask][TASK_NUMBER], (uint8_t)ChannelList[chNum][CHANNEL_LASTSTATE]);
   needSave = true;
   }
  else if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK && TaskList[foundTask][TASK_ACTION] == ACTION_TURN_OFF )    
   {   
   ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_OFF_BY_TASK;
   log_Append(LOG_EVENT_TASK_SWITCHING_BY_TASK, (uint8_t)chNum, TaskList[foundTask][TASK_NUMBER], (uint8_t)ChannelList[chNum][CHANNEL_LASTSTATE]);
   needSave = true;
   }
  else if ( (ChannelList[chNum][CHANNEL_LASTSTATE] >= 50 && ChannelList[chNum][CHANNEL_LASTSTATE] < 150
           && ActiveNowTasksList[chNum] != ChannelList[chNum][CHANNEL_LASTSTATE] - 50)
         || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 150 && ChannelList[chNum][CHANNEL_LASTSTATE] < 250
           && ActiveNowTasksList[chNum] != ChannelList[chNum][CHANNEL_LASTSTATE] - 150)
          ) 
   {
   if ( calcOldChMode(chNum) != LOG_EVENT_CHANNEL_BY_TASK ) { log_Append(calcOldChMode(chNum), chNum, 0, LOG_EVENT_CHANNEL_BY_TASK); }
        if ( TaskList[foundTask][TASK_ACTION] == ACTION_TURN_ON )  { ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_ON_BY_TASK; }
   else if ( TaskList[foundTask][TASK_ACTION] == ACTION_TURN_OFF ) { ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_OFF_BY_TASK; }
   log_Append(LOG_EVENT_TASK_SWITCHING_BY_TASK, (uint8_t)chNum, TaskList[foundTask][TASK_NUMBER], (uint8_t)ChannelList[chNum][CHANNEL_LASTSTATE]);
   needSave = true;
   }
  } 
 }
if ( needSave ) 
 {
 save_channellist_to_EEPROM();
 read_channellist_from_EEPROM_and_switch_channels();
 }   
}

void read_and_sort_tasklist_from_EEPROM()
{
int taskNum, task_element_num, taskAddress;
uint8_t TaskListRAW[numberOfTasks][TASK_NUM_ELEMENTS];
uint8_t IndexArray[numberOfTasks];
#ifdef DEBUG 
 Serial.println(F("Reading tasks"));
#endif
for ( taskNum = 0; taskNum < numberOfTasks; taskNum++ )
 {
 taskAddress = TASKLIST_EEPROM_ADDRESS + taskNum * (TASK_NUM_ELEMENTS - 1);
 yield(); 
 for ( task_element_num = 0; task_element_num < TASK_NUM_ELEMENTS - 1; task_element_num++ ) 
  {
  TaskListRAW[taskNum][task_element_num] = EEPROM.read(taskAddress + task_element_num);
  }
 TaskListRAW[taskNum][TASK_NUMBER] = taskNum; // Not written to EEPROM !
 if ( TaskListRAW[taskNum][TASK_ACTION] != ACTION_NOACTION && TaskListRAW[taskNum][TASK_ACTION] != ACTION_TURN_OFF
   && TaskListRAW[taskNum][TASK_ACTION] != ACTION_TURN_ON )   { TaskListRAW[taskNum][TASK_ACTION] = ACTION_NOACTION; }
 if ( TaskListRAW[taskNum][TASK_HOUR] > 23 )                  { TaskListRAW[taskNum][TASK_HOUR] = 0; }
 if ( TaskListRAW[taskNum][TASK_MIN] > 59 )                   { TaskListRAW[taskNum][TASK_MIN] = 0; }
 if ( TaskListRAW[taskNum][TASK_SEC] > 59 )                   { TaskListRAW[taskNum][TASK_SEC] = 0; }
 if ( TaskListRAW[taskNum][TASK_DAY] > TASK_DAY_EVERYDAY )    { TaskListRAW[taskNum][TASK_DAY] = TASK_DAY_EVERYDAY; }
 if ( TaskListRAW[taskNum][TASK_CHANNEL] > numberOfChannels ) { TaskListRAW[taskNum][TASK_CHANNEL] = 0; }
 }
// Sorting in enabled action, channel and ascending order of time
for ( taskNum = 0; taskNum < numberOfTasks; taskNum++ ) { yield(); IndexArray[taskNum] = taskNum; } 
for ( taskNum = 0; taskNum < numberOfTasks - 1; taskNum++ )
 {
 yield(); 
 for ( int bubble_num = 0; bubble_num < numberOfTasks - taskNum - 1; bubble_num++ )
  {
  yield(); 
  if ((( TaskListRAW[IndexArray[bubble_num]][TASK_ACTION] != ACTION_NOACTION ? 0 : 1 + 86401 * CHANNELLIST_MAX_NUMBER)
       + TaskListRAW[IndexArray[bubble_num]][TASK_CHANNEL] * 86401
       + TaskListRAW[IndexArray[bubble_num]][TASK_HOUR] * 3600
       + TaskListRAW[IndexArray[bubble_num]][TASK_MIN] * 60 
       + TaskListRAW[IndexArray[bubble_num]][TASK_SEC])
    > (( TaskListRAW[IndexArray[bubble_num + 1]][TASK_ACTION] != ACTION_NOACTION ? 0 : 1 + 86401 * CHANNELLIST_MAX_NUMBER)
       + TaskListRAW[IndexArray[bubble_num + 1]][TASK_CHANNEL] * 86401
       + TaskListRAW[IndexArray[bubble_num + 1]][TASK_HOUR] * 3600 
       + TaskListRAW[IndexArray[bubble_num + 1]][TASK_MIN] * 60 
       + TaskListRAW[IndexArray[bubble_num + 1]][TASK_SEC]))  
   { 
   IndexArray[bubble_num] ^= IndexArray[bubble_num + 1];
   IndexArray[bubble_num + 1] ^= IndexArray[bubble_num];
   IndexArray[bubble_num] ^= IndexArray[bubble_num + 1];
   }
  }
 }
#ifdef DEBUG 
 Serial.println(F("Sorting tasks"));
#endif
for ( taskNum = 0; taskNum < numberOfTasks; taskNum++ ) 
 { 
 yield();
 for ( task_element_num = 0; task_element_num < TASK_NUM_ELEMENTS; task_element_num++ )
  { TaskList[taskNum][task_element_num] = TaskListRAW[IndexArray[taskNum]][task_element_num]; }
 }
//
thereAreEnabledTasks = false; 
for ( int chNum = 0; chNum < numberOfChannels; chNum++ ) { NumEnabledTasks[chNum] = 0; }
for ( int taskNum = 0; taskNum < numberOfTasks; taskNum++ )
 { 
 yield();
 if ( ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_ENABLED] && TaskList[taskNum][TASK_ACTION] != ACTION_NOACTION )
  {
  NumEnabledTasks[TaskList[taskNum][TASK_CHANNEL]]++;
  thereAreEnabledTasks = true;
  }
 }
}

void save_tasklist_to_EEPROM()
{
int taskAddress;
#ifdef DEBUG 
 Serial.println(F("Saving tasks"));
#endif
for ( int taskNum = 0; taskNum < numberOfTasks; taskNum++ )
 {
 taskAddress = TASKLIST_EEPROM_ADDRESS + TaskList[taskNum][TASK_NUMBER] * (TASK_NUM_ELEMENTS - 1);
 yield(); 
 for ( int task_element_num = 0; task_element_num < TASK_NUM_ELEMENTS - 1; task_element_num++ )
  {
  if ( TaskList[taskNum][task_element_num] != EEPROM.read(taskAddress + task_element_num) )
   { EEPROM.write(taskAddress + task_element_num, TaskList[taskNum][task_element_num]); }
  }
 }
EEPROM.commit();
read_and_sort_tasklist_from_EEPROM();
}

void read_channellist_from_EEPROM_and_switch_channels()
{
int chNum, ch_element_num;
#ifdef DEBUG 
 Serial.println(F("Reading and switching channels"));
#endif
for ( chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 for ( ch_element_num = 0; ch_element_num < CHANNEL_NUM_ELEMENTS; ch_element_num++ ) 
  { ChannelList[chNum][ch_element_num] = EEPROM.read(CHANNELLIST_EEPROM_ADDRESS + ch_element_num + chNum * CHANNEL_NUM_ELEMENTS); }
 if ( ChannelList[chNum][CHANNEL_GPIO] > GPIO_MAX_NUMBER || NodeMCUpins[ChannelList[chNum][CHANNEL_GPIO]] == "N/A" ) 
  { ChannelList[chNum][CHANNEL_GPIO] = LED_BUILTIN; }
 if ( ChannelList[chNum][CHANNEL_INVERTED] != 0 && ChannelList[chNum][CHANNEL_INVERTED] != 1 )  
  { ChannelList[chNum][CHANNEL_INVERTED] = 0; }
 if ( ChannelList[chNum][CHANNEL_ENABLED] != 0 && ChannelList[chNum][CHANNEL_ENABLED] != 1 ) 
  { ChannelList[chNum][CHANNEL_ENABLED] = 0; }
 if ( ChannelList[chNum][CHANNEL_ENABLED] )
  {
  pinMode(ChannelList[chNum][CHANNEL_GPIO], OUTPUT);
       if (  ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_BY_TASK 
         ||  ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_MANUALLY 
         || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 50 && ChannelList[chNum][CHANNEL_LASTSTATE] < 150) )
   { 
   if ( digitalRead(ChannelList[chNum][CHANNEL_GPIO]) != (ChannelList[chNum][CHANNEL_INVERTED] ? true : false ) )
    { digitalWrite(ChannelList[chNum][CHANNEL_GPIO], (ChannelList[chNum][CHANNEL_INVERTED] ? true : false )); }
   }
  else if (  ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK 
         ||  ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_MANUALLY 
         || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 150 && ChannelList[chNum][CHANNEL_LASTSTATE] < 250) )
   {
   if ( digitalRead(ChannelList[chNum][CHANNEL_GPIO]) != (ChannelList[chNum][CHANNEL_INVERTED] ? false : true ) )
    { digitalWrite(ChannelList[chNum][CHANNEL_GPIO], (ChannelList[chNum][CHANNEL_INVERTED] ? false : true )); }
   }
  } 
 }
}

void save_channellist_to_EEPROM()
{
int chNum, ch_element_num;
#ifdef DEBUG 
 Serial.println(F("Saving channels"));
#endif
for ( chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 for ( ch_element_num = 0; ch_element_num < CHANNEL_NUM_ELEMENTS; ch_element_num++ ) 
  {
  if ( ChannelList[chNum][ch_element_num] != EEPROM.read(CHANNELLIST_EEPROM_ADDRESS + ch_element_num + chNum * CHANNEL_NUM_ELEMENTS) )
   { EEPROM.write(CHANNELLIST_EEPROM_ADDRESS + ch_element_num + chNum * CHANNEL_NUM_ELEMENTS, ChannelList[chNum][ch_element_num]); }
  }
 }
EEPROM.commit();
}

void setupServer()
{
server.on("/",[]() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 if ( !AccessPointMode ) { drawHeader(0); drawHomePage(); }
                   else  { drawHeader(2); drawWiFiSettings(); AccessPointModeTimer = millis(); }
 });
server.on("/settasksettings", []() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 int taskNumber, param;   
 String buf;
 bool needSave = false;
 if ( server.args() != TASK_NUM_ELEMENTS - 1 ) { return; }
 buf = server.argName(0);
 buf = buf.substring(1);
 taskNumber = buf.toInt(); // task
 buf = server.arg(0); param = buf.toInt(); // channel
 if ( param >= 0 && param <= numberOfChannels ) { if ( TaskList[taskNumber][TASK_CHANNEL] != param ) { TaskList[taskNumber][TASK_CHANNEL] = param; needSave = true; } } 
 buf = server.arg(1); param = buf.toInt(); // action
 if ( param == ACTION_NOACTION || param == ACTION_TURN_OFF || param == ACTION_TURN_ON ) 
  { if ( TaskList[taskNumber][TASK_ACTION] != param ) { TaskList[taskNumber][TASK_ACTION] = param; needSave = true; } } 
 buf = server.arg(2); param = buf.toInt(); // hour
 if ( param >= 0 && param <= 23 ) { if ( TaskList[taskNumber][TASK_HOUR] != param ) { TaskList[taskNumber][TASK_HOUR] = param; needSave = true; } } 
 buf = server.arg(3); param = buf.toInt(); // min
 if ( param >= 0 && param <= 59 ) { if ( TaskList[taskNumber][TASK_MIN] != param ) { TaskList[taskNumber][TASK_MIN] = param; needSave = true; } } 
 buf = server.arg(4); param = buf.toInt(); // sec
 if ( param >= 0 && param <= 59 ) { if ( TaskList[taskNumber][TASK_SEC] != param ) { TaskList[taskNumber][TASK_SEC] = param; needSave = true; } } 
 buf = server.arg(5); param = buf.toInt(); // day(s)
 if ( param >= 0 && param <= TASK_DAY_EVERYDAY )  { if ( TaskList[taskNumber][TASK_DAY] != param ) { TaskList[taskNumber][TASK_DAY] = param; needSave = true; } }
 if ( needSave ) 
  { 
  log_Append(LOG_EVENT_TASK_SETTINGS_CHANGED, 0, taskNumber);
  save_tasklist_to_EEPROM();
  check_previous_tasks();
  find_next_tasks(); 
  }
 ServerSendMessageAndRefresh();
 }); 
server.on("/cleartasklist", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 for ( int taskNum = 0; taskNum < numberOfTasks; taskNum++ )
  { 
  yield(); 
  TaskList[taskNum][TASK_ACTION] = ACTION_NOACTION; 
  TaskList[taskNum][TASK_HOUR] = 0; 
  TaskList[taskNum][TASK_MIN] = 0; 
  TaskList[taskNum][TASK_SEC] = 0; 
  TaskList[taskNum][TASK_DAY] = TASK_DAY_EVERYDAY;
  TaskList[taskNum][TASK_CHANNEL] = 0;
  }
 log_Append(LOG_EVENT_CLEAR_TASKLIST); 
 save_tasklist_to_EEPROM(); 
 ServerSendMessageAndRefresh();
 }); 
server.on("/format", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 if ( littleFS_OK ) 
  {
  RingCounter.clear();
  LittleFS.format();
  ServerSendMessageAndReboot();
  }
 }); 
server.on("/setnumberOfTasks", []() 
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String buf = server.arg(0);
 int param = buf.toInt();
 if ( param >= TASKLIST_MIN_NUMBER && param <= TASKLIST_MAX_NUMBER ) 
  { if ( numberOfTasks != param ) 
     {
     numberOfTasks = param; 
     EEPROM.write(NUMBER_OF_TASKS_EEPROM_ADDRESS, numberOfTasks); EEPROM.commit();
     log_Append(LOG_EVENT_NUMBER_OF_TASKS_CHANGED);
     ServerSendMessageAndReboot();
     }
  } 
 ServerSendMessageAndRefresh();
 }); 
server.on("/setnumberOfChannels", []() 
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String buf = server.arg(0);
 int param = buf.toInt();
 if ( param >= CHANNELLIST_MIN_NUMBER && param <= CHANNELLIST_MAX_NUMBER ) 
  { if ( numberOfChannels != param ) 
     {
     numberOfChannels = param; 
     EEPROM.write(NUMBER_OF_CHANNELS_EEPROM_ADDRESS, numberOfChannels); EEPROM.commit();
     log_Append(LOG_EVENT_NUMBER_OF_CHANNELS_CHANGED);
     ServerSendMessageAndReboot();
     }
  } 
 ServerSendMessageAndRefresh();
 });
server.on("/setchannelstateon", []()
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 int chNum;
 String buf = server.argName(0);
 buf = buf.substring(1);
 chNum = buf.toInt();
 if ( calcOldChMode(chNum) != LOG_EVENT_CHANNEL_MANUALLY ) { log_Append(calcOldChMode(chNum), chNum, 0, LOG_EVENT_CHANNEL_MANUALLY); }
 ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_ON_MANUALLY;
 log_Append(LOG_EVENT_TASK_SWITCHING_MANUALLY, chNum, 0, 1);
 save_channellist_to_EEPROM(); 
 read_channellist_from_EEPROM_and_switch_channels();
 ServerSendMessageAndRefresh();
 }); 
server.on("/setchannelstateoff", []()
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 int chNum;
 String buf = server.argName(0);
 buf = buf.substring(1);
 chNum = buf.toInt();
 if ( calcOldChMode(chNum) != LOG_EVENT_CHANNEL_MANUALLY ) { log_Append(calcOldChMode(chNum), chNum, 0, LOG_EVENT_CHANNEL_MANUALLY); }
 ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_OFF_MANUALLY;
 log_Append(LOG_EVENT_TASK_SWITCHING_MANUALLY, chNum, 0, 0);
 save_channellist_to_EEPROM(); 
 read_channellist_from_EEPROM_and_switch_channels();
 ServerSendMessageAndRefresh();
 }); 
server.on("/setchannelstateonuntil", []()
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 int chNum;
 String buf = server.argName(0);
 buf = buf.substring(1);
 chNum = buf.toInt();
 if ( ActiveNowTasksList[chNum] <= numberOfTasks )
  {
  if ( calcOldChMode(chNum) != LOG_EVENT_CHANNEL_UNTIL_NEXT_TASK ) { log_Append(calcOldChMode(chNum), chNum, 0, LOG_EVENT_CHANNEL_UNTIL_NEXT_TASK); }
  ChannelList[chNum][CHANNEL_LASTSTATE] = ActiveNowTasksList[chNum] + 150;
  log_Append(LOG_EVENT_TASK_SWITCHING_MANUALLY, chNum, 0, 1);
  save_channellist_to_EEPROM(); 
  read_channellist_from_EEPROM_and_switch_channels();
  }
 ServerSendMessageAndRefresh();
 }); 
server.on("/setchannelstateoffuntil", []()
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 int chNum;
 String buf = server.argName(0);
 buf = buf.substring(1);
 chNum = buf.toInt();
 if ( ActiveNowTasksList[chNum] <= numberOfTasks )
  {
  if ( calcOldChMode(chNum) != LOG_EVENT_CHANNEL_UNTIL_NEXT_TASK ) { log_Append(calcOldChMode(chNum), chNum, 0, LOG_EVENT_CHANNEL_UNTIL_NEXT_TASK); }
  ChannelList[chNum][CHANNEL_LASTSTATE] = ActiveNowTasksList[chNum] + 50;
  log_Append(LOG_EVENT_TASK_SWITCHING_MANUALLY, chNum, 0, 0);
  save_channellist_to_EEPROM(); 
  read_channellist_from_EEPROM_and_switch_channels();
  }
 ServerSendMessageAndRefresh();
 }); 
server.on("/setchannelmanually", []()
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 int chNum;
 String buf = server.argName(0);
 buf = buf.substring(1);
 chNum = buf.toInt();
 if ( calcOldChMode(chNum) != LOG_EVENT_CHANNEL_MANUALLY ) { log_Append(calcOldChMode(chNum), chNum, 0, LOG_EVENT_CHANNEL_MANUALLY); }
      if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_BY_TASK
       || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 50 && ChannelList[chNum][CHANNEL_LASTSTATE] < 150) )
       {
       ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_OFF_MANUALLY;
       save_channellist_to_EEPROM(); 
       read_channellist_from_EEPROM_and_switch_channels();
       }
 else if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK
       || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 150 && ChannelList[chNum][CHANNEL_LASTSTATE] < 250) )
       {
       ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_ON_MANUALLY;
       save_channellist_to_EEPROM(); 
       read_channellist_from_EEPROM_and_switch_channels();
       }
 ServerSendMessageAndRefresh();
 }); 
server.on("/setchanneluntil", []()
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 int chNum;
 String buf = server.argName(0);
 buf = buf.substring(1);
 chNum = buf.toInt();
 if ( calcOldChMode(chNum) != LOG_EVENT_CHANNEL_UNTIL_NEXT_TASK ) { log_Append(calcOldChMode(chNum), chNum, 0, LOG_EVENT_CHANNEL_UNTIL_NEXT_TASK); }
 if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_BY_TASK || ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_MANUALLY )
  { 
  if ( ActiveNowTasksList[chNum] <= numberOfTasks )
   {
   ChannelList[chNum][CHANNEL_LASTSTATE] = ActiveNowTasksList[chNum] + 50; 
   save_channellist_to_EEPROM(); 
   read_channellist_from_EEPROM_and_switch_channels();
   }
  }
 if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK || ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_MANUALLY ) 
  {
  if ( ActiveNowTasksList[chNum] <= numberOfTasks )
   {
   ChannelList[chNum][CHANNEL_LASTSTATE] = ActiveNowTasksList[chNum] + 150; 
   save_channellist_to_EEPROM(); 
   read_channellist_from_EEPROM_and_switch_channels();
   }
  }
 ServerSendMessageAndRefresh();
 }); 
server.on("/setchannelbytasks", []()
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 int chNum;
 String buf = server.argName(0);
 buf = buf.substring(1);
 chNum = buf.toInt();
 if ( calcOldChMode(chNum) != LOG_EVENT_CHANNEL_BY_TASK ) { log_Append(calcOldChMode(chNum), chNum, 0, LOG_EVENT_CHANNEL_BY_TASK); }
      if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_OFF_MANUALLY
       || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 50 && ChannelList[chNum][CHANNEL_LASTSTATE] < 150) )
       {
       ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_OFF_BY_TASK;
       save_channellist_to_EEPROM();
       read_channellist_from_EEPROM_and_switch_channels();
       } 
 else if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_MANUALLY
       || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 150 && ChannelList[chNum][CHANNEL_LASTSTATE] < 250) )
       {
       ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_ON_BY_TASK;
       save_channellist_to_EEPROM();
       read_channellist_from_EEPROM_and_switch_channels();
       } 
 ServerSendMessageAndRefresh();
 }); 
server.on("/setchannelparams", []() 
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 bool needSave = false;
 int chNum, param;
 String buf = server.argName(0);
 buf = buf.substring(1);
 chNum = buf.toInt();
 buf = server.arg(0);
 param = buf.toInt();
 if ( param == 10 || param == 11 ) 
  { if ( ((!ChannelList[chNum][CHANNEL_ENABLED]) && param == ACTION_TURN_ON) 
        || (ChannelList[chNum][CHANNEL_ENABLED]  && param == ACTION_TURN_OFF) ) 
     {
     ChannelList[chNum][CHANNEL_ENABLED] = ( param == ACTION_TURN_OFF ? 0 : 1 ); 
     if ( NumEnabledTasks[chNum] > 0 ) { ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_OFF_BY_TASK; }
                                  else { ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_OFF_MANUALLY; }
     needSave = true;
     }
  } 
 buf = server.arg(1);
 param = buf.toInt();
 if ( NodeMCUpins[param] == "N/A" )
  { ServerSendMessageAndRefresh( 3, "/", (Language ? F("Не используйте GPIO 6,7,8 и 11 !") : F("Don't use GPIO 6,7,8 and 11 !"))); return;}
 if ( param >= 0 && param <= GPIO_MAX_NUMBER && NodeMCUpins[param] != "N/A" )
  { if ( ChannelList[chNum][CHANNEL_GPIO] != param ) 
     {
     ChannelList[chNum][CHANNEL_GPIO] = param; 
     needSave = true;
     }
  }   
 buf = server.arg(2);
 param = buf.toInt();
 if ( param == 10 || param == 11 ) 
  { if ( ((!ChannelList[chNum][CHANNEL_INVERTED]) && param == 11) || (ChannelList[chNum][CHANNEL_INVERTED] && param == 10) ) 
     {
     ChannelList[chNum][CHANNEL_INVERTED] = ( param == 10 ? 0 : 1 ); 
     needSave = true;
     }
  } 
 if ( needSave ) 
  {
  log_Append(LOG_EVENT_CHANNEL_SETTINGS_CHANGED, chNum);
  save_channellist_to_EEPROM(); 
  read_channellist_from_EEPROM_and_switch_channels(); 
  } 
 ServerSendMessageAndRefresh();
 }); 
server.on("/setlanguage", []() 
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String buf = server.arg(0);
 int param = buf.toInt();
 if ( param == 10 || param == 11 ) 
  { if ( Language != param - 10 ) 
     {
     Language = ( param == 10 ? 0 : 1 ); 
     EEPROM.write(LANGUAGE_EEPROM_ADDRESS, Language);
     EEPROM.commit();
     log_Append(LOG_EVENT_INTERFACE_LANGUAGE_CHANGED);
     }
  }  
 ServerSendMessageAndRefresh();
 }); 
server.on("/setntpTimeZone", []() 
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String buf = server.arg(0);
 int param = buf.toInt();
 if ( ntpTimeZone != param ) 
  {
  ntpTimeZone = param; 
  check_DaylightSave();
  EEPROM.write(NTP_TIME_ZONE_EEPROM_ADDRESS, ntpTimeZone + 12); EEPROM.commit();
  log_Append(LOG_EVENT_TIME_ZONE_CHANGED);
  ServerSendMessageAndReboot();
  }
 ServerSendMessageAndRefresh();
 }); 
server.on("/setntpdaylightsave", []() 
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String buf = server.arg(0);
 int param = buf.toInt();
 if ( param == 10 || param == 11 ) 
  { 
  if ( ntpAutoDaylightSavingEnabled != param - 10 ) 
   {
   ntpAutoDaylightSavingEnabled = ( param == 10 ? 0 : 1 ); 
   EEPROM.write(NTP_DAYLIGHT_SAVING_ENABLED_ADDRESS, ntpAutoDaylightSavingEnabled);
   EEPROM.commit();
   log_Append(LOG_EVENT_AUTO_DAYLIGHT_SAVING_CHANGED);
   check_DaylightSave();
   }
  }  
 ServerSendMessageAndRefresh();
 });
server.on("/setntpdaylightsavezone", []() 
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String buf = server.arg(0);
 int param = buf.toInt();
 if ( param == 10 || param == 11 ) 
  { 
  if ( ntpDaylightSaveZone != param - 10 ) 
   {
   ntpDaylightSaveZone = ( param == 10 ? 0 : 1 ); 
   EEPROM.write(NTP_DAYLIGHTSAVEZONE_EEPROM_ADDRESS, ntpDaylightSaveZone);
   EEPROM.commit();
   log_Append(LOG_EVENT_DAYLIGHT_SAVING_ZONE_CHANGED);
   check_DaylightSave();
   }
  }  
 ServerSendMessageAndRefresh();
 });
server.on("/setntpservername1", []() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String nname = server.arg(0);
 if ( nname.length() > 0 && nname.length() <= 32 && nname.compareTo(ntpServerName1) != 0 )
  {
  for ( int i = NTP_TIME_SERVER1_EEPROM_ADDRESS; i < NTP_TIME_SERVER1_EEPROM_ADDRESS + 32; ++i) { EEPROM.write(i, 0); } // fill with 0
  for ( unsigned int i = 0; i < nname.length(); ++i ) { EEPROM.write(i + NTP_TIME_SERVER1_EEPROM_ADDRESS, nname[i]); }
  EEPROM.commit();
  log_Append(LOG_EVENT_TIME_SERVER_CHANGED);
  read_and_set_ntpservers_from_EEPROM();
  ServerSendMessageAndRefresh();
  }
 });
server.on("/setntpservername2", []() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String nname = server.arg(0);
 if ( nname.length() > 0 && nname.length() <= 32 && nname.compareTo(ntpServerName2) != 0 )
  {
  for ( int i = NTP_TIME_SERVER2_EEPROM_ADDRESS; i < NTP_TIME_SERVER2_EEPROM_ADDRESS + 32; ++i) { EEPROM.write(i, 0); } // fill with 0
  for ( unsigned int i = 0; i < nname.length(); ++i ) { EEPROM.write(i + NTP_TIME_SERVER2_EEPROM_ADDRESS, nname[i]); }
  EEPROM.commit();
  log_Append(LOG_EVENT_TIME_SERVER_CHANGED);
  read_and_set_ntpservers_from_EEPROM();
  ServerSendMessageAndRefresh();
  }
 });
server.on("/setlogin", []() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String qlogin = server.arg(0);
 String qpass1 = server.arg(1);
 String oldpass = server.arg(3);
 if ( qlogin.length() > 0 && qlogin.length() <= 10 && qpass1.length() > 0 && qpass1.length() <= 10 && oldpass.length() > 0 && oldpass.length() <= 10 
   && qpass1 == server.arg(2) && oldpass == loginPass )
  {
  for ( int i = LOGIN_NAME_EEPROM_ADDRESS; i < LOGIN_NAME_EEPROM_ADDRESS + 10; ++i ) { EEPROM.write(i, 0); } // fill with 0
  for ( int i = LOGIN_PASS_EEPROM_ADDRESS; i < LOGIN_PASS_EEPROM_ADDRESS + 12; ++i ) { EEPROM.write(i, 0); } // fill with 0
  for ( unsigned int i = 0; i < qlogin.length(); ++i ) { EEPROM.write(i + LOGIN_NAME_EEPROM_ADDRESS, qlogin[i]); }
  for ( unsigned int i = 0; i < qpass1.length(); ++i ) { EEPROM.write(i + LOGIN_PASS_EEPROM_ADDRESS, qpass1[i]); }
  EEPROM.commit();
  if ( qlogin != loginName ) { log_Append(LOG_EVENT_LOGIN_NAME_CHANGED); }
  if ( qpass1 != loginPass ) { log_Append(LOG_EVENT_LOGIN_PASSWORD_CHANGED); }
  read_login_pass_from_EEPROM();
  ServerSendMessageAndRefresh();
  }
 else { ServerSendMessageAndRefresh( 3, "/", (Language ? F("Неверные данные или пароли не совпадают...") : F("Incorrect data or passwords do not match...")) ); }
 });  
server.on("/wifisetting", []() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 boolean needReboot = false;
 String tmpPassword = ( server.arg(1) != "" ? server.arg(1) : AP_pass );
 if ( wifiManuallySetIP.toString()     != server.arg(3) 
   || wifiManuallySetSUBNET.toString() != server.arg(4) 
   || wifiManuallySetGW.toString()     != server.arg(5) 
   || wifiManuallySetDNS.toString()    != server.arg(6)
   || wifiManuallySetAddresses != server.arg(2).toInt() ) { needReboot = true; }
 if ( server.arg(0) != "" && server.arg(1) != "" )
  { if ( server.arg(0) != AP_name || server.arg(1) != AP_pass ) { needReboot = true; } }
 if ( needReboot ) 
  {
  ServerSendMessageAndRefresh( 10, "/", (Language ? F("Попытка переподключиться...") : F("Trying to reconnect...")) );
  yield();
  delay(500);
  server.stop();
  WiFi.disconnect(true);
  if ( WiFi.getPersistent() ) { WiFi.persistent(false); } // disable saving wifi config into SDK flash area
  WiFi.mode(WIFI_OFF);
  WiFi.persistent(true);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);
  yield();
  delay(500);
  WiFi.softAPdisconnect(true);
  if ( server.arg(2).toInt() > 0 ) 
   {
   IPAddress ip; ip.fromString(server.arg(3));
   IPAddress sn; sn.fromString(server.arg(4));  
   IPAddress gw; gw.fromString(server.arg(5));
   IPAddress dn; dn.fromString(server.arg(6));
   WiFi.config(ip, gw, sn, dn);
   }   
  WiFi.begin(server.arg(0), tmpPassword);
  #ifdef DEBUG 
   Serial.print(F("Trying to reconnect "));
   Serial.println(server.arg(0));
  #endif
  int ct = 60;
  while ( WiFi.status() != WL_CONNECTED && ct > 0 ) 
   {
   yield(); delay(500); ct--;
   #ifdef DEBUG 
    Serial.print(F("."));
   #endif
   }
  #ifdef DEBUG 
   Serial.println();
  #endif
  if ( WiFi.status() == WL_CONNECTED ) 
   {
   AP_name = server.arg(0); 
   AP_pass = tmpPassword;
   save_AP_name_pass_to_EEPROM();  
   wifiManuallySetIP.fromString(server.arg(3));
   wifiManuallySetSUBNET.fromString(server.arg(4));
   wifiManuallySetGW.fromString(server.arg(5));
   wifiManuallySetDNS.fromString(server.arg(6));
   save_manually_set_addresses();
   wifiManuallySetAddresses = server.arg(2).toInt();
   EEPROM.write(WIFI_MANUALLY_SET_EEPROM_ADDRESS, server.arg(2).toInt());
   EEPROM.commit();
   #ifdef DEBUG 
    Serial.println(F("Connected to ")); Serial.println(AP_name);
   #endif
   log_Append(LOG_EVENT_WIFI_SETTINGS_CHANGED);
   }
  else
   {
   #ifdef DEBUG 
    Serial.println(F("Can't connect to WiFi.")); 
   #endif
   }
  server.stop();
  delay(500);
  ESP.restart(); 
  }
 else { ServerSendMessageAndRefresh(); }
 });
server.on("/savesettings", []() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String fileName = server.arg(0);
 File f = LittleFS.open(fileName, "w");
 if ( f ) 
  {
  boolean writeerror = false;
  for ( int i = 0; i < MAX_EEPROM_ADDRESS; i++ )
   { 
   uint8_t b = EEPROM.read(i);
   if ( f.write((uint8_t *)&b, sizeof(b)) == 0 ) { writeerror = true; break; }
   }   
  f.close();
  if ( writeerror ) { ServerSendMessageAndRefresh( 3, "/", (Language ? F("ОШИБКА записи в файл ") : F("ERROR saving to file ")), fileName ); }
  else 
   {
   log_Append(LOG_EVENT_FILE_SAVE);
   ServerSendMessageAndRefresh( 3, "/", (Language ? F("Сохранено в файл ") : F("Saved to file ")), fileName ); 
   }
  }
 else { ServerSendMessageAndRefresh( 3, "/", (Language ? F("ОШИБКА !") : F("ERROR !")) ); }
 });  
server.on("/restoresettings", []() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String fileName = server.arg(0);
 File f = LittleFS.open(fileName, "r");
 if ( f ) 
  {
  uint8_t b = f.read();
  if ( b == FIRST_RUN_SIGNATURE )
   {
   f.seek(0, SeekSet);
   int i = 0;
   while ( f.available() )
    { 
    b = f.read();
    EEPROM.write(i, b);
    i++;
    if ( i >= MAX_EEPROM_ADDRESS ) { break; }
    }   
   f.close();
   EEPROM.commit();
   RingCounter.clear();
   RingCounter.init();
   saveGMTtimeToRingCounter();
   log_Append(LOG_EVENT_FILE_RESTORE);
   ServerSendMessageAndReboot();
   }
  else
   {
   f.close();
   ServerSendMessageAndRefresh( 3, "/", (Language ? F("Неверный файл...") : F("Incorrect file...")) );
   }
  }
 else { ServerSendMessageAndRefresh( 3, "/", (Language ? F("ОШИБКА !") : F("ERROR !")) ); }
 });  
server.on("/renamefile", []() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String oldName = server.arg(0);
 String newName = server.arg(1);
 if ( LittleFS.rename(oldName, newName) )
  {
  log_Append(LOG_EVENT_FILE_RENAME);
  ServerSendMessageAndRefresh( 3, "/", (Language ? F("Переименован файл ") : F("Renamed file ")), oldName, (Language ? F(" в ") : F(" to ")), newName); 
  }
 else 
  { ServerSendMessageAndRefresh( 3, "/", (Language ? F("ОШИБКА !") : F("ERROR !")) ); }
 });  
server.on("/deletefile", []() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String fileName = server.arg(0);
 if ( LittleFS.exists(fileName) ) 
  {
  LittleFS.remove(fileName);
  log_Append(LOG_EVENT_FILE_DELETE);
  ServerSendMessageAndRefresh( 3, "/", (Language ? F("Удален файл ") : F("Deleted file ")), fileName );
  }
 else { ServerSendMessageAndRefresh( 3, "/", (Language ? F("ОШИБКА !") : F("ERROR !")) ); }
 });  
server.on("/downloadfile", HTTP_POST, []()
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String fileName = server.arg(0);
 if ( LittleFS.exists(fileName) ) 
  {
  File downloadHandle = LittleFS.open("/" + fileName, "r");
  if ( downloadHandle )
   {
   log_Append(LOG_EVENT_FILE_DOWNLOAD); 
   server.sendHeader("Content-Type", "text/text");
   server.sendHeader("Content-Disposition", "attachment; filename=" + fileName);
   server.sendHeader("Connection", "close");
   server.streamFile(downloadHandle, "application/octet-stream");
   downloadHandle.close();
   } 
  }
 else { ServerSendMessageAndRefresh( 3, "/", (Language ? F("ОШИБКА !") : F("ERROR !")) ); }
 });
server.onFileUpload(handleFileUpload);
server.on("/uploadfile", HTTP_POST, []()
 {
 ServerSendMessageAndRefresh( 3, "/", (Language ? F("Файл загружен") : F("File uploaded") ));
 });
server.on("/reset", []()  
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 EEPROM.write(FIRST_RUN_SIGNATURE_EEPROM_ADDRESS, 0); 
 EEPROM.commit();
 RingCounter.clear();
 log_Append(LOG_EVENT_FILE_UPLOAD);
 ServerSendMessageAndReboot();
 }); 
server.on("/restartapmode", []()  
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String mess = (Language ? F("Точка доступа:") : F("Access Point:<br> SSID = ")); 
 mess += APMODE_SSID; 
 mess += F("<br> Password = "); 
 mess += APMODE_PASSWORD;
 mess += F("<br> URL = http://192.168.4.1 <br><br>");
 EEPROM.write(ACCESS_POINT_SIGNATURE_EEPROM_ADDRESS, ACCESS_POINT_SIGNATURE); 
 EEPROM.commit();
 log_Append(LOG_EVENT_RESTART_TO_AP_MODE);
 ServerSendMessageAndRefresh( 10, "/", mess );
 delay(8000); 
 ServerSendMessageAndReboot();
 }); 
server.on("/restart", []()  
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_Append(LOG_EVENT_MANUAL_RESTART);
 ServerSendMessageAndReboot();
 }); 
server.on("/settasklistcollapsed", []()  
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 TaskListCollapsed = !TaskListCollapsed;
 if ( EEPROM.read(TASKLISTCOLLAPSED_EEPROM_ADDRESS) != TaskListCollapsed )
  {
  EEPROM.write(TASKLISTCOLLAPSED_EEPROM_ADDRESS, TaskListCollapsed);
  EEPROM.commit();
  }
 ServerSendMessageAndRefresh();
 }); 
server.on("/setchannellistcollapsed", []()  
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 ChannelListCollapsed = !ChannelListCollapsed;
 if ( EEPROM.read(CHANNELLISTCOLLAPSED_EEPROM_ADDRESS) != ChannelListCollapsed )
  {
  EEPROM.write(CHANNELLISTCOLLAPSED_EEPROM_ADDRESS, ChannelListCollapsed);
  EEPROM.commit();
  }
 ServerSendMessageAndRefresh();
 }); 
server.on("/viewlog", []()
 { 
 if ( ntpcurEpochTime < NTP_MINIMUM_EPOCHTIME ) { return; }
 log_ViewDate = ntpcurEpochTime;
 log_CalcForViewDate();
 logstat_BegDate = log_CurDate;
 logstat_EndDate = log_CurDate;
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 }); 
server.on(LOG_DIR,[]() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 drawHeader(1);
 drawLOG(); 
 });
server.on("/log_first", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_StartRecord = log_NumRecords; 
 logstat_BegDate = log_CurDate;
 logstat_EndDate = log_CurDate;
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_previous", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_StartRecord += log_ViewStep; 
 if ( log_StartRecord > log_NumRecords ) { log_StartRecord = log_NumRecords; }
 logstat_BegDate = log_CurDate;
 logstat_EndDate = log_CurDate;
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_next", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_StartRecord -= log_ViewStep; 
 if ( log_StartRecord < 1 ) { log_StartRecord = log_ViewStep; }
 if ( log_StartRecord > log_NumRecords ) { log_StartRecord = log_NumRecords; }
 logstat_BegDate = log_CurDate;
 logstat_EndDate = log_CurDate;
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_last", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_StartRecord = log_ViewStep; 
 if ( log_StartRecord > log_NumRecords ) { log_StartRecord = log_NumRecords; }
 logstat_BegDate = log_CurDate;
 logstat_EndDate = log_CurDate;
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_incdate", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_ViewDate += SECS_PER_DAY;
 log_CalcForViewDate();
 logstat_BegDate = log_CurDate;
 logstat_EndDate = log_CurDate;
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_decdate", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_ViewDate -= SECS_PER_DAY;
 log_CalcForViewDate();
 logstat_BegDate = log_CurDate;
 logstat_EndDate = log_CurDate;
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_setdate", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String buf = server.arg(0); // YYYY-MM-DD
 int y = (buf.substring(0,4)).toInt();
 int m = (buf.substring(5,7)).toInt();
 int d = (buf.substring(8,10)).toInt();
 log_ViewDate = makeTime(y - 1970, m, d, 12, 0, 0);
 log_CalcForViewDate();
 logstat_BegDate = log_CurDate;
 logstat_EndDate = log_CurDate;
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_setstatperiod", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String buf = server.arg(0); // YYYY-MM-DD
 logstat_BegDate = buf.substring(0,4) + buf.substring(5,7) + buf.substring(8,10);
 buf = server.arg(1); // YYYY-MM-DD
 logstat_EndDate = buf.substring(0,4) + buf.substring(5,7) + buf.substring(8,10);
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_return", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 logstat_BegDate = log_CurDate;
 logstat_EndDate = log_CurDate;
 ServerSendMessageAndRefresh();
 });
server.on("/setlog_DaysToKeep", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String buf = server.arg(0);
 int param = buf.toInt();
 if ( log_DaysToKeep != param ) 
  {
  log_DaysToKeep = param; 
  EEPROM.write(LOG_DAYSTOKEEP_EEPROM_ADDRESS, log_DaysToKeep); EEPROM.commit();
  log_Append(LOG_EVENT_LOG_DAYS_TO_KEEP_CHANGED);
  ServerSendMessageAndReboot();
  }
 });
server.on("/setlog_ViewStep", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String buf = server.arg(0);
 int param = buf.toInt();
 if ( log_ViewStep != param ) 
  {
  log_ViewStep = param; 
  EEPROM.write(LOG_VIEWSTEP_EEPROM_ADDRESS, log_ViewStep); EEPROM.commit();
  log_Append(LOG_EVENT_LOG_ENTRIES_PER_PAGE_CHANGED);
  ServerSendMessageAndRefresh( 0, LOG_DIR );
  }
 });
#ifdef DEBUG 
 server.on("/log_deletefile", []()
  { 
  if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
  char path[32];
  log_pathFromDate(path, log_ViewDate);
  if ( LittleFS.exists(path) ) { LittleFS.remove(path); }
  ServerSendMessageAndReboot();
  });
#endif 
}

void setup()
{
#ifdef DEBUG 
 Serial.begin(9600);
 delay(3000); yield();
 Serial.println();
 Serial.println(F("Versatile timer started"));
 Serial.print(F("LittleFS: "));
#endif
delay(500); yield();
littleFS_OK = LittleFS.begin();
#ifdef DEBUG 
 if ( littleFS_OK ) { Serial.println(F("OK")); } else { Serial.println(F("FAIL")); }
#endif 
if ( littleFS_OK ) 
 {
 for ( int i = 0; i < LOG_BUFFER_SIZE; i++ ) { log_delayed_write_buffer[i].event = LOG_POLLING; }
 FSInfo fs_info;
 LittleFS.info(fs_info);
 littleFStotalBytes = fs_info.totalBytes;
 littleFSblockSize = fs_info.blockSize;
 }
EEPROM.begin(EEPROMSIZE);
// Check signature to detect first run on the device and prepare EEPROM
if ( EEPROM.read(FIRST_RUN_SIGNATURE_EEPROM_ADDRESS) != FIRST_RUN_SIGNATURE ) 
 {
 #ifdef DEBUG 
  Serial.println(F("First run on the device detected."));
  Serial.print(F("Preparing EEPROM addresses from 0 to "));
  Serial.println(MAX_EEPROM_ADDRESS);
 #endif 
 for ( int i = 0; i < MAX_EEPROM_ADDRESS; i++ ) { EEPROM.write(i, 0); }
 EEPROM.write(FIRST_RUN_SIGNATURE_EEPROM_ADDRESS, FIRST_RUN_SIGNATURE); 
 EEPROM.write(LOG_DAYSTOKEEP_EEPROM_ADDRESS, 3);
 EEPROM.write(LOG_VIEWSTEP_EEPROM_ADDRESS, LOG_VIEWSTEP_DEF);
 EEPROM.write(NTP_TIME_ZONE_EEPROM_ADDRESS, NTP_DEFAULT_TIME_ZONE + 12);
 EEPROM.write(NTP_DAYLIGHT_SAVING_ENABLED_ADDRESS, 1);
 EEPROM.write(BUILD_HOUR_EEPROM_ADDRESS, FW_H);
 EEPROM.write(BUILD_MIN_EEPROM_ADDRESS, FW_M);
 EEPROM.write(BUILD_SEC_EEPROM_ADDRESS, FW_S);
 EEPROM.commit();
 #ifdef DEBUG 
  Serial.println(F("Formatting LittleFS"));
 #endif   
 LittleFS.format();
 #ifdef DEBUG 
  Serial.println(F("Rebooting..."));
  delay(1000);
 #endif 
 ESP.restart();
 }
// Check signature to set AccessPointMode after start
if ( EEPROM.read(ACCESS_POINT_SIGNATURE_EEPROM_ADDRESS) == ACCESS_POINT_SIGNATURE ) 
 {
 AccessPointMode = true;
 EEPROM.write(ACCESS_POINT_SIGNATURE_EEPROM_ADDRESS, 0); 
 EEPROM.commit();
 }
read_settings_from_EEPROM();
read_and_set_ntpservers_from_EEPROM();
read_login_pass_from_EEPROM();
read_AP_name_pass_from_EEPROM();
read_manually_set_addresses();
ChannelList = new uint8_t*[numberOfChannels];
for ( int chNum = 0; chNum < numberOfChannels; chNum++ ) 
 {
 yield(); 
 ChannelList[chNum] = new uint8_t[CHANNEL_NUM_ELEMENTS];
 ActiveNowTasksList[chNum] = 255;
 NextTasksList[chNum] = 255;
 NumEnabledTasks[chNum] = 0;
 }
if ( !AccessPointMode )
 { 
 read_channellist_from_EEPROM_and_switch_channels();
 read_and_sort_tasklist_from_EEPROM();
 yield(); 
 RTCMEMread();
 struct LastChannelsStates lastchannelsstates = read_lastchannelsstates();
 log_Append(LOG_EVENT_START, lastchannelsstates.firstpart, lastchannelsstates.lastpart, 0);
 checkFirmwareUpdated();
 getLastResetReason();
 begin_WiFi_STA();
 yield(); 
 }
else 
 { begin_WiFi_AP(); }
yield(); 
lastPowerOff = RingCounter.init();
if ( ipEmpty(wifiManuallySetIP) )     { wifiManuallySetIP = WiFi.localIP(); }
if ( ipEmpty(wifiManuallySetGW) )     { wifiManuallySetGW = WiFi.gatewayIP(); }
if ( ipEmpty(wifiManuallySetDNS) )    { wifiManuallySetDNS = WiFi.dnsIP(); }
if ( ipEmpty(wifiManuallySetSUBNET) ) { wifiManuallySetSUBNET = WiFi.subnetMask(); }
//
settimeofday_cb(ntpTimeSynked); // optional callback function
configTime("GMT0",ntpServerName1.c_str(),ntpServerName2.c_str(),NTP_SERVER_NAME);
//
counterOfReboots = EEPROMReadInt(COUNTER_OF_REBOOTS_EEPROM_ADDRESS);
if ( counterOfReboots < 0 || counterOfReboots > 32766 ) { counterOfReboots = 0; }
counterOfReboots++;
EEPROMWriteInt(COUNTER_OF_REBOOTS_EEPROM_ADDRESS, counterOfReboots); 
//
setupServer();
server.begin();
if ( !AccessPointMode ) { if ( MDNS.begin(MDNSHOST) ) { MDNS.addService("http", "tcp", 80); } }
ESP.wdtEnable(WDTO_8S);
everyMinuteTimer = millis();
everyHalfSecondTimer = millis();
if ( AccessPointMode ) { AccessPointModeTimer = millis(); }
RingCounterSaveTimer = millis();
}

void loop()
{
yield(); delay(0);
if ( AccessPointMode )
 {
 server.handleClient();
 if ( millis() - AccessPointModeTimer > ACCESS_POINT_MODE_TIMER_INTERVAL ) { ServerSendMessageAndReboot(); }
 return;
 }  
statusWiFi = ( WiFi.status() == WL_CONNECTED );  
if ( statusWiFi ) 
 {
 server.handleClient();
 MDNS.update();
 } 
if ( ntpTimeIsSynkedFirst && ntpTimeIsSynked )
 {
 check_DaylightSave(); 
 log_process();
 check_previous_tasks(); 
 find_next_tasks();
 everyMinuteTimer = millis();
 ntpTimeIsSynkedFirst = false;
 }
if ( statusWiFi != previousstatusWiFi )
 {
 log_Append(statusWiFi ? LOG_EVENT_WIFI_CONNECTION_SUCCESS : LOG_EVENT_WIFI_CONNECTION_ERROR); 
 previousstatusWiFi = statusWiFi; 
 }
if ( millis() - everyMinuteTimer > 60000UL )
 {
 check_DaylightSave();
 everyMinuteTimer = millis();
 } 
if ( millis() - everyHalfSecondTimer > 500UL )
 {
 ntpGetTime();
 yield(); delay(0);
 check_previous_tasks();
 find_next_tasks();
 everyHalfSecondTimer = millis();
 }
if ( millis() - RingCounterSaveTimer > RINGCOUNTER_SAVE_PERIOD )
 {
 saveGMTtimeToRingCounter();
 RingCounterSaveTimer = millis();
 }
log_Append(LOG_POLLING);
}
