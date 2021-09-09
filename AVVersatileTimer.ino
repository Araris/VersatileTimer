// Tested on Arduino 1.8.8
// Tested on NodeMCU 1.0 (ESP-12E Module) and on Generic ESP8266 Module

//#define DEBUG 1

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <NTPClient.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
//
// Create a Secrets.h file with Wi-Fi connection settings as follows:
//
//   #define AP_SSID   "yourSSID"
//   #define AP_PASS   "yourPASSWORD"
//
#include "Secrets.h"
//
#define FIRST_RUN_SIGNATURE_EEPROM_ADDRESS         0 // int FIRST_RUN_SIGNATURE
#define CHANNELS_CONTROLS_MODE_EEPROM_ADDRESS      3 // byte ChannelsControlsMode
#define LANGUAGE_EEPROM_ADDRESS                    4 // boolean Language
#define COUNTER_OF_REBOOTS_EEPROM_ADDRESS          8 // int counterOfReboots
#define NUMBER_OF_TASKS_EEPROM_ADDRESS            10 // byte numberOfTasks
#define NTP_TIME_ZONE_EEPROM_ADDRESS              11 // byte (int ntpTimeZone)
#define NUMBER_OF_CHANNELS_EEPROM_ADDRESS         12 // byte numberOfChannels
#define LOGIN_NAME_PASS_EEPROM_ADDRESS            16 // (16-26) String loginName from LOGIN_NAME_PASS_EEPROM_ADDRESS to LOGIN_NAME_PASS_EEPROM_ADDRESS + 10
                                                     // (27-39) String loginPass from LOGIN_NAME_PASS_EEPROM_ADDRESS + 11 to LOGIN_NAME_PASS_EEPROM_ADDRESS + 22
#define CHANNELLIST_EEPROM_ADDRESS                40 // byte ChannelList from CHANNELLIST_EEPROM_ADDRESS to CHANNELLIST_EEPROM_ADDRESS + CHANNELLIST_MAX_NUMBER * CHANNEL_NUM_ELEMENTS
#define TASKLIST_EEPROM_ADDRESS                  100 // byte TaskList from TASKLIST_EEPROM_ADDRESS to TASKLIST_EEPROM_ADDRESS + numberOfTasks * TASK_NUM_ELEMENTS
//
#define FIRST_RUN_SIGNATURE                      139 // two-byte signature to verify the first run on the device and prepare EEPROM
#define MDNSID                                  "VT" // mDNS host
#define CHANNELS_CONTROLS_ONLY_MANUALLY           10
#define CHANNELS_CONTROLS_MANUALLY_AND_BY_TASKS   11
#define CHANNELS_CONTROLS_ONLY_BY_TASKS           12
#define GPIO_MAX_NUMBER                           16
#define CHANNELLIST_MIN_NUMBER                     1
#define CHANNELLIST_MAX_NUMBER                    15
#define CHANNEL_NUM_ELEMENTS                       4
#define CHANNEL_GPIO                               0
#define CHANNEL_INVERTED                           1
#define CHANNEL_LASTSTATE                          2
#define CHANNEL_ENABLED                            3
#define TASKLIST_MIN_NUMBER                        1
#define TASKLIST_MAX_NUMBER                      100
#define TASK_NUM_ELEMENTS                          6
#define TASK_ACTION                                0
#define TASK_HOUR                                  1
#define TASK_MIN                                   2
#define TASK_SEC                                   3
#define TASK_DAY                                   4
#define TASK_CHANNEL                               5
#define ACTION_NOACTION                            0
#define ACTION_TURN_OFF                           10
#define ACTION_TURN_ON                            11 
//                    GPIO      0     1    2    3    4    5     6     7     8     9    10    11   12   13   14   15   16
const String NodeMCUpins[] = {"D3","D10","D4","D9","D2","D1","N/A","N/A","N/A","D11","D12","N/A","D6","D7","D5","D8","D0"};
const String namesOfDaysE[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Workdays","Weekends","Every day"};
const String namesOfDaysR[] = {"Воскресенье","Понедельник","Вторник","Среда","Четверг","Пятница","Суббота","Рабочие дни","Выходные дни","Каждый день"};
//
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0);
//
String loginName; // default "admin"
String loginPass; // default "admin"
byte ChannelsControlsMode; // 10 - only manually, 11 - by tasks and manualy, 12 - only by tasks
byte numberOfTasks; // TASKLIST_MIN_NUMBER...TASKLIST_MAX_NUMBER
bool thereAreEnabledTasks = false;
uint8_t** TaskList;
// uint8_t TaskList[numberOfTasks][TASK_NUM_ELEMENTS]
                                // 0 - action - 0 no action, 10 off, 11 on
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
byte NumEnabledTasks[CHANNELLIST_MAX_NUMBER];                         
int counterOfReboots = 0; 
int ntpTimeZone = 0;      // -12...12
boolean Language = false; // false - English, true - Russian
int curTimeHour = 0;      // 0...23
int curTimeMin = 0;       // 0...59
int curTimeSec = 0;       // 0...59
int curDayOfWeek = 0;     // 0 - Sunday, 1...5 - Monday...Friday, 6 - Saturday
int previousSecond = 0;
bool statusWiFi = false;
bool timeSyncOK = false;
bool timeSyncInitially = false;
unsigned long everySecondTimer = 0;
unsigned long setClockcurrentMillis, setClockpreviousMillis, setClockelapsedMillis; 
                               
/////////////////////////////////////////////////////////

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

void drawHomePage()
{
byte curChNum;
bool curTaskEnabled;
String content = F("<link rel='shortcut icon' href='data:image/x-icon;base64,AAABAAEAEBAAAAEAIABoBAAAFgAAACgAAAAQAAAAIAAAAAEAIAAAAAAAQAQAAAAAAAAAAAAAAAAAAAAAAADCt6pgwrep/cK4qv/CuKr/wriq/8K4qv/CuKr/wriq/8K4qv/CuKr/wriq/8K4qv/CuKr/wriq/8K3qf3DuaxiwrmqisK4qv/CuKr/wriq/8K4qv/CuKr/vrSm/7WrnP+1q5z/vrSm/8K4qv/CuKr/wriq/8K4qv/CuKr/w7ipjMK5qorCuKr/wriq/8K4qv+zqZr/kYd4/5CHev+ln5T/pZ+U/5CHev+Rh3j/s6ma/8K4qv/CuKr/wriq/8O4qYzCuaqKwriq/8K4qv+jmYv/lo6C/9/d2f//////t7Wz/7e1s///////393Z/5aOgv+jmYv/wriq/8K4qv/DuKmMwrmqisK4qv+qoJH/npaL/6Cem//8/Pv//////9/f3v/f397///////z8+/+gnpv/npaL/6qgkf/CuKr/w7ipjMK5qoq/tab/i4Jz//Py8P+fnZr/7ezs///////////////////////t7Ov/n52a//Py8P+LgnP/v7Wm/8O4qYzCuaqKqZ+R/7awqP//////////////////////////////////////////////////////trCo/6mfkf/DuKmMwrmqipqQgv/X1M///////////////////////////////////////////////////////9fUz/+akIL/w7ipjMK5qoqWjH3/tbKu/1tYU///////+fj+/7ev8v9oVuP/xb70/////////////////1tYU/+1sq7/lox9/8K5q4vCt6t5m5GD/9XSzf/9/f7/npPt/1xI4f+RhOv/4d75////////////////////////////1dLN/5uRg//Ct6t5wrqrQ6uhk/+xq6H//fz+/7mx8v/z8v3//////////////////////////////////////7Grof+roZP/wrqrQ7+/vwS/tqfiioFy/+3s6f+IhoL/6urp///////////////////////q6un/iIaC/+3s6f+KgXL/v7an4r+/vwQAAAAAw7iqXa+llv+WjoL/sa+s//7+/v//////1dTT/9XU0////////v7+/7GvrP+WjoL/r6WW/8O4ql0AAAAAAAAAAAAAAADDuauYqqCS/4+Gef/Rzsn/+/v7/8HAvv/BwL7/+/v7/9HOyf+Phnn/qqCS/8O5q5gAAAAAAAAAAAAAAAAAAAAAgICAAsG3qYC5rqD6mY+A/4l/cf+Xj4P/l4+D/4l/cf+Zj4D/ua6g+sG3qYCAgIACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAw7WoJsO5q5HBuKrXvLKk+byypPnBuKrXw7mrkcO1qCYAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==' />");
content += F("<html><body style='background: #87CEFA'><span style='font-size:22pt'>");
content += (Language ? F("Универсальный программируемый таймер</span><p>Текущее время: <b>") : F("VERSATILE TIMER</span><p>Time: <b>"));
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
if ( days > 0 ) { content += String(days) + (Language ? F("д ") : F("d ")); } 
if ( hours > 0 ) { content += String(hours) + (Language ? F("ч ") : F("h ")); }
if ( mins > 0 ) { content += String(mins) + (Language ? F("м ") : F("m ")); }
content += String(secs) + (Language ? F("с ") : F("s "));
content += (Language ? F("</b>&emsp;Версия: <b>") : F("</b>&emsp;Version: <b>"));
content += String(__DATE__);
content += (Language ? F("</b>&emsp;Количество перезагрузок: <b>") : F("</b>&emsp;Number of reboots: <b>"));
content += String(counterOfReboots);
content += F("</b></p><hr />");
server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
server.sendHeader("Pragma", "no-cache");
server.sendHeader("Expires", "-1");
server.setContentLength(CONTENT_LENGTH_UNKNOWN);
server.send(200, "text/html; charset=utf-8", "");
server.sendContent(content); content = "";
for ( int chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 if ( ChannelsControlsMode != CHANNELS_CONTROLS_ONLY_BY_TASKS || !ChannelList[chNum][CHANNEL_ENABLED] )
  { content += F("<form method='get' form action='/setchannelstate'>"); }
 content += ( ChannelList[chNum][CHANNEL_ENABLED] ? F("<font color='black'><b>") : F("<font color='dimgrey'>") );
 content += (Language ? F("<p>Канал ") : F("<p>Channel "));
 content += String(chNum + 1);
 content += (Language ? F(" </b><span style='color: ") : F(" </b>is <span style='color: "));
 if ( Language ) { content += ( ChannelList[chNum][CHANNEL_LASTSTATE] ? F("green; '><b>ВКЛЮЧЕН</b></span>") : F("red; '><b>ВЫКЛЮЧЕН</b></span>") ); }
            else { content += ( ChannelList[chNum][CHANNEL_LASTSTATE] ? F("green; '><b>ON</b></span>") : F("red; '><b>OFF</b></span>") ); }
 if ( ChannelsControlsMode != CHANNELS_CONTROLS_ONLY_BY_TASKS || !ChannelList[chNum][CHANNEL_ENABLED] )
  {
  content += F("&emsp;<input name='s");   
  if ( chNum < 10 ) { content += F("0"); }
  content += String(chNum) + F("' type='submit' value='");   
  if ( Language ) { content += ( ChannelList[chNum][CHANNEL_LASTSTATE] ? F(" Выключить '/>") : F(" Включить '/>") ); }
             else { content += ( ChannelList[chNum][CHANNEL_LASTSTATE] ? F(" Turn OFF '/>") : F(" Turn ON '/>") ); }
  if ( ChannelsControlsMode != CHANNELS_CONTROLS_ONLY_MANUALLY && ChannelList[chNum][CHANNEL_ENABLED] )
   { content += F("<font color='darkblue'>&emsp;"); 
     content += (Language ? F("количество заданий: ") : F("number of tasks: "));
     content += String(NumEnabledTasks[chNum]); }
  content += F("</form>");
  }
 else if ( ChannelList[chNum][CHANNEL_ENABLED] ) 
       { content += F("<font color='darkblue'>&emsp;");
         content += (Language ? F("количество заданий: ") : F("number of tasks: "));
         content += String(NumEnabledTasks[chNum]); }
 content += F("</font></p>"); 
 }
content += F("<hr /><form method='get' form action='/ChannelsControlsMode'><p>");
content += (Language ? F("Режим управления каналами:") : F("Channels controls mode:"));
content += F("&emsp;<select name='cm' size='1'><option ");
if ( ChannelsControlsMode == CHANNELS_CONTROLS_ONLY_MANUALLY ) 
 {
 content += F("selected='selected' value='10'>"); 
 content += (Language ? F("только ручное") : F("only manually"));
 content += F("</option><option value='11'>"); 
 content += (Language ? F("по заданиям и вручную") : F("by tasks and manualy"));
 content += F("</option><option "); 
 }
if ( ChannelsControlsMode == CHANNELS_CONTROLS_MANUALLY_AND_BY_TASKS )
 { 
 content += F("value='10'>"); 
 content += (Language ? F("только ручное") : F("only manually"));
 content += F("</option><option selected='selected' value='11'>"); 
 content += (Language ? F("по заданиям и вручную") : F("by tasks and manualy"));
 content += F("</option><option "); 
 }
if ( ChannelsControlsMode == CHANNELS_CONTROLS_ONLY_BY_TASKS )
 { 
 content += F("value='10'>"); 
 content += (Language ? F("только ручное") : F("only manually"));
 content += F("</option><option value='11'>"); 
 content += (Language ? F("по заданиям и вручную") : F("by tasks and manualy"));
 content += F("</option><option selected='selected' "); 
 }
content += F("value='12'>"); 
content += (Language ? F("только по заданиям") : F("only by tasks"));
content += F("</option></select>&emsp;<input type='submit' value='"); 
content += (Language ? F("Сохранить' />") : F("Save' />"));
if ( ChannelsControlsMode == CHANNELS_CONTROLS_MANUALLY_AND_BY_TASKS )
 {
 content += (Language ? F("<i>&emsp;(ручное переключение до наступления действия очередного задания, или пропадания питания)</i>")
                      : F("<i>&emsp;(manual switching is active until the task-trigger or reboot)</i>"));
 }
content += F("</p></form><hr />");
server.sendContent(content); content = "";
// list tasks
curChNum = TaskList[0][TASK_CHANNEL];
curTaskEnabled = (TaskList[0][TASK_ACTION] > 0);
for (int taskNum = 0; taskNum < numberOfTasks; taskNum++)
 {
 yield(); 
 if ( numberOfChannels > 1 
  && ((curChNum != TaskList[taskNum][TASK_CHANNEL] && (TaskList[taskNum][TASK_ACTION] > 0)) 
    || curTaskEnabled != (TaskList[taskNum][TASK_ACTION] > 0))
    )
  {
  content += F("<p style='line-height:1%'> <br></p>");
  curChNum = TaskList[taskNum][TASK_CHANNEL];
  curTaskEnabled = (TaskList[taskNum][TASK_ACTION] > 0);
  }
 content += ( TaskList[taskNum][TASK_ACTION] 
           && ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_ENABLED]
           && ChannelsControlsMode != CHANNELS_CONTROLS_ONLY_MANUALLY ? F("<font color='black'><b>") : F("<font color='dimgrey'>") );
 content += F("<form method='get' form action='/settask'><p>");
 content += (Language ? F("Задание ") : F("Task "));
 if ( numberOfTasks >  9 && (taskNum + 1) <  10 ) { content += F("0"); }
 if ( numberOfTasks > 99 && (taskNum + 1) < 100 ) { content += F("0"); }
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
  { content += (Language ? F("<option selected='selected' value='0'>задание отключено</option><option value='11'>включить</option><option")
                         : F("<option selected='selected' value='0'>no action</option><option value='11'>set ON</option><option")); }
 if ( TaskList[taskNum][TASK_ACTION] == ACTION_TURN_OFF ) 
  { content += (Language ? F("<option value='0'>задание отключено</option><option value='11'>включить</option><option selected='selected'")
                         : F("<option value='0'>disabled</option><option value='11'>set ON</option><option selected='selected'")); }
 if ( TaskList[taskNum][TASK_ACTION] == ACTION_TURN_ON )  
  { content += (Language ? F("<option value='0'>задание отключено</option><option selected='selected' value='11'>включить</option><option")
                         : F("<option value='0'>disabled</option><option selected='selected' value='11'>set ON</option><option")); }
 content += (Language ? F(" value='10'>выключить</option></select>&nbsp;") : F(" value='10'>set OFF</option></select>&nbsp;"));
 // Hour, Minute, Second
 content += (Language ? F("Час") : F("Hour"));
 content += F("&nbsp;<input name='h' type='number' min='0' max='23' value='");
 content += String(TaskList[taskNum][TASK_HOUR]);
 content += (Language ? F("' />&nbsp;Минута") : F("' />&nbsp;Minute"));
 content += F("&nbsp;<input name='m' type='number' min='0' max='59' value='");
 content += String(TaskList[taskNum][TASK_MIN]);
 content += (Language ? F("' />&nbsp;Секунда") : F("' />&nbsp;Second"));
 content += F("&nbsp;<input name='s' type='number' min='0' max='59' value='");
 content += String(TaskList[taskNum][TASK_SEC]) + F("' />&nbsp;");
 // Day(s)
 content += (Language ? F("День(дни)") : F("Day(s)"));
 content += F("&nbsp;<select name='d' size='1'><option ");
 for (int t = 0; t <= 9; t++)
  {
  if ( TaskList[taskNum][TASK_DAY] == t ) { content += F("selected='selected' "); } 
  content += F("value='"); content += String(t) + F("'>") + (Language ? namesOfDaysR[t] : namesOfDaysE[t]) + F("</option>");       
  if ( t < 9 ) { content += F("<option "); }
  }
 content += F("</select>&nbsp;<input type='submit' value='");
 content += (Language ? F("Сохранить' />") : F("Save' />"));
 if ( TaskList[taskNum][TASK_ACTION] != 0 )
  {
  int duplicateTaskNumber = find_duplicate_or_conflicting_task(taskNum);
  if ( duplicateTaskNumber >= 0 )
   {
   content += F("<font color='red'>&emsp;");  
   if ( duplicateTaskNumber >= 1000 ) { content += (Language ? F("<b>конфликт с заданием ") : F("<b>conflicts with task ")); duplicateTaskNumber -= 1000; }
                                 else { content += (Language ? F("<b>дублирует задание ") : F("<b>duplicates task ")); }
   if ( numberOfTasks >  9 && (duplicateTaskNumber + 1) <  10 ) { content += F("0"); }
   if ( numberOfTasks > 99 && (duplicateTaskNumber + 1) < 100 ) { content += F("0"); }
   content += String(duplicateTaskNumber + 1) + F(" !</b>");
   }
  if ( ChannelsControlsMode != CHANNELS_CONTROLS_ONLY_MANUALLY )
   { 
   if ( ActiveNowTasksList[TaskList[taskNum][TASK_CHANNEL]] == taskNum )
    {
    content += F("<font color='darkblue'>&emsp;");  
    content += ( TaskList[taskNum][TASK_ACTION] - 10 == ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_LASTSTATE] 
               ? (Language ? F("активно") : F("active now")) : (Language ? F("переключено вручную") : F("manually switched")) );
    }
   }    
  } 
 content += F("</p></form></font></b>");
 server.sendContent(content); content = "";
 } // end of for (int taskNum = 0; taskNum < numberOfTasks; taskNum++)
// 
yield(); 
content += F("<hr /><form method='get' form action='/cleartasklist'><p>");
content += (Language ? F("Очистить и отключить все задания (будьте осторожны !):") : F("Clear and disable all tasks (be careful !):"));
content += F("&emsp;<input name='ctl' type='submit' value='");
content += (Language ? F("Очистить список заданий' />") : F("Clear task list' />"));
content += F("</form></p>"); 
content += F("<hr /><form method='get' form action='/setnumberOfTasks'><p>");
content += (Language ? F("Количество заданий (") : F("Number of tasks ("));
content += String(TASKLIST_MIN_NUMBER) + F("...") + String(TASKLIST_MAX_NUMBER) 
        + F("):&emsp;<input name='nt' type='number' min='") + String(TASKLIST_MIN_NUMBER) + F("' max='") + String(TASKLIST_MAX_NUMBER) + F("' value='");
content += String(numberOfTasks) + F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить и перезагрузить") : F("Save and reboot"));
content += F("'/></p></form>");
content += F("<hr /><form method='get' form action='/setnumberOfChannels'><p>");
content += (Language ? F("Количество каналов (") : F("Number of channels ("));
content += String(CHANNELLIST_MIN_NUMBER) + F("...") + String(CHANNELLIST_MAX_NUMBER) 
        + F("):&emsp;<input name='nc' type='number' min='") + String(CHANNELLIST_MIN_NUMBER) + F("' max='") + String(CHANNELLIST_MAX_NUMBER) + F("' value='");
content += String(numberOfChannels) + F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить и перезагрузить") : F("Save and reboot"));
content += F("'/></p></form>");
content += F("<hr />");
// ChannelList
for ( int chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 content += ( ChannelList[chNum][CHANNEL_ENABLED] ? F("<font color='black'><b>") : F("<font color='dimgrey'>") );
 content += F("<form method='get' form action='/setchannelparams'><p>");
 content += (Language ? F("Канал ") : F("Channel "));
 if ( numberOfChannels > 9 && (chNum + 1) <  10 ) { content += F("0"); }
 content += String(chNum + 1) + F(":</b>&nbsp;<select name='e");
 if ( chNum < 10 ) { content += F("0"); }
 content += String(chNum) + F("' size='1'><option ");
 if ( ChannelList[chNum][CHANNEL_ENABLED] )
      { content += (Language ? F("selected='selected' value='11'>включен</option><option value='10'>") : F("selected='selected' value='11'>enabled</option><option value='10'>")); }
 else { content += (Language ? F("value='11'>включен</option><option selected='selected' value='10'>") : F("value='11'>enabled</option><option selected='selected' value='10'>")); }
 content += (Language ? F("отключен") : F("disabled"));
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
  if ( duplicateChannelNumber >= 1000 ) { content += (Language ? F("<b>конфликт с каналом ") : F("<b>conflicts with channel ")); duplicateChannelNumber -= 1000; }
                                   else { content += (Language ? F("<b>дублирует канал ") : F("<b>duplicates channel ")); }
   if ( numberOfChannels > 9 && (duplicateChannelNumber + 1) < 10 ) { content += F("0"); }
   content += String(duplicateChannelNumber + 1) + F(" !</b></font>");
  }
 content += F("</p></form></font>");
 }
content += (Language ? F("<i>Для NodeMCU GPIO может быть от ") : F("<i>For NodeMCU GPIO can be from 0"));
content += (Language ? F(" до ") : F(" to "));
content += String(GPIO_MAX_NUMBER);
content += (Language ? F(", исключая 6,7,8 и 11</i>") : F(", exclude 6,7,8 and 11</i>"));
content += F("<hr /><form method='get' form action='/setlanguage'><p>");
content += (Language ? F("Язык интерфейса") : F("Interface language"));
content += F(":&emsp;<select name='lang' size='1'><option ");
content += ( Language ? F("selected='selected' value='11'>Русский</option><option value='10'>English</option></select>")
                      : F("value='11'>Русский</option><option selected='selected' value='10'>English</option></select>"));
content += F("&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить' /></p></form>") : F("Save' /></p></form>"));
content += F("<hr /><form method='get' form action='/setntpTimeZone'><p>");
content += (Language ? F("Часовой пояс") : F("Time Zone"));
content += F("(-12...12):&emsp;<input name='tz' type='number' min='-12' max='12' value='");
content += String(ntpTimeZone) + F("' />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить и перезагрузить'") : F("Save and reboot'"));
content += F(" /></p></form><hr /><form method='get' form action='/setlogin'><p>");
content += (Language ? F("Имя авторизации") : F("Login name"));
content += F(":&emsp;<input maxlength='10' name='ln' size='10' type='text' value='");
content += loginName + F("' /></p><p>");
content += (Language ? F("Новый пароль") : F("New password"));
content += F(":&emsp;<input maxlength='10' name='np' size='10' type='password' />&emsp;");
content += (Language ? F("Подтвердите новый пароль") : F("Confirm new password"));
content += F(":&emsp;<input maxlength='10' name='npc' size='10' type='password' />&emsp;");
content += (Language ? F("Старый пароль") : F("Old password"));
content += F(":&emsp;<input maxlength='10' name='op' size='10' type='password'");
content += F(" />&emsp;<input type='submit' value='");
content += (Language ? F("Сохранить'/></p></form>") : F("Save'/></p></form>"));
content += F("<hr /><form><input formaction='/firmware' formmethod='get' type='submit' value='");
content += (Language ? F("Загрузка прошивки'") : F("Firmware update'"));
content += F("' />&emsp;&emsp;&emsp;<input formaction='/restart' formmethod='get' type='submit' value='");
content += (Language ? F("Перезагрузить' /></form>") : F("Reboot' /></form>"));
content += F("</body></html>\r\n");
yield();
server.sendContent(content);
server.sendContent(F("")); // transfer is done
server.client().stop();
}

void setupServer()
{
server.on("/",[]() 
 {
 if (!server.authenticate(loginName.c_str(), loginPass.c_str())) { return server.requestAuthentication(); }
 drawHomePage(); 
 });
server.on("/settask", []() 
 {
 if (!server.authenticate(loginName.c_str(), loginPass.c_str())) { return server.requestAuthentication(); }
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
 if ( param >= 0 && param <= 9 )  { if ( TaskList[taskNumber][TASK_DAY] != param ) { TaskList[taskNumber][TASK_DAY] = param; needSave = true; } }
 #ifdef DEBUG
  Serial.print(F("Set task ")); Serial.println(taskNumber + 1);
  Serial.print(F("Ch="));       Serial.print(TaskList[taskNumber][TASK_CHANNEL] + 1);
  Serial.print(F("\tAction=")); Serial.print(TaskList[taskNumber][TASK_ACTION]); 
  Serial.print(F("\tHour="));   Serial.print(TaskList[taskNumber][TASK_HOUR]);
  Serial.print(F("\tMinute=")); Serial.print(TaskList[taskNumber][TASK_MIN]);
  Serial.print(F("\tSecond=")); Serial.print(TaskList[taskNumber][TASK_SEC]);
  Serial.print(F("\tDay(s)=")); Serial.println(TaskList[taskNumber][TASK_DAY]);
 #endif 
 if ( needSave ) { save_tasks_to_EEPROM(); check_previous_tasks(); }  
 server.send(200, "text/html; charset=utf-8", F("<META http-equiv=\"refresh\" content=\"0;URL=/\">"));
 }); 
server.on("/cleartasklist", []()
 { 
 if (!server.authenticate(loginName.c_str(), loginPass.c_str())) { return server.requestAuthentication(); }
 for ( int taskNum = 0; taskNum < numberOfTasks; taskNum++ )
  { 
  yield(); 
  TaskList[taskNum][TASK_ACTION] = 0; 
  TaskList[taskNum][TASK_HOUR] = 0; 
  TaskList[taskNum][TASK_MIN] = 0; 
  TaskList[taskNum][TASK_SEC] = 0; 
  TaskList[taskNum][TASK_DAY] = 9;
  TaskList[taskNum][TASK_CHANNEL] = 0;
  }
 save_tasks_to_EEPROM(); 
 server.send(200, "text/html; charset=utf-8", F("<META http-equiv=\"refresh\" content=\"0;URL=/\">"));
 }); 
server.on("/setnumberOfTasks", []() 
 { 
 if (!server.authenticate(loginName.c_str(), loginPass.c_str())) { return server.requestAuthentication(); }
 String buf = server.arg(0);
 int param = buf.toInt();
 if ( param >= TASKLIST_MIN_NUMBER && param <= TASKLIST_MAX_NUMBER ) 
  { if ( numberOfTasks != param ) 
     {
     numberOfTasks = param; 
     EEPROM.write(NUMBER_OF_TASKS_EEPROM_ADDRESS, numberOfTasks); EEPROM.commit();
     server.send(200, "text/html; charset=utf-8", (Language ? F("<META http-equiv=\'refresh\' content=\'15;URL=/\'> Перезагрузка...") : F("<META http-equiv=\'refresh\' content=\'15;URL=/\'> Rebooting..."))); 
     delay(500);
     server.stop();
     ESP.restart(); 
     }
  } 
 server.send(200, "text/html; charset=utf-8", F("<META http-equiv=\"refresh\" content=\"0;URL=/\">"));
 }); 
server.on("/setnumberOfChannels", []() 
 { 
 if (!server.authenticate(loginName.c_str(), loginPass.c_str())) { return server.requestAuthentication(); }
 String buf = server.arg(0);
 int param = buf.toInt();
 if ( param >= CHANNELLIST_MIN_NUMBER && param <= CHANNELLIST_MAX_NUMBER ) 
  { if ( numberOfChannels != param ) 
     {
     numberOfChannels = param; 
     EEPROM.write(NUMBER_OF_CHANNELS_EEPROM_ADDRESS, numberOfChannels); EEPROM.commit();
     server.send(200, "text/html; charset=utf-8", (Language ? F("<META http-equiv=\'refresh\' content=\'15;URL=/\'> Перезагрузка...") : F("<META http-equiv=\'refresh\' content=\'15;URL=/\'> Rebooting..."))); 
     delay(500);
     server.stop();
     ESP.restart(); 
     }
  } 
 server.send(200, "text/html; charset=utf-8", F("<META http-equiv=\"refresh\" content=\"0;URL=/\">"));
 });
server.on("/ChannelsControlsMode", []() 
 { 
 if (!server.authenticate(loginName.c_str(), loginPass.c_str())) { return server.requestAuthentication(); }
 String buf = server.arg(0);
 int param = buf.toInt();
 if ( param == CHANNELS_CONTROLS_ONLY_MANUALLY || param == CHANNELS_CONTROLS_MANUALLY_AND_BY_TASKS || param == CHANNELS_CONTROLS_ONLY_BY_TASKS ) 
  { if ( ChannelsControlsMode != param ) 
     {
     ChannelsControlsMode = param; 
     EEPROM.write(CHANNELS_CONTROLS_MODE_EEPROM_ADDRESS, ChannelsControlsMode); EEPROM.commit();
     }
  }  
 server.send(200, "text/html; charset=utf-8", F("<META http-equiv=\"refresh\" content=\"0;URL=/\">"));
 }); 
server.on("/setchannelstate", []()
 {
 if (!server.authenticate(loginName.c_str(), loginPass.c_str())) { return server.requestAuthentication(); }
 int chNum, param = 0;
 String buf = server.argName(0);
 buf = buf.substring(1);
 chNum = buf.toInt();
 buf = server.arg(0);
 if ( buf.indexOf("ON") > -1 || buf.indexOf("Включить") > -1 ) { param = 1; }
 if ( ChannelList[chNum][CHANNEL_LASTSTATE] != param ) 
  {
  ChannelList[chNum][CHANNEL_LASTSTATE] = param;
  save_channellist_to_EEPROM(); 
  }
 server.send(200, "text/html; charset=utf-8", F("<META http-equiv=\"refresh\" content=\"0;URL=/\">"));
 }); 
server.on("/setchannelparams", []() 
 { 
 if (!server.authenticate(loginName.c_str(), loginPass.c_str())) { return server.requestAuthentication(); }
 bool needSave = false;
 int chNum, param;
 String buf = server.argName(0);
 buf = buf.substring(1);
 chNum = buf.toInt();
 buf = server.arg(0);
 param = buf.toInt();
 if ( param == 10 || param == 11 ) 
  { if ( ((!ChannelList[chNum][CHANNEL_ENABLED]) && param == 11) || (ChannelList[chNum][CHANNEL_ENABLED] && param == 10) ) 
     {
     ChannelList[chNum][CHANNEL_ENABLED] = ( param == 10 ? 0 : 1 ); 
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
 if ( needSave ) { save_channellist_to_EEPROM(); } 
 server.send(200, "text/html; charset=utf-8", F("<META http-equiv=\"refresh\" content=\"0;URL=/\">"));
 }); 
server.on("/setlanguage", []() 
 { 
 if (!server.authenticate(loginName.c_str(), loginPass.c_str())) { return server.requestAuthentication(); }
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
 server.send(200, "text/html; charset=utf-8", F("<META http-equiv=\"refresh\" content=\"0;URL=/\">"));
 }); 
server.on("/setntpTimeZone", []() 
 { 
 if (!server.authenticate(loginName.c_str(), loginPass.c_str())) { return server.requestAuthentication(); }
 String buf = server.arg(0);
 int param = buf.toInt();
 if ( ntpTimeZone != param ) 
  {
  ntpTimeZone = param; 
  timeClient.setTimeOffset(ntpTimeZone * 3600);
  EEPROM.write(NTP_TIME_ZONE_EEPROM_ADDRESS, ntpTimeZone + 12); EEPROM.commit();
  server.send(200, "text/html; charset=utf-8", (Language ? F("<META http-equiv=\'refresh\' content=\'15;URL=/\'> Перезагрузка...") : F("<META http-equiv=\'refresh\' content=\'15;URL=/\'> Rebooting..."))); 
  delay(500);
  server.stop();
  ESP.restart(); 
  }
server.send(200, "text/html; charset=utf-8", F("<META http-equiv=\"refresh\" content=\"0;URL=/\">"));
 }); 
server.on("/setlogin", []() 
 {
 if (!server.authenticate(loginName.c_str(), loginPass.c_str())) { return server.requestAuthentication(); }
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
  server.send(200, "text/html; charset=utf-8", F("<META http-equiv=\"refresh\" content=\"0;URL=/\">"));
  }
 else { server.send(200, "text/html", F("<META http-equiv=\"refresh\" content=\"5;URL=/\"> Incorrect data or passwords do not match...")); }
 });  
server.on("/restart", []()  
 {
 if (!server.authenticate(loginName.c_str(), loginPass.c_str())) { return server.requestAuthentication(); }
 server.send(200, "text/html; charset=utf-8", (Language ? F("<META http-equiv=\'refresh\' content=\'15;URL=/\'> Перезагрузка...") : F("<META http-equiv=\'refresh\' content=\'15;URL=/\'> Rebooting..."))); 
 delay(500);
 server.stop();
 ESP.restart(); 
 }); 
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
#ifdef DEBUG
 Serial.print(F("Login = ")); Serial.print(loginName);
 Serial.print(F(" Password = ")); Serial.println(loginPass);
#endif  
httpUpdater.setup(&server, "/firmware", loginName, loginPass);
}

int find_duplicate_or_conflicting_task(int findNUM)
{
int found = -1;
for ( int taskNum = 0; taskNum < numberOfTasks - 1; taskNum++ )
 {
 yield(); 
 if ( taskNum != findNUM 
   && ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_ENABLED]
   && TaskList[taskNum][TASK_ACTION]  != 0
   && TaskList[taskNum][TASK_HOUR]    == TaskList[findNUM][TASK_HOUR] 
   && TaskList[taskNum][TASK_MIN]     == TaskList[findNUM][TASK_MIN]
   && TaskList[taskNum][TASK_SEC]     == TaskList[findNUM][TASK_SEC]
   && TaskList[taskNum][TASK_CHANNEL] == TaskList[findNUM][TASK_CHANNEL]
    )
  {
  if ( TaskList[taskNum][TASK_DAY] == TaskList[findNUM][TASK_DAY]
   || (TaskList[taskNum][TASK_DAY] == 7 && working_day_of_week(TaskList[findNUM][TASK_DAY]))
   || (TaskList[taskNum][TASK_DAY] == 8 && weekend_day_of_week(TaskList[findNUM][TASK_DAY]))
   ||  TaskList[taskNum][TASK_DAY] == 9 )
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
   && TaskList[taskNum][TASK_ACTION] != 0
   && (
       TaskList[taskNum][TASK_HOUR] < hh 
   || (TaskList[taskNum][TASK_HOUR] == hh && TaskList[taskNum][TASK_MIN] < mm)
   || (TaskList[taskNum][TASK_HOUR] == hh && TaskList[taskNum][TASK_MIN] == mm && TaskList[taskNum][TASK_SEC] <= ss)
      )
    )
  {
  if ( (TaskList[taskNum][TASK_DAY] == findDOW)
    || (TaskList[taskNum][TASK_DAY] == 7 && working_day_of_week(findDOW))
    || (TaskList[taskNum][TASK_DAY] == 8 && weekend_day_of_week(findDOW))
    || (TaskList[taskNum][TASK_DAY] == 9)
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
for (chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 if ( NumEnabledTasks[chNum] == 0 ) { continue; }
 findDOW = curDayOfWeek;
 foundTask = TASKLIST_MAX_NUMBER;  
 DaysCounter = 0;
 #ifdef DEBUG
  Serial.print(F("Searching previous task for Ch=")); Serial.print(chNum + 1); 
  Serial.print(F(" H=")); Serial.print(curTimeHour); 
  Serial.print(F(" M=")); Serial.print(curTimeMin); 
  Serial.print(F(" S=")); Serial.print(curTimeSec);
  Serial.print(F(" D=")); Serial.print(findDOW); Serial.println();
 #endif
 while ( DaysCounter < 7 ) 
  {
  foundTask = check_previous_tasks_within_day(chNum, findDOW, curTimeHour, curTimeMin, curTimeSec);
  if ( foundTask < TASKLIST_MAX_NUMBER ) { break; }
  DaysCounter++;
  if ( findDOW == 0 ) { findDOW = 6; } else { findDOW--; }
  }
 #ifdef DEBUG
  if ( foundTask == TASKLIST_MAX_NUMBER ) 
   { Serial.println(F("NOT FOUND")); }
  else
   {
   Serial.print(F("                          FOUND"));
   Serial.print(F(" H=")); Serial.print(TaskList[foundTask][TASK_HOUR]); 
   Serial.print(F(" M=")); Serial.print(TaskList[foundTask][TASK_MIN]);
   Serial.print(F(" S=")); Serial.print(TaskList[foundTask][TASK_SEC]);
   Serial.print(F(" D=")); Serial.print(TaskList[foundTask][TASK_DAY]); 
   Serial.print(F(" A=")); Serial.print(TaskList[foundTask][TASK_ACTION]);
   Serial.println();
   }
 #endif
 ActiveNowTasksList[chNum] = ( foundTask < TASKLIST_MAX_NUMBER ? foundTask : 255 ); 
 if ( foundTask < TASKLIST_MAX_NUMBER
  && (ChannelsControlsMode != CHANNELS_CONTROLS_ONLY_MANUALLY)
  && (TaskList[foundTask][TASK_ACTION] == ACTION_TURN_ON) != ChannelList[chNum][CHANNEL_LASTSTATE] )
  {
  ChannelList[chNum][CHANNEL_LASTSTATE] = (TaskList[foundTask][TASK_ACTION] == ACTION_TURN_ON);
  needSave = true;
  }
 }
if ( needSave ) { save_channellist_to_EEPROM(); }   
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
 taskAddress = TASKLIST_EEPROM_ADDRESS + taskNum * TASK_NUM_ELEMENTS;
 yield(); 
 for ( task_element_num = 0; task_element_num < TASK_NUM_ELEMENTS; task_element_num++ ) 
  {
  TaskListRAW[taskNum][task_element_num] = EEPROM.read(taskAddress + task_element_num);
  }
 if ( TaskListRAW[taskNum][TASK_ACTION] != ACTION_NOACTION && TaskListRAW[taskNum][TASK_ACTION] != ACTION_TURN_OFF
   && TaskListRAW[taskNum][TASK_ACTION] != ACTION_TURN_ON ) { TaskListRAW[taskNum][TASK_ACTION] = ACTION_NOACTION; }
 if ( TaskListRAW[taskNum][TASK_HOUR] > 23 ) { TaskListRAW[taskNum][TASK_HOUR] = 0; }
 if ( TaskListRAW[taskNum][TASK_MIN] > 59 )  { TaskListRAW[taskNum][TASK_MIN] = 0; }
 if ( TaskListRAW[taskNum][TASK_SEC] > 59 )  { TaskListRAW[taskNum][TASK_SEC] = 0; }
 if ( TaskListRAW[taskNum][TASK_DAY] > 9 )   { TaskListRAW[taskNum][TASK_DAY] = 9; }
 if ( TaskListRAW[taskNum][TASK_CHANNEL] > numberOfChannels ) { TaskListRAW[taskNum][TASK_CHANNEL] = 0; }
 #ifdef DEBUG
  Serial.print(F("Addr="));     Serial.print(taskAddress);
  Serial.print(F("\tTask="));   Serial.print(taskNum + 1);
  Serial.print(F("\tCh="));     Serial.print(TaskListRAW[taskNum][TASK_CHANNEL] + 1);
  Serial.print(F("\tAction=")); Serial.print(TaskListRAW[taskNum][TASK_ACTION]); 
  Serial.print(F("\tHour="));   Serial.print(TaskListRAW[taskNum][TASK_HOUR]);
  Serial.print(F("\tMinute=")); Serial.print(TaskListRAW[taskNum][TASK_MIN]);
  Serial.print(F("\tSecond=")); Serial.print(TaskListRAW[taskNum][TASK_SEC]);
  Serial.print(F("\tDay(s)=")); Serial.println(TaskListRAW[taskNum][TASK_DAY]);
 #endif 
 }
// Sorting in enabled action, channel and ascending order of time
for ( taskNum = 0; taskNum < numberOfTasks; taskNum++ ) { yield(); IndexArray[taskNum] = taskNum; } 
for ( taskNum = 0; taskNum < numberOfTasks - 1; taskNum++ )
 {
 yield(); 
 for ( int bubble_num = 0; bubble_num < numberOfTasks - taskNum - 1; bubble_num++ )
  {
  yield(); 
  if ((( TaskListRAW[IndexArray[bubble_num]][TASK_ACTION] ? 0 : 1 + 86401 * CHANNELLIST_MAX_NUMBER)
       + TaskListRAW[IndexArray[bubble_num]][TASK_CHANNEL] * 86401
       + TaskListRAW[IndexArray[bubble_num]][TASK_HOUR] * 3600
       + TaskListRAW[IndexArray[bubble_num]][TASK_MIN] * 60 
       + TaskListRAW[IndexArray[bubble_num]][TASK_SEC])
    > (( TaskListRAW[IndexArray[bubble_num+1]][TASK_ACTION] ? 0 : 1 + 86401 * CHANNELLIST_MAX_NUMBER)
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
 if ( ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_ENABLED] 
   && TaskList[taskNum][TASK_ACTION] != 0 )
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
 #ifdef DEBUG
  Serial.print(F("Addr="));     Serial.print(taskAddress);
  Serial.print(F("\tTask="));   Serial.print(taskNum + 1);
  Serial.print(F("\tCh="));     Serial.print(TaskList[taskNum][TASK_CHANNEL] + 1);
  Serial.print(F("\tAction=")); Serial.print(TaskList[taskNum][TASK_ACTION]); 
  Serial.print(F("\tHour="));   Serial.print(TaskList[taskNum][TASK_HOUR]);
  Serial.print(F("\tMinute=")); Serial.print(TaskList[taskNum][TASK_MIN]);
  Serial.print(F("\tSecond=")); Serial.print(TaskList[taskNum][TASK_SEC]);
  Serial.print(F("\tDay(s)=")); Serial.println(TaskList[taskNum][TASK_DAY]);
 #endif 
 }
EEPROM.commit();
read_and_sort_tasklist_from_EEPROM();
}

void read_channellist_from_EEPROM_and_switch_channels()
{
int chNum, ch_element_num;
#ifdef DEBUG 
 Serial.println(F("Reading channels"));
#endif
for ( chNum = 0; chNum < numberOfChannels; chNum++ )
 {
 yield(); 
 for ( ch_element_num = 0; ch_element_num < CHANNEL_NUM_ELEMENTS; ch_element_num++ ) 
  { ChannelList[chNum][ch_element_num] = EEPROM.read(CHANNELLIST_EEPROM_ADDRESS + ch_element_num + chNum * CHANNEL_NUM_ELEMENTS); }
 if ( ChannelList[chNum][CHANNEL_GPIO] > GPIO_MAX_NUMBER || NodeMCUpins[ChannelList[chNum][CHANNEL_GPIO]] == "N/A" ) { ChannelList[chNum][CHANNEL_GPIO] = LED_BUILTIN; }
 if ( ChannelList[chNum][CHANNEL_INVERTED] != 0  && ChannelList[chNum][CHANNEL_INVERTED] != 1 )  { ChannelList[chNum][CHANNEL_INVERTED] = 0; }
 if ( ChannelList[chNum][CHANNEL_LASTSTATE] != 0 && ChannelList[chNum][CHANNEL_LASTSTATE] != 1 ) { ChannelList[chNum][CHANNEL_LASTSTATE] = 0; }
 if ( ChannelList[chNum][CHANNEL_ENABLED] != 0 && ChannelList[chNum][CHANNEL_ENABLED] != 1 ) { ChannelList[chNum][CHANNEL_ENABLED] = 0; }
 pinMode(ChannelList[chNum][CHANNEL_GPIO], OUTPUT);
 digitalWrite(ChannelList[chNum][CHANNEL_GPIO], (ChannelList[chNum][CHANNEL_INVERTED] ? (!ChannelList[chNum][CHANNEL_LASTSTATE]) : ChannelList[chNum][CHANNEL_LASTSTATE])); 
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
read_channellist_from_EEPROM_and_switch_channels();
}

void setup()
{
#ifdef DEBUG 
 Serial.begin(9600);
 Serial.println();
 Serial.println(F("Versatile timer started"));
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
 EEPROM.commit();
 EEPROMWriteInt(FIRST_RUN_SIGNATURE_EEPROM_ADDRESS, FIRST_RUN_SIGNATURE); 
 EEPROM.end();
 #ifdef DEBUG 
  Serial.println(F("Rebooting..."));
  delay(5000);
 #endif 
 ESP.restart();
 }
ChannelsControlsMode = EEPROM.read(CHANNELS_CONTROLS_MODE_EEPROM_ADDRESS);
if ( ChannelsControlsMode != CHANNELS_CONTROLS_ONLY_MANUALLY 
  && ChannelsControlsMode != CHANNELS_CONTROLS_MANUALLY_AND_BY_TASKS 
  && ChannelsControlsMode != CHANNELS_CONTROLS_ONLY_BY_TASKS ) { ChannelsControlsMode = CHANNELS_CONTROLS_MANUALLY_AND_BY_TASKS; }
Language = EEPROM.read(LANGUAGE_EEPROM_ADDRESS);
if ( Language != 0 && Language != 1 ) { Language = 0; }
ntpTimeZone = EEPROM.read(NTP_TIME_ZONE_EEPROM_ADDRESS);
ntpTimeZone = ntpTimeZone - 12;
if ( ntpTimeZone > 24 ) { ntpTimeZone = 2; }
numberOfTasks = EEPROM.read(NUMBER_OF_TASKS_EEPROM_ADDRESS);
if ( numberOfTasks < TASKLIST_MIN_NUMBER || numberOfTasks > TASKLIST_MAX_NUMBER ) { numberOfTasks = 5; }
TaskList = new uint8_t*[numberOfTasks];
for ( int taskNum = 0; taskNum < numberOfTasks; taskNum++ ) { yield(); TaskList[taskNum] = new uint8_t[TASK_NUM_ELEMENTS]; }
numberOfChannels = EEPROM.read(NUMBER_OF_CHANNELS_EEPROM_ADDRESS);
if ( numberOfChannels < CHANNELLIST_MIN_NUMBER || numberOfChannels > CHANNELLIST_MAX_NUMBER ) { numberOfChannels = CHANNELLIST_MIN_NUMBER; }
ChannelList = new uint8_t*[numberOfChannels];
for ( int chNum = 0; chNum < numberOfChannels; chNum++ ) 
 { yield(); ChannelList[chNum] = new uint8_t[CHANNEL_NUM_ELEMENTS]; ActiveNowTasksList[chNum] = 255; NumEnabledTasks[chNum] = 0; }
EEPROM.end();
EEPROM.begin(TASKLIST_EEPROM_ADDRESS + numberOfTasks * TASK_NUM_ELEMENTS);
read_login_pass_from_EEPROM();
read_channellist_from_EEPROM_and_switch_channels();
read_and_sort_tasklist_from_EEPROM();
//
WiFi.begin();
delay(500);
WiFi.mode(WIFI_STA);
WiFi.softAPdisconnect(true);
#ifdef DEBUG 
 Serial.println();
 Serial.println(F("Connecting to saved AP..."));
#endif
int ct = 60;
while ( WiFi.status() != WL_CONNECTED && (ct > 0) ) 
 {
 yield(); delay(500); yield(); ct--;
 #ifdef DEBUG 
  Serial.print(F("."));
 #endif
 }
#ifdef DEBUG 
 Serial.println();
#endif
// if not autoconnected
statusWiFi = ( WiFi.status() == WL_CONNECTED );
if ( !statusWiFi )
 {
 #ifdef DEBUG 
  Serial.print(F("Connecting to "));
  Serial.println(AP_SSID);
 #endif
 WiFi.begin(AP_SSID, AP_PASS);
 int ct = 60;
 while ( WiFi.status() != WL_CONNECTED && (ct > 0) )
  {
  yield(); delay(500); yield(); ct--;
  #ifdef DEBUG 
   Serial.print(F("."));
  #endif
  }
 #ifdef DEBUG 
  Serial.println();
 #endif
 }
statusWiFi = ( WiFi.status() == WL_CONNECTED );
#ifdef DEBUG 
 if ( statusWiFi ) 
  {
  Serial.print(F("Connected to ")); Serial.println(AP_SSID);
  Serial.print(F("Server can be accessed at http://")); Serial.print(WiFi.localIP());
  Serial.print(F(" or at http://")); Serial.print(MDNSID); Serial.println(F(".local"));
  }
 else { Serial.println(F("WiFi NOT connected now.")); }
#endif
if ( !WiFi.getAutoConnect() ) WiFi.setAutoConnect(true);
WiFi.setAutoReconnect(true);
//
counterOfReboots = EEPROMReadInt(COUNTER_OF_REBOOTS_EEPROM_ADDRESS);
if ( counterOfReboots < 0 || counterOfReboots > 32766 ) { counterOfReboots = 0; }
counterOfReboots++;
EEPROMWriteInt(COUNTER_OF_REBOOTS_EEPROM_ADDRESS, counterOfReboots); 
//
timeClient.setTimeOffset(ntpTimeZone * 3600);
timeClient.begin();
MDNS.begin(MDNSID);
setupServer();
server.begin();
ESP.wdtEnable(WDTO_8S);
everySecondTimer = millis();
}

void loop()
{
yield(); delay(0);  
statusWiFi = ( WiFi.status() == WL_CONNECTED );  
server.handleClient();
if ( statusWiFi ) { timeSyncOK = timeClient.update(); } else { timeSyncOK = false; }
if ( timeSyncOK )
 {
 curTimeHour = timeClient.getHours();
 curTimeMin = timeClient.getMinutes();
 curTimeSec = timeClient.getSeconds();
 curDayOfWeek = timeClient.getDay(); // 0 - Sunday, 1...5 - Monday...Friday, 6 - Saturday
 } 
//
if ( timeSyncOK != timeSyncInitially && timeSyncOK )
 {
 check_previous_tasks();
 timeSyncInitially = true;
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
 setClockpreviousMillis = setClockcurrentMillis; 
 everySecondTimer = millis(); 
 } 
//
if ( thereAreEnabledTasks && timeSyncOK && curTimeSec != previousSecond )
 {
 if ( ChannelsControlsMode == CHANNELS_CONTROLS_ONLY_BY_TASKS )
  { check_previous_tasks(); } 
 else if ( ChannelsControlsMode == CHANNELS_CONTROLS_MANUALLY_AND_BY_TASKS )
  {
  for ( int taskNum = 0; taskNum < numberOfTasks; taskNum++ )
   {
   yield(); 
   if ( ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_ENABLED]
     && TaskList[taskNum][TASK_ACTION] > 0 
     && curTimeHour == TaskList[taskNum][TASK_HOUR] && curTimeMin == TaskList[taskNum][TASK_MIN] && curTimeSec == TaskList[taskNum][TASK_SEC]
     && (TaskList[taskNum][TASK_DAY] == curDayOfWeek
     || (TaskList[taskNum][TASK_DAY] == 7 && working_day_of_week(curDayOfWeek))
     || (TaskList[taskNum][TASK_DAY] == 8 && weekend_day_of_week(curDayOfWeek))
     ||  TaskList[taskNum][TASK_DAY] == 9)
      )
    {
    ActiveNowTasksList[TaskList[taskNum][TASK_CHANNEL]] = taskNum;
    ChannelList[TaskList[taskNum][TASK_CHANNEL]][CHANNEL_LASTSTATE] = (TaskList[taskNum][TASK_ACTION] == ACTION_TURN_ON);
    check_previous_tasks();
    save_channellist_to_EEPROM();
    }
   }
  }
 previousSecond = curTimeSec; 
 } 
}
