// Tested on Arduino IDE 1.8.8, 1.8.11, 1.8.16
// Tested on NodeMCU 1.0 (ESP-12E Module) and on Generic ESP8266 Module

// !!! CRITICAL IMPORTANT !!! 
// Set 'Debug port: "Serial"' in Arduino IDE

//#define DEBUG 1

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <NTPClient.h>                 // https://github.com/arduino-libraries/NTPClient
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <LittleFS.h>                  // https://github.com/esp8266/Arduino/tree/master/libraries/LittleFS
#include <time.h>
//
// Create a Secrets.h file with Wi-Fi connection settings as follows:
//
//   #define AP_SSID   "yourSSID"
//   #define AP_PASS   "yourPASSWORD"
//
#include "Secrets.h"
//
//#define USE_FIXED_IP_ADDRESSES 1
#ifdef USE_FIXED_IP_ADDRESSES 
 IPAddress local_IP(192, 168, 1, 111);
 IPAddress gateway(192, 168, 1, 1);
 IPAddress subnet(255, 255, 0, 0);
 IPAddress primaryDNS(8, 8, 8, 8);
 IPAddress secondaryDNS(8, 8, 4, 4);
#endif
#define POOL_SERVER_NAME                          "europe.pool.ntp.org" // timeClient poolServerName
#define MDNSHOST                                  "VT" // mDNS host
#define FIRST_RUN_SIGNATURE_EEPROM_ADDRESS         0 // int FIRST_RUN_SIGNATURE
#define LANGUAGE_EEPROM_ADDRESS                    4 // boolean Language
#define LOG_DAYSTOKEEP_EEPROM_ADDRESS              5 // byte log_DaysToKeep
#define LOG_VIEWSTEP_EEPROM_ADDRESS                6 // byte log_ViewStep
#define COUNTER_OF_REBOOTS_EEPROM_ADDRESS          8 // int counterOfReboots
#define NUMBER_OF_TASKS_EEPROM_ADDRESS            10 // byte numberOfTasks
#define NTP_TIME_ZONE_EEPROM_ADDRESS              11 // byte 1...24 (int ntpTimeZone + 12)
#define NTP_DEFAULT_TIME_ZONE                      3
#define NUMBER_OF_CHANNELS_EEPROM_ADDRESS         12 // byte numberOfChannels
#define LOGIN_NAME_PASS_EEPROM_ADDRESS            16 // (16-26) String loginName from LOGIN_NAME_PASS_EEPROM_ADDRESS to LOGIN_NAME_PASS_EEPROM_ADDRESS + 10
                                                     // (27-39) String loginPass from LOGIN_NAME_PASS_EEPROM_ADDRESS + 11 to LOGIN_NAME_PASS_EEPROM_ADDRESS + 22
#define CHANNELLIST_EEPROM_ADDRESS                40 // byte ChannelList from CHANNELLIST_EEPROM_ADDRESS to CHANNELLIST_EEPROM_ADDRESS + CHANNELLIST_MAX_NUMBER * CHANNEL_NUM_ELEMENTS
#define TASKLIST_EEPROM_ADDRESS                  100 // byte TaskList from TASKLIST_EEPROM_ADDRESS to TASKLIST_EEPROM_ADDRESS + numberOfTasks * TASK_NUM_ELEMENTS
#define FIRST_RUN_SIGNATURE                      139 // (0x8B) two-byte signature to verify the first run on the device and prepare EEPROM
#define GPIO_MAX_NUMBER                           16
#define CHANNELLIST_MIN_NUMBER                     1
#define CHANNELLIST_MAX_NUMBER                    15
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
#define TASK_NUM_ELEMENTS                          6
#define TASK_ACTION                                0
#define TASK_HOUR                                  1
#define TASK_MIN                                   2
#define TASK_SEC                                   3
#define TASK_DAY                                   4
#define TASK_CHANNEL                               5
#define TASK_DAY_WORKDAYS                          7
#define TASK_DAY_WEEKENDS                          8
#define TASK_DAY_EVERYDAY                          9
#define ACTION_NOACTION                            0
#define ACTION_TURN_OFF                           10
#define ACTION_TURN_ON                            11 
#define EVENT_START                                0
#define EVENT_MANUAL_SWITCHING                     1
#define EVENT_TASK_SWITCHING                       2
#define DAYSTOKEEP_DEF                             7
#define DAYSTOKEEP_MIN                             1
#define DAYSTOKEEP_MAX                            60
#define LOG_DIR                                   "/log"
#define LOG_VIEWSTEP_DEF                          20
#define LOG_VIEWSTEP_MIN                           5
#define LOG_VIEWSTEP_MAX                          50
#define SECS_PER_MIN                            60UL
#define SECS_PER_HOUR                         3600UL
#define SECS_PER_DAY                         86400UL
#define LEAP_YEAR(Y)                         (((1970+(Y))>0) && !((1970+(Y))%4) && (((1970+(Y))%100) || !((1970+(Y))%400)))
//                    GPIO      0     1    2    3    4    5     6     7     8     9    10    11   12   13   14   15   16
const String NodeMCUpins[] = {"D3","D10","D4","D9","D2","D1","N/A","N/A","N/A","D11","D12","N/A","D6","D7","D5","D8","D0"};
const String namesOfDaysE[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Workdays","Weekends","Every day"};
const String namesOfDaysR[] = {"Воскресенье","Понедельник","Вторник","Среда","Четверг","Пятница","Суббота","Рабочие дни","Выходные дни","Каждый день"};
const String namesOfEventsE[] = {"Start","Manual switching&nbsp;","Switching by task"};
const String namesOfEventsR[] = {"Начало работы","Переключение вручную&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;","Переключение по заданию"};
const uint8_t monthDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
//
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, POOL_SERVER_NAME, 0, 60000UL);
//
struct Log_Data
 {
 time_t   utc; 
 uint8_t  event;  // Event number (EVENT_START, EVENT_MANUAL_SWITCHING, EVENT_TASK_SWITCHING), see namesOfEventsE/namesOfEventsR
 uint8_t  ch;     // Channel number (CHANNELLIST_MIN_NUMBER...CHANNELLIST_MAX_NUMBER), 0 if none
 uint8_t  task;   // Task number (TASKLIST_MIN_NUMBER...TASKLIST_MAX_NUMBER), 0 if none
 uint8_t  act;    // Action (0 / 1)
 };
boolean littleFS_OK = false;
byte log_DaysToKeep;
byte log_ViewStep;
size_t log_FileSize = 0;
int log_NumRecords = 0;
time_t log_ViewDate;
String log_MinDate;
String log_MaxDate;
int log_StartRecord;
int log_Day = 0;
int log_Month = 0;
int log_Year = 0;
time_t log_today = 0;
File log_FileHandle;
File uploadFileHandle;
unsigned long log_lastProcess = 0;
bool log_processAfterStart = true;
char log_curPath[32];
//
String loginName; // default "admin"
String loginPass; // default "admin"
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
byte numberOfChannels; // CHANNELLIST_MIN_NUMBER...CHANNELLIST_MAX_NUMBER
uint8_t** ChannelList;
// uint8_t ChannelList[numberOfChannels][CHANNEL_NUM_ELEMENTS]
                                // 0 - 0...GPIO_MAX_NUMBER - GPIO
                                // 1 - 0...1 - channel controls noninverted/inverted
                                // 2 - 0...1 - channel last saved state
                                // 3 - 0...1 - channel enabled
byte ActiveNowTasksList[CHANNELLIST_MAX_NUMBER];  
byte NextTasksList[CHANNELLIST_MAX_NUMBER];
byte NumEnabledTasks[CHANNELLIST_MAX_NUMBER];                         
int counterOfReboots = 0; 
int ntpTimeZone = NTP_DEFAULT_TIME_ZONE;  // -11...12
boolean Language = false;       // false - English, true - Russian
#define NTPUPDATEINTERVAL 1800000UL
bool timeSyncOK = false;
bool timeSyncInitially = false;
#define EPOCHTIMEMAXDIFF 600UL  // seconds
unsigned long curEpochTime = 0; // time in seconds since Jan. 1, 1970
int curTimeHour = 0;
int curTimeMin = 0;
int curTimeSec = 0;
int curDayOfWeek = 0;           // 0 - Sunday, 1...5 - Monday...Friday, 6 - Saturday
bool statusWiFi = false;
unsigned long everySecondTimer = 0;
unsigned long CheckTaskListTimer = 0;
unsigned long setClockcurrentMillis, setClockpreviousMillis, setClockelapsedMillis; 
                               
/////////////////////////////////////////////////////////

void ServerSendMessageAndRefresh( int interval = 0, String url = "/", String mess1 = "", String mess2 = "", String mess3 = "", String mess4 = "" )
{
String ans = F("<META http-equiv='refresh' content='");
ans += String(interval) + F(";URL=") + url + F("'>") + mess1 + F("&nbsp;") + mess2 + F("&nbsp;") + mess3 + F("&nbsp;") + mess4;
server.send(200, F("text/html; charset=utf-8"), ans);
}

void ServerSendMessageAndReboot()
{
ServerSendMessageAndRefresh( 10, "/", (Language ? F(" Перезагрузка...") : F(" Rebooting...")) );
delay(500);
server.stop();
ESP.restart(); 
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
 if ( !filename.startsWith("/") ) filename = "/"+filename;
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
unsigned long currentMillis = millis();
if  ( currentMillis - log_lastProcess > 1000UL || log_processAfterStart )
 {
 time_t today = curEpochTime / 86400 * 86400; // remove the time part
 if ( log_today != today ) // we have switched to another day
  { 
  log_today = today;
  log_pathFromDate(log_curPath, log_today);
  log_runRotation();
  }
 log_lastProcess = currentMillis;
 log_processAfterStart = false;
 }
} 

void log_pathFromDate(char *output, time_t date)
{
if ( date <= 0 ) { date = log_today; }
struct tm *tinfo = gmtime(&date);
sprintf_P(output, "%s/%d%02d%02d", LOG_DIR, 1900 + tinfo->tm_year, tinfo->tm_mon + 1, tinfo->tm_mday);
}

void log_runRotation()
{
const uint8_t dirLen = strlen(LOG_DIR);
Dir tempDir = LittleFS.openDir(LOG_DIR);
while ( tempDir.next() )
 {
 const char *dateStart = tempDir.fileName().c_str() + dirLen + 1;
 const time_t midnight = log_filenameToDate(dateStart);
 if ( midnight < (log_today - log_DaysToKeep * 86400) )  { LittleFS.remove(tempDir.fileName()); }
 }
}

time_t log_filenameToDate(const char *filename)
{
struct tm tm = {0,0,0,1,0,100,0,0,0};
char datePart[5] = {0};
strncpy(datePart, filename, 4);
tm.tm_year = atoi(datePart) - 1900;
strncpy(datePart, filename + 4, 2);
datePart[2] = '\0';
tm.tm_mon = atoi(datePart) - 1;
strncpy(datePart, filename + 6, 2);
tm.tm_mday = atoi(datePart);
struct tm start2000 = {0,0,0,1,0,100,0,0,0};
return ( mktime(&tm) - (mktime(&start2000) - 946684800) ) / 86400 * 86400;
}

void log_Append(uint8_t ev, uint8_t ch, uint8_t ts, uint8_t act)
{
if ( !littleFS_OK ) { return; }
struct Log_Data data = { curEpochTime, ev, ch, ts, act };
File f = LittleFS.open(log_curPath, "a");
if ( f ) { f.write((uint8_t *)&data, sizeof(data)); f.close(); }
}

boolean log_readRow(Log_Data *output, size_t rownumber)
{
if ( ((int32_t)(log_FileHandle.size() / sizeof(Log_Data)) - (int32_t)rownumber) <= 0 ) { return false; }
log_FileHandle.seek(rownumber * sizeof(Log_Data));
log_FileHandle.read((uint8_t *)output, sizeof(Log_Data));
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
log_FileSize = log_FileSizeForDate(log_ViewDate);
log_NumRecords = log_FileSize / sizeof(Log_Data); 
log_StartRecord = log_NumRecords;
}

void drawHeader(boolean isMainPage)
{
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
content += F("<body style='background: #87CEFA'><div class='maindiv'><div class='dac'><span style='font-size:22pt'>");
if ( isMainPage )
 {
 content += (Language ? F("Универсальный программируемый таймер</span><p>Время: <b>") : F("Versatile timer</span><p>Time: <b>"));
 content += String(curTimeHour) + F(":")  + ( curTimeMin < 10 ? "0" : "" ) + String(curTimeMin) + F(":") + ( curTimeSec < 10 ? "0" : "" ) + String(curTimeSec) + F("</b>,&nbsp;<b>");
 content += (Language ? namesOfDaysR[curDayOfWeek] : namesOfDaysE[curDayOfWeek]);
 long secs = millis()/1000;
 long mins = secs / 60;
 long hours = mins / 60;
 long days = hours / 24;
 secs = secs - ( mins * 60 );
 mins = mins - ( hours * 60 );
 hours = hours - ( days * 24 );
 content += (Language ? F("</b>&emsp;Время работы: <b>") : F("</b>&emsp;Uptime: <b>"));
 if ( days > 0 )  { content += String(days)  + (Language ? F("д ") : F("d ")); } 
 if ( hours > 0 ) { content += String(hours) + (Language ? F("ч ") : F("h ")); }
 if ( mins > 0 )  { content += String(mins)  + (Language ? F("м ") : F("m ")); }
 content += String(secs) + (Language ? F("с ") : F("s "));
 content += (Language ? F("</b>&emsp;Версия: <b>") : F("</b>&emsp;Version: <b>"));
 content += String(__DATE__);
 content += (Language ? F("</b>&emsp;Перезагрузок: <b>") : F("</b>&emsp;Reboots: <b>"));
 content += String(counterOfReboots);
 content += F("</p><hr>");
 }
else
 {
 Dir dir = LittleFS.openDir("/");
 File f;
 size_t usedBytes = 0;
 String log_CurDate;
 while ( dir.next() )
  {
  if ( dir.fileSize() )
   {
   f = dir.openFile("r");
   if ( f ) { usedBytes += f.size(); f.close(); }
   yield();
   }
  } 
 log_MinDate = F("99999999");
 log_MaxDate = F("00000000");
 dir = LittleFS.openDir(LOG_DIR);
 while ( dir.next() )
  {
  if ( dir.fileSize() )
   {
   f = dir.openFile("r");
   if ( f ) 
    {
    usedBytes += f.size();
    log_CurDate = dir.fileName();
    if ( log_CurDate.compareTo(log_MaxDate) > 0 ) { log_MaxDate = log_CurDate; }
    if ( log_CurDate.compareTo(log_MinDate) < 0 ) { log_MinDate = log_CurDate; }
    f.close();
    }
   yield();
   }
  }
 content += (Language ? F("Журнал на ") : F("Log for "));
 content += String(log_Day) + F(".");
 if ( log_Month < 10 ) { content += F("0"); }
 content += String(log_Month) + F(".");
 content += String(log_Year) + F("</span><p>");
 content += (Language ? F("Файл журнала: <b>") : F("Log file: <b>"));
 content += String(log_FileSize);
 FSInfo fs_info;
 LittleFS.info(fs_info);
 content += (Language ? F("</b> Б&emsp;Всего: <b>") : F("</b> B&emsp;Total: <b>"));
 content += String(fs_info.totalBytes);
 content += (Language ? F("</b> Б&emsp;Занято: <b>") : F("</b> B&emsp;Used: <b>"));
 content += String(usedBytes);
 content += (Language ? F("</b> Б&emsp;") : F("</b> B&emsp;"));
 if ( fs_info.totalBytes - usedBytes <= 1024 ) { content += F("<font color='red'>"); }
 content += (Language ? F("Свободно: <b>") : F("Free: <b>"));
 content += String(fs_info.totalBytes - usedBytes);
 content += (Language ? F("</b> Б") : F("</b> B"));
 if ( fs_info.totalBytes - usedBytes <= 1024 ) { content += F("</font>"); }
 content += F("</b></p><p>");
 content += (Language ? F("Найдены журналы на даты: <b>") : F("Found logs for dates: <b>"));
 content += log_MinDate.substring(6,8) + F(".");
 content += log_MinDate.substring(4,6) + F(".");
 content += log_MinDate.substring(0,4) + F("<b> - </b>");
 content += log_MaxDate.substring(6,8) + F(".");
 content += log_MaxDate.substring(4,6) + F(".");
 content += log_MaxDate.substring(0,4);
 }
content += F("</b></p></div>");
server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
server.sendHeader("Pragma", "no-cache");
server.sendHeader("Expires", "-1");
server.setContentLength(CONTENT_LENGTH_UNKNOWN);
server.send(200, F("text/html; charset=utf-8"), "");
server.sendContent(content);
yield();
}

void drawLOG()
{
if ( !littleFS_OK ) { return; }   
drawHeader(false);
String content = F("<hr><table><tr>");
content += (Language ? F("<th>Дата и время</th>") : F("<th>Date&time</th>"));
content += (Language ? F("<th>Событие</th>") : F("<th>Event</th>"));
content += (Language ? F("<th>Задание</th>") : F("<th>Task</th>"));
content += (Language ? F("<th>&emsp;Канал</th>") : F("<th>&emsp;Channel</th>"));
content += (Language ? F("<th>&emsp;Состояние</th>") : F("<th>&emsp;State</th>"));
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
  while ( log_readRow(&rl, ls - 1) )
   { 
   content += F("<tr><td>");
   struct tm *ptm = gmtime((time_t *)&rl.utc); 
   content += String(ptm->tm_mday) + F(".") 
            + (ptm->tm_mon + 1 < 10 ? F("0") : F("")) + String(ptm->tm_mon + 1) + F(".") 
            + String(ptm->tm_year + 1900) + F("&emsp;")
            + (ptm->tm_hour < 10 ? F("0") : F("")) + String(ptm->tm_hour) + F(":") 
            + (ptm->tm_min  < 10 ? F("0") : F("")) + String(ptm->tm_min) + F(":") 
            + (ptm->tm_sec  < 10 ? F("0") : F("")) + String(ptm->tm_sec) + F("</td><td>&emsp;");
   content += (Language ? namesOfEventsR[rl.event] : namesOfEventsE[rl.event]);
   content += F("</td><td align='right'>");
   if ( rl.event > 0 )
    {
    content += (rl.event == 2 ? String(rl.task + 1) : F("&nbsp;"));
    content += F("</td><td align='right'>");
    //content += ((rl.ch + 1) < 10 ? F("&nbsp;&nbsp;") : F(""));
    content += String(rl.ch + 1);
    content += F("</td><td align='left'>&emsp;");
    if ( rl.act ) { content += (Language ? F("ВКЛ")  : F("ON")); } 
             else { content += (Language ? F("ВЫКЛ") : F("OFF")); }
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
if ( log_NumRecords > 0 )
 {
 content += (Language ? F("Записи с <b>") : F("Records from <b>"));
 content += String(ls + 1);
 content += (Language ? F("</b> по <b>") : F("</b> to <b>"));
 content += String(log_StartRecord);
 content += (Language ? F("</b> из <b>") : F("</b> of <b>"));
 content += String(log_NumRecords);
 }
else
 {
 content += (Language ? F("</b><i>Записи на указанную дату не найдены</i>") : F("</b><i>No records found for the specified date</i>"));
 }
content += F("</b></p><hr><form>");
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
if ( log_FileSizeForDate(log_ViewDate - SECS_PER_DAY) == 0 )
 { content += F("' disabled />"); } else { content += F("' />"); }
content += F("&emsp;<input formaction='/log_incdate' formmethod='get' type='submit' value='");
content += (Language ? F(" Следующий день ") : F(" Next Day "));
if ( log_FileSizeForDate(log_ViewDate + SECS_PER_DAY) == 0 )
 { content += F("' disabled />"); } else { content += F("' />"); }
content += F("&emsp;&emsp;<input formaction='/log_return' formmethod='get' type='submit' value='");
content += (Language ? F("На главную страницу") : F("To Home Page"));
content += F("' /></form>");
yield(); 
content += F("<hr>");
if ( log_MinDate != log_MaxDate )
 {
 content += F("<form action='/log_setdate'><p>");
 content += (Language ? F("Выберите дату") : F("Select a date"));
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
 }
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
content += F(":&emsp;<input name='dk' type='number' min='");
content += String(DAYSTOKEEP_MIN);
content += F("' max='");
content += String(DAYSTOKEEP_MAX);
content += F("' value='");
content += String(log_DaysToKeep) + F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить и перезагрузить") : F("Save and reboot"));
content += F("' /></p></form></body></html>\r\n");
yield();
server.sendContent(content);
server.sendContent(F("")); // transfer is done
server.client().stop();
}

void drawHomePage()
{
byte curChNum;
bool curTaskEnabled;
drawHeader(true);
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
   { content += ( Language ? F("red'><b>ВЫКЛЮЧЕН </font></b></td><td>(заданием)") : F("red'><b>OFF </font></b></td><td>(by task)") ); }
  else 
   { content += ( Language ? F("red'><b>ВЫКЛЮЧЕН </font></b></td><td>(вручную)") : F("red'><b>OFF </font></b></td><td>(manually)") ); }
  }
 else if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK )
  {
  onoff = "off";  
  if ( NumEnabledTasks[chNum] > 0 )
   { content += ( Language ? F("green'><b>ВКЛЮЧЕН </font></b></td><td>(заданием)") : F("green'><b>ON </font></b></td><td>(by task)") ); }
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
            || ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_MANUALLY ? F("Выключить") : F("Включить") ); 
  }
 else
  {
  content += ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK 
            || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 150 && ChannelList[chNum][CHANNEL_LASTSTATE] < 250)
            || ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_MANUALLY ? F("Set OFF") : F("Turn ON") ); 
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
 content += F("<hr><center><p><form method='get' form action='/viewlog'><input name='vwl' type='submit' value='&emsp;&emsp;&emsp;");
 content += (Language ? F("Просмотр журнала") : F("View log"));
 content += F("&emsp;&emsp;&emsp;' /></form></p></center>");
 }
content += F("<hr>");
server.sendContent(content); content = "";
// list tasks
curChNum = TaskList[0][TASK_CHANNEL];
curTaskEnabled = ( TaskList[0][TASK_ACTION] != ACTION_NOACTION );
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
 content += F("<form method='get' form action='/settask'><p>");
 if ( numberOfTasks >  9 && (taskNum + 1) <  10 ) { content += F("&nbsp;&nbsp;"); }
 if ( numberOfTasks > 99 && (taskNum + 1) < 100 ) { content += F("&nbsp;&nbsp;"); }
 content += String(taskNum + 1) + F(":</b>&nbsp;");
 // Channel
 content += (Language ? F("Канал") : F("Channel"));
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
 content += (Language ? F("Действие") : F("Action"));
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
 content += (Language ? F("День(дни)") : F("Day(s)"));
 content += F("&nbsp;<select name='d' size='1'><option ");
 for (int t = 0; t <= TASK_DAY_EVERYDAY; t++)
  {
  if ( TaskList[taskNum][TASK_DAY] == t ) { content += F("selected='selected' "); } 
  content += F("value='"); content += String(t) + F("'>") + (Language ? namesOfDaysR[t] : namesOfDaysE[t]) + F("</option>");       
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
content += F("<hr>");
// ChannelList
for ( int chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 content += ( ChannelList[chNum][CHANNEL_ENABLED] ? F("<font color='black'>") : F("<font color='dimgrey'>") );
 content += F("<form method='get' form action='/setchannelparams'><p>");
 if ( numberOfChannels > 9 && (chNum + 1) <  10 ) { content += F("&nbsp;&nbsp;"); }
 content += (Language ? F("Канал ") : F("Channel "));
 content += ( ChannelList[chNum][CHANNEL_ENABLED] ? F("<b>") : F("") );
 content += String(chNum + 1) + F(":</b>&nbsp;<select name='e");
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
content += (Language ? F("<i>&emsp;(для NodeMCU GPIO может быть от ") : F("<i>&emsp;(for NodeMCU GPIO can be from 0"));
content += (Language ? F(" до ") : F(" to "));
content += String(GPIO_MAX_NUMBER);
content += (Language ? F(", исключая 6,7,8 и 11)</i>") : F(", exclude 6,7,8 and 11)</i>"));
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
content += F("(-12...12):&emsp;<input name='tz' type='number' min='-11' max='12' value='");
content += String(ntpTimeZone) + F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить и перезагрузить") : F("Save and reboot"));
content += F("' /></p></form><form method='get' form action='/setlogin'><p>");
content += (Language ? F("Имя авторизации") : F("Login name"));
content += F(":&emsp;<input maxlength='10' name='ln' required size='10' type='text' value='");
content += loginName + F("' /></p><p>");
content += (Language ? F("Новый пароль") : F("New password"));
content += F(":&emsp;<input maxlength='10' name='np' required size='10' type='password' />&emsp;");
content += (Language ? F("Подтвердите новый пароль") : F("Confirm new password"));
content += F(":&emsp;<input maxlength='10' name='npc' required size='10' type='password' />&emsp;");
content += (Language ? F("Старый пароль") : F("Old password"));
content += F(":&emsp;<input maxlength='10' name='op' required size='10' type='password'");
content += F(" />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить") : F("Save"));
content += F("' /></form>");
content += F("<hr>");
// Settings files
// Save
struct tm *tinfo = gmtime(&log_today);
content += F("<p><form method='get' form action='/savesettings'>");
content += (Language ? F("Сохранить все настройки в файл") : F("Save all settings to file"));
content += F(":&emsp;<input maxlength='30' name='fn' size='30' type='text' pattern='[a-zA-Z0-9-_.]+' required value='");
content += String(1900 + tinfo->tm_year) + F("-")
        + ( tinfo->tm_mon + 1 < 10 ? F("0") : F("") ) + String(tinfo->tm_mon + 1) + F("-")
        + ( tinfo->tm_mday < 10 ? F("0") : F("") ) + String(tinfo->tm_mday) + F("-")
        + ( curTimeHour < 10 ? F("0") : F("") ) + String(curTimeHour) + F("-")
        + ( curTimeMin < 10 ? F("0") : F("") ) + String(curTimeMin) + F(".VTcfg");
content += F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить") : F("Save"));
content += F("' /></form></p>");
int numFiles = 0;
Dir dir = LittleFS.openDir("/");
while ( dir.next() )
 {
 if ( dir.isFile() )
  {
  if ( dir.fileSize() )
   {
   File f = dir.openFile("r");
   if ( f )
    {
    if ( String(f.name()) != "" && f.size() == (TASKLIST_EEPROM_ADDRESS + TASKLIST_MAX_NUMBER * TASK_NUM_ELEMENTS) )
     { numFiles++; }
    f.close();
    }
   }
  } 
 yield();
 }
if ( numFiles > 0 )
 {
 String arrFileNames[numFiles];
 dir.rewind();
 int fileNum = 0;
 do
  {
  if ( dir.fileName() != "" && dir.fileSize() == (TASKLIST_EEPROM_ADDRESS + TASKLIST_MAX_NUMBER * TASK_NUM_ELEMENTS) )
   { arrFileNames[fileNum] = dir.fileName(); fileNum++; }
  if ( fileNum >= numFiles ) { break; }
  yield();
  }
 while ( dir.next() ); 
 // Restore
 content += F("<form method='get' form action='/restoresettings' onsubmit='warn();return false;'>");
 content += (Language ? F("Восстановить настройки из файла") : F("Restore settings from file"));
 content += F(":&emsp;<select name='file' size='1'>");
 for ( int fileNum = 0; fileNum < numFiles; fileNum++ )
  {
  content += F("<option required value='"); 
  content += arrFileNames[fileNum] + F("'>") + arrFileNames[fileNum] + F("</option>");       
  yield();
  }
 content += F("</select>&emsp;<input type='submit' value='");
 content += (Language ? F("Восстановить (будьте осторожны!) и перезагрузить") : F("Restore (be careful!) and reboot"));
 content += F("' /></form>");
 // Rename
 content += F("<form method='get' form action='/renamefile'>");
 content += (Language ? F("Переименовать файл") : F("Rename settings file"));
 content += F(":&emsp;<select name='file' size='1'>");
 for ( int fileNum = 0; fileNum < numFiles; fileNum++ )
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
 content += F("' /></form>");
 // Delete
 content += F("<form method='get' form action='/deletefile' onsubmit='warn();return false;'>");
 content += (Language ? F("Удалить файл с настройками") : F("Delete settings file"));
 content += F(":&emsp;<select name='file' size='1'>");
 for ( int fileNum = 0; fileNum < numFiles; fileNum++ )
  {
  content += F("<option required value='"); 
  content += arrFileNames[fileNum] + F("'>") + arrFileNames[fileNum] + F("</option>");       
  yield();
  }
 content += F("</select>&emsp;<input type='submit' value='");
 content += (Language ? F("Удалить (будьте осторожны!)") : F("Delete (be careful!)"));
 content += F("' /></form>");
 // Download
 content += F("<form method='post' form action='/downloadfile'>");
 content += (Language ? F("Выгрузить файл с настройками") : F("Download settings file"));
 content += F(":&emsp;<select name='file' size='1'>");
 for ( int fileNum = 0; fileNum < numFiles; fileNum++ )
  {
  content += F("<option required value='"); 
  content += arrFileNames[fileNum] + F("'>") + arrFileNames[fileNum] + F("</option>");       
  yield();
  }
 content += F("</select>&emsp;<input type='submit' value='");
 content += (Language ? F("Выгрузить") : F("Download"));
 content += F("' /></form>");
 } // end of if ( numFiles > 0 )
// Upload
content += F("<form method='post' form action='/uploadfile' enctype='multipart/form-data'>");
content += (Language ? F("Загрузить файл с настройками") : F("Upload settings file"));
content += F(":&emsp;<input type='file' name='upload'><input type='submit' value='");
content += (Language ? F("Загрузить") : F("Upload"));
content += F("' /></form>");
// Actions
content += F("<hr>");
content += F("<form action='/firmware' form method='get'><input name='fwu' type='submit' value='");
content += (Language ? F("Обновление прошивки") : F("Firmware update"));
content += F("' /></form><form action='/cleartasklist' form method='get' onsubmit='warn();return false;'><input name='ctl' type='submit' value='");
content += (Language ? F("Очистить и отключить все задания (будьте осторожны!)") : F("Clear and disable all tasks (be careful!)"));
content += F("' /></form><form action='/reset' form method='get' onsubmit='warn();return false;'><input name='rst' type='submit' value='");
content += (Language ? F("Сброс всех настроек (будьте осторожны!)") : F("Reset to defaults (be careful!)"));
content += F("' /></form>");
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
   found = ( TaskList[taskNum][TASK_ACTION] == TaskList[findNUM][TASK_ACTION] ? taskNum           // duplicate
                                                                              : taskNum + 1000 ); // conflicting
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

void read_login_pass_from_EEPROM()
{
loginName = "";
loginPass = "";
if ( EEPROM.read(LOGIN_NAME_PASS_EEPROM_ADDRESS + 22) != 0 )
 {
 loginName = "admin";
 loginPass = "admin";
 }
else
 { 
 for ( int i = LOGIN_NAME_PASS_EEPROM_ADDRESS;      i < (LOGIN_NAME_PASS_EEPROM_ADDRESS + 11); ++i) { if ( EEPROM.read(i) != 0 ) loginName += char(EEPROM.read(i)); }
 for ( int i = LOGIN_NAME_PASS_EEPROM_ADDRESS + 11; i < (LOGIN_NAME_PASS_EEPROM_ADDRESS + 22); ++i) { if ( EEPROM.read(i) != 0 ) loginPass += char(EEPROM.read(i)); }
 if ( loginName == "" || loginPass == "" )
  {
  loginName = "admin";
  loginPass = "admin";
  }
 }
httpUpdater.setup(&server, "/firmware", loginName, loginPass);
}

byte find_next_tasks_within_day(int findChannel, int findDOW, int hh, int mm, int ss)
{
int taskNum;  
byte found = TASKLIST_MAX_NUMBER;
if ( findDOW != curDayOfWeek ) { hh = 0; mm = 0; ss = 0; }
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
if ( !thereAreEnabledTasks ) { return; }  
int findDOW, chNum;
byte foundTask, DaysCounter;
for ( chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 if ( NumEnabledTasks[chNum] == 0 ) { continue; }
 findDOW = curDayOfWeek;
 foundTask = TASKLIST_MAX_NUMBER;  
 DaysCounter = 0;
 while ( DaysCounter < 7 ) 
  {
  foundTask = find_next_tasks_within_day(chNum, findDOW, curTimeHour, curTimeMin, curTimeSec);
  if ( foundTask < TASKLIST_MAX_NUMBER ) { break; }
  DaysCounter++;
  if ( findDOW == 6 ) { findDOW = 0; } else { findDOW++; }
  }
 NextTasksList[chNum] = ( foundTask < TASKLIST_MAX_NUMBER ? foundTask : 255 ); 
 }
}

byte check_previous_tasks_within_day(int findChannel, int findDOW, int hh, int mm, int ss)
{
int taskNum;  
byte found = TASKLIST_MAX_NUMBER;
if ( findDOW != curDayOfWeek ) { hh = 23; mm = 59; ss = 95; }
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
if ( !thereAreEnabledTasks ) { return; }  
bool needSave = false;  
int findDOW, chNum;
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
 findDOW = curDayOfWeek;
 foundTask = TASKLIST_MAX_NUMBER;  
 DaysCounter = 0;
 while ( DaysCounter < 7 ) 
  {
  foundTask = check_previous_tasks_within_day(chNum, findDOW, curTimeHour, curTimeMin, curTimeSec);
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
   log_Append(EVENT_TASK_SWITCHING, (uint8_t)chNum, foundTask, (uint8_t)ChannelList[chNum][CHANNEL_LASTSTATE]);
   needSave = true;
   }
  else if ( ChannelList[chNum][CHANNEL_LASTSTATE] == LASTSTATE_ON_BY_TASK && TaskList[foundTask][TASK_ACTION] == ACTION_TURN_OFF )    
   {   
   ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_OFF_BY_TASK;
   log_Append(EVENT_TASK_SWITCHING, (uint8_t)chNum, foundTask, (uint8_t)ChannelList[chNum][CHANNEL_LASTSTATE]);
   needSave = true;
   }
  else if ( (ChannelList[chNum][CHANNEL_LASTSTATE] >= 50 && ChannelList[chNum][CHANNEL_LASTSTATE] < 150
           && ActiveNowTasksList[chNum] != ChannelList[chNum][CHANNEL_LASTSTATE] - 50)
         || (ChannelList[chNum][CHANNEL_LASTSTATE] >= 150 && ChannelList[chNum][CHANNEL_LASTSTATE] < 250
           && ActiveNowTasksList[chNum] != ChannelList[chNum][CHANNEL_LASTSTATE] - 150)
          ) 
   {
        if ( TaskList[foundTask][TASK_ACTION] == ACTION_TURN_ON )  { ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_ON_BY_TASK; }
   else if ( TaskList[foundTask][TASK_ACTION] == ACTION_TURN_OFF ) { ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_OFF_BY_TASK; }
   log_Append(EVENT_TASK_SWITCHING, (uint8_t)chNum, foundTask, (uint8_t)ChannelList[chNum][CHANNEL_LASTSTATE]);
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
for ( taskNum = 0; taskNum < numberOfTasks; taskNum++ )
 {
 taskAddress = TASKLIST_EEPROM_ADDRESS + taskNum * TASK_NUM_ELEMENTS;
 yield(); 
 for ( task_element_num = 0; task_element_num < TASK_NUM_ELEMENTS; task_element_num++ ) 
  {
  TaskListRAW[taskNum][task_element_num] = EEPROM.read(taskAddress + task_element_num);
  }
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
    > (( TaskListRAW[IndexArray[bubble_num+1]][TASK_ACTION] != ACTION_NOACTION ? 0 : 1 + 86401 * CHANNELLIST_MAX_NUMBER)
       + TaskListRAW[IndexArray[bubble_num+1]][TASK_CHANNEL] * 86401
       + TaskListRAW[IndexArray[bubble_num+1]][TASK_HOUR] * 3600 
       + TaskListRAW[IndexArray[bubble_num+1]][TASK_MIN] * 60 
       + TaskListRAW[IndexArray[bubble_num+1]][TASK_SEC]))  
   { 
   IndexArray[bubble_num]^= IndexArray[bubble_num + 1];
   IndexArray[bubble_num+1]^= IndexArray[bubble_num];
   IndexArray[bubble_num]^= IndexArray[bubble_num + 1];
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

void save_tasks_to_EEPROM()
{
int taskAddress;
#ifdef DEBUG 
 Serial.println(F("Saving tasks"));
#endif
for ( int taskNum = 0; taskNum < numberOfTasks; taskNum++ )
 {
 taskAddress = TASKLIST_EEPROM_ADDRESS + taskNum * TASK_NUM_ELEMENTS;
 yield(); 
 for ( int task_element_num = 0; task_element_num < TASK_NUM_ELEMENTS; task_element_num++ )
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
 Serial.println(F("Reading channels and switch channels if needed"));
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
 drawHomePage(); 
 });
server.on("/settask", []() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 int taskNumber, param;   
 String buf;
 bool needSave = false;
 if ( server.args() != TASK_NUM_ELEMENTS ) { return; }
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
 if ( needSave ) { save_tasks_to_EEPROM(); check_previous_tasks(); find_next_tasks(); }
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
 save_tasks_to_EEPROM(); 
 ServerSendMessageAndRefresh();
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
 ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_ON_MANUALLY;
 log_Append(EVENT_MANUAL_SWITCHING, chNum, 0, 1);
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
 ChannelList[chNum][CHANNEL_LASTSTATE] = LASTSTATE_OFF_MANUALLY;
 log_Append(EVENT_MANUAL_SWITCHING, chNum, 0, 0);
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
  ChannelList[chNum][CHANNEL_LASTSTATE] = ActiveNowTasksList[chNum] + 150;
  log_Append(EVENT_MANUAL_SWITCHING, chNum, 0, 1);
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
  ChannelList[chNum][CHANNEL_LASTSTATE] = ActiveNowTasksList[chNum] + 50;
  log_Append(EVENT_MANUAL_SWITCHING, chNum, 0, 0);
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
  { if ( Language != param ) 
     {
     Language = ( param == 10 ? 0 : 1 ); 
     EEPROM.write(LANGUAGE_EEPROM_ADDRESS, Language);
     EEPROM.commit();
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
  timeClient.setTimeOffset(ntpTimeZone * 3600);
  EEPROM.write(NTP_TIME_ZONE_EEPROM_ADDRESS, ntpTimeZone + 12); EEPROM.commit();
  ServerSendMessageAndReboot();
  }
 ServerSendMessageAndRefresh();
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
  for ( int i = LOGIN_NAME_PASS_EEPROM_ADDRESS; i <= (LOGIN_NAME_PASS_EEPROM_ADDRESS + 22); ++i ) { EEPROM.write(i, 0); } // fill with 0
  for ( unsigned int i = 0; i < qlogin.length(); ++i ) { EEPROM.write(i + LOGIN_NAME_PASS_EEPROM_ADDRESS, qlogin[i]); }
  for ( unsigned int i = 0; i < qpass1.length(); ++i ) { EEPROM.write(i + LOGIN_NAME_PASS_EEPROM_ADDRESS + 11, qpass1[i]); }
  EEPROM.commit();
  read_login_pass_from_EEPROM();
  ServerSendMessageAndRefresh();
  }
 else { ServerSendMessageAndRefresh( 3, "/", (Language ? F("Неверные данные или пароли не совпадают...") : F("Incorrect data or passwords do not match...")) ); }
 });  
server.on("/savesettings", []() 
 {
 if ( !littleFS_OK ) { return; }
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String fileName = server.arg(0);
 File f = LittleFS.open(fileName, "w");
 if ( f ) 
  {
  for ( int i = 0; i < (TASKLIST_EEPROM_ADDRESS + TASKLIST_MAX_NUMBER * TASK_NUM_ELEMENTS); i++ )
   { 
   uint8_t b = EEPROM.read(i);
   f.write((uint8_t *)&b, sizeof(b));
   }   
  f.close();
  ServerSendMessageAndRefresh( 3, "/", (Language ? F("Сохранено в файл ") : F("Saved to file ")), fileName );
  }
 else { ServerSendMessageAndRefresh( 3, "/", (Language ? F("ОШИБКА !") : F("ERROR !")) ); }
 });  
server.on("/restoresettings", []() 
 {
 if ( !littleFS_OK ) { return; }
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
    if ( i >= (TASKLIST_EEPROM_ADDRESS + TASKLIST_MAX_NUMBER * TASK_NUM_ELEMENTS) ) { break; }
    }   
   f.close();
   EEPROM.commit();
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
 if ( !littleFS_OK ) { return; }
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String oldName = server.arg(0);
 String newName = server.arg(1);
 if ( LittleFS.rename(oldName, newName) )
  { ServerSendMessageAndRefresh( 3, "/", (Language ? F("Переименован файл ") : F("Renamed file ")), oldName, (Language ? F(" в ") : F(" to ")), newName); }
 else 
  { ServerSendMessageAndRefresh( 3, "/", (Language ? F("ОШИБКА !") : F("ERROR !")) ); }
 });  
server.on("/deletefile", []() 
 {
 if ( !littleFS_OK ) { return; }
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String fileName = server.arg(0);
 if ( LittleFS.exists(fileName) ) 
  {
  LittleFS.remove(fileName);
  ServerSendMessageAndRefresh( 3, "/", (Language ? F("Удален файл ") : F("Deleted file ")), fileName );
  }
 else { ServerSendMessageAndRefresh( 3, "/", (Language ? F("ОШИБКА !") : F("ERROR !")) ); }
 });  
server.on("/downloadfile", HTTP_POST, []()
 {
 if ( !littleFS_OK ) { return; }
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 String fileName = server.arg(0);
 if ( LittleFS.exists(fileName) ) 
  {
  File downloadHandle = LittleFS.open("/" + fileName, "r");
  if ( downloadHandle )
   {
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
 ServerSendMessageAndRefresh( 3, "/", (Language ? F("Файл загружен") : F("File downloaded") ));
 });
server.on("/reset", []()  
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 EEPROMWriteInt(FIRST_RUN_SIGNATURE_EEPROM_ADDRESS, 0); 
 ServerSendMessageAndReboot();
 }); 
server.on("/restart", []()  
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 ServerSendMessageAndReboot();
 }); 
server.on("/viewlog", []()
 { 
 log_ViewDate = curEpochTime;
 log_CalcForViewDate();
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 }); 
server.on("/log",[]() 
 {
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 drawLOG(); 
 });
server.on("/log_first", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_StartRecord = log_NumRecords; 
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_previous", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_StartRecord += log_ViewStep; 
 if ( log_StartRecord > log_NumRecords ) { log_StartRecord = log_NumRecords; }
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_next", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_StartRecord -= log_ViewStep; 
 if ( log_StartRecord < 1 ) { log_StartRecord = log_ViewStep; }
 if ( log_StartRecord > log_NumRecords ) { log_StartRecord = log_NumRecords; }
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_last", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_StartRecord = log_ViewStep; 
 if ( log_StartRecord > log_NumRecords ) { log_StartRecord = log_NumRecords; }
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_incdate", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_ViewDate += SECS_PER_DAY;
 log_CalcForViewDate();
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_decdate", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
 log_ViewDate -= SECS_PER_DAY;
 log_CalcForViewDate();
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
 ServerSendMessageAndRefresh( 0, LOG_DIR );
 });
server.on("/log_return", []()
 { 
 if ( !server.authenticate(loginName.c_str(), loginPass.c_str()) ) { return server.requestAuthentication(); }
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
  ServerSendMessageAndRefresh( 0, LOG_DIR );
  }
 });
}

void setup()
{
#ifdef DEBUG 
 Serial.begin(9600, SERIAL_8N1, SERIAL_TX_ONLY);
 Serial.println();
 Serial.println(F("Versatile timer started"));
 Serial.println(F("Inizializing LittleFS..."));
#endif 
littleFS_OK = LittleFS.begin();
#ifdef DEBUG 
 if ( littleFS_OK ) { Serial.println(F("done.")); } else { Serial.println(F("FAIL!")); }
#endif 
EEPROM.begin(TASKLIST_EEPROM_ADDRESS);
// Check signature to verify the first run on the device and prepare EEPROM
if ( EEPROMReadInt(FIRST_RUN_SIGNATURE_EEPROM_ADDRESS) != FIRST_RUN_SIGNATURE ) 
 {
 #ifdef DEBUG 
  Serial.println(F("First run on the device detected."));
  Serial.print(F("Preparing EEPROM addresses from 0 to "));
  Serial.println(TASKLIST_EEPROM_ADDRESS + TASKLIST_MAX_NUMBER * TASK_NUM_ELEMENTS);
 #endif 
 WiFi.disconnect(true);
 EEPROM.end();
 EEPROM.begin(TASKLIST_EEPROM_ADDRESS + TASKLIST_MAX_NUMBER * TASK_NUM_ELEMENTS);
 for ( int i = 0; i < (TASKLIST_EEPROM_ADDRESS + TASKLIST_MAX_NUMBER * TASK_NUM_ELEMENTS); i++ ) { EEPROM.write(i, 0); }
 EEPROMWriteInt(FIRST_RUN_SIGNATURE_EEPROM_ADDRESS, FIRST_RUN_SIGNATURE); 
 EEPROM.write(LOG_DAYSTOKEEP_EEPROM_ADDRESS, DAYSTOKEEP_DEF);
 EEPROM.write(LOG_VIEWSTEP_EEPROM_ADDRESS, LOG_VIEWSTEP_DEF);
 EEPROM.write(NTP_TIME_ZONE_EEPROM_ADDRESS, NTP_DEFAULT_TIME_ZONE + 12);
 EEPROM.commit();
 EEPROM.end();
 #ifdef DEBUG 
  Serial.println(F("Formatting LittleFS"));
 #endif   
 LittleFS.format();
 #ifdef DEBUG 
  Serial.println(F("Rebooting..."));
  delay(5000);
 #endif 
 ESP.restart();
 }
Language = EEPROM.read(LANGUAGE_EEPROM_ADDRESS);
if ( Language != 0 && Language != 1 ) { Language = 0; }
log_DaysToKeep = EEPROM.read(LOG_DAYSTOKEEP_EEPROM_ADDRESS);
if ( log_DaysToKeep > DAYSTOKEEP_MAX || log_DaysToKeep < DAYSTOKEEP_MIN ) { log_DaysToKeep = DAYSTOKEEP_DEF; }
log_ViewStep = EEPROM.read(LOG_VIEWSTEP_EEPROM_ADDRESS);
if ( log_ViewStep > LOG_VIEWSTEP_MAX || log_ViewStep < LOG_VIEWSTEP_MIN ) { log_ViewStep = LOG_VIEWSTEP_DEF; }
ntpTimeZone = EEPROM.read(NTP_TIME_ZONE_EEPROM_ADDRESS);
ntpTimeZone = ntpTimeZone - 12;
if ( ntpTimeZone > 24 ) { ntpTimeZone = NTP_DEFAULT_TIME_ZONE; }
numberOfTasks = EEPROM.read(NUMBER_OF_TASKS_EEPROM_ADDRESS);
if ( numberOfTasks < TASKLIST_MIN_NUMBER || numberOfTasks > TASKLIST_MAX_NUMBER ) { numberOfTasks = 5; }
TaskList = new uint8_t*[numberOfTasks];
for ( int taskNum = 0; taskNum < numberOfTasks; taskNum++ ) { yield(); TaskList[taskNum] = new uint8_t[TASK_NUM_ELEMENTS]; }
numberOfChannels = EEPROM.read(NUMBER_OF_CHANNELS_EEPROM_ADDRESS);
if ( numberOfChannels < CHANNELLIST_MIN_NUMBER || numberOfChannels > CHANNELLIST_MAX_NUMBER ) { numberOfChannels = CHANNELLIST_MIN_NUMBER; }
EEPROM.end();
ChannelList = new uint8_t*[numberOfChannels];
for ( int chNum = 0; chNum < numberOfChannels; chNum++ ) 
 {
 yield(); 
 ChannelList[chNum] = new uint8_t[CHANNEL_NUM_ELEMENTS];
 ActiveNowTasksList[chNum] = 255;
 NextTasksList[chNum] = 255;
 NumEnabledTasks[chNum] = 0;
 }
EEPROM.begin(TASKLIST_EEPROM_ADDRESS + numberOfTasks * TASK_NUM_ELEMENTS);
read_login_pass_from_EEPROM();
read_channellist_from_EEPROM_and_switch_channels();
read_and_sort_tasklist_from_EEPROM();
//
#ifdef USE_FIXED_IP_ADDRESSES 
 if ( !WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS) )
  {
  #ifdef DEBUG
   Serial.println(F("Failed to configure fixed IP addresses."));
   while ( true ) { yield(); }
  #endif 
  }
#endif
WiFi.mode(WIFI_STA);
WiFi.begin(AP_SSID, AP_PASS);
delay(500);
int ct = 60;
#ifdef DEBUG 
 Serial.print(F("Connecting to "));
 Serial.println(AP_SSID);
#endif
while ( WiFi.status() != WL_CONNECTED && (ct > 0) )
 {
 yield(); delay(500); ct--;
 #ifdef DEBUG 
  Serial.print(F("."));
 #endif
 }
#ifdef DEBUG 
 Serial.println();
#endif
statusWiFi = ( WiFi.status() == WL_CONNECTED );
#ifdef DEBUG 
 if ( statusWiFi ) 
  {
  Serial.print(F("Connected to ")); Serial.println(AP_SSID);
  Serial.print(F("MAC address ")); Serial.println(WiFi.macAddress());
  Serial.print(F("Server can be accessed at http://")); Serial.println(WiFi.localIP());
  Serial.print(F("                    or at http://")); Serial.print(MDNSHOST); Serial.println(F(".local"));
  }
 else { Serial.println(F("WiFi NOT connected now.")); }
#endif
if ( !WiFi.getAutoConnect() ) WiFi.setAutoConnect(true);
WiFi.setAutoReconnect(true);
WiFi.persistent(true);
//
counterOfReboots = EEPROMReadInt(COUNTER_OF_REBOOTS_EEPROM_ADDRESS);
if ( counterOfReboots < 0 || counterOfReboots > 32766 ) { counterOfReboots = 0; }
counterOfReboots++;
EEPROMWriteInt(COUNTER_OF_REBOOTS_EEPROM_ADDRESS, counterOfReboots); 
//
timeClient.begin();
timeClient.setTimeOffset(ntpTimeZone * 3600);
setupServer();
server.begin();
if ( MDNS.begin(MDNSHOST) ) {  MDNS.addService("http", "tcp", 80); }
ESP.wdtEnable(WDTO_8S);
everySecondTimer = millis();
CheckTaskListTimer = millis();
}

void loop()
{
yield(); delay(0);  
statusWiFi = ( WiFi.status() == WL_CONNECTED );  
if ( statusWiFi ) 
 {
 server.handleClient();
 MDNS.update();
 timeSyncOK = timeClient.update(); 
 } 
else { timeSyncOK = false; }
if ( timeSyncOK )
 {
 unsigned long difference = 0;
 if ( timeSyncInitially )
  {
  difference = ( curEpochTime > timeClient.getEpochTime() ? (curEpochTime - timeClient.getEpochTime()) 
                                                          : (timeClient.getEpochTime() - curEpochTime) );
  }
 if ( difference < EPOCHTIMEMAXDIFF ) // check the correctness of the received time relative to the set
  { 
  curEpochTime = timeClient.getEpochTime();  
  curTimeHour  = timeClient.getHours();
  curTimeMin   = timeClient.getMinutes();
  curTimeSec   = timeClient.getSeconds();
  curDayOfWeek = timeClient.getDay(); // 0 - Sunday, 1...5 - Monday...Friday, 6 - Saturday
  if ( !timeSyncInitially ) 
   {
   check_previous_tasks(); 
   find_next_tasks();
   timeClient.setUpdateInterval(NTPUPDATEINTERVAL);
   timeSyncInitially = true;
   if ( littleFS_OK ) { log_process(); }
   log_Append(EVENT_START,0,0,0);
   }
  }
 } 
//
if ( millis() - everySecondTimer > 1000 )
 {
 setClockcurrentMillis = millis();  
 if ( (!timeSyncOK) && timeSyncInitially )
  {
  // Update the time when there is no network connection 
  if ( setClockcurrentMillis < setClockpreviousMillis ) { setClockcurrentMillis = setClockpreviousMillis; }
  setClockelapsedMillis += (setClockcurrentMillis - setClockpreviousMillis);
  while ( setClockelapsedMillis > 999 )
   {
   curTimeSec++;
   curEpochTime++;
   if ( curTimeSec > 59 ) { curTimeMin++;  curTimeSec = 0; }
   if ( curTimeMin > 59 ) { curTimeHour++; curTimeMin = 0; }
   if ( curTimeHour > 23 ) 
    {
    curTimeHour = 0;
    curDayOfWeek++;
    if ( curDayOfWeek > 6 ) { curDayOfWeek = 0; } // 0 - Sunday, 1 - Monday, 6 - Saturday
    }
   setClockelapsedMillis -= 1000;
   }
  timeSyncOK = true; 
  } 
 if ( littleFS_OK && timeSyncInitially ) { log_process(); }
 setClockpreviousMillis = setClockcurrentMillis; 
 everySecondTimer = millis(); 
 } 
//
if ( millis() - CheckTaskListTimer > 500 )
 {
 if ( timeSyncOK ) { check_previous_tasks(); find_next_tasks(); } 
 CheckTaskListTimer = millis();
 }
}
