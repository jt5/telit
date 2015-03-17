/*
Скетч для отправки данных температуры на сервис http://narodmon.ru/
Используется плата Ардуино Uno/Duemilanove, шилд «Cosmo GSM Shield» и температурный датчик - DS18B20.
В текущей версии к устройству можно подключить до 4-х температурных датчиков.
В скетче реализовано:
1) Через заданный интервал отправляются показания на сервис «Народный мониторинг»
2) Проверка баланса сим-карты
3) Запрос текущего состояния по SMS, а также настройка интервала отправки данных через SMS.
4) Синхронизация времени
Подробности http://jt5.ru/examples/gprs-telit-narodmon/

Зависимости:
- библиотека Time http://www.pjrc.com/teensy/td_libs_Time.html
- библиотека OneWire http://www.pjrc.com/teensy/td_libs_OneWire.html
- библиотека DallasTemperature http://milesburton.com/Dallas_Temperature_Control_Library
*/

#include <avr/pgmspace.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Time.h>      
#include <TelitGSM.h>
#include <TelitGPRS.h>
#include <Narodmon.h>

char TimeZone = +3; //Ваш часовой пояс относительно GMT+00

#define ALL_INCOMING_SMS_ENABLED (1)            // 1 - разрешен приём команд со всех входящих/ 0 -только с номера CommandID
const char RemoteID[] PROGMEM = "+7XXXXXXXXXX"; //Шаблон номера, на который отправляем СМС
const char CommandID[] PROGMEM = "+7XXXXXXXXXX"; //Шаблон номера, с которого принимаем СМС
const char PIN[] PROGMEM = "0000";              //ПИН-код! Поменяйте на свой!!! Иначе СИМ карта заблокируется!

/*
  Настройки для доступа к GPRS, по умолчанию настройки Мегафона
*/
const char ISP_IP[] PROGMEM =   "internet";         // Для билайна Билайн: "home.beeline.ru", МТС: "internet.mts.ru"
const char LOGIN[] PROGMEM =    "gdata";            // Логин=Пароль
const char BalanceID[] PROGMEM = "*100#";           // USSD номер проверки балланса
const char NTP_IP[] PROGMEM =   "ru.pool.ntp.org";  // Сервер NTP-синхронизации

char IMEI[16];                                      //Буфер для считывания IMEI
char SMStxt[162];                                   // Буфер для чтения текста СМС
char SenderID[16];                                  // Буфер номер отправителя СМС
String strSMS;                                      // Буфер для обработки текста СМС в скетче
signed int MoneyBalance;                            // Переменная в которой хранится текущий денежный баланс
#define MoneyBalanceTreshold      (30)            // Нижний порог отправки СМС о снижении баланса
unsigned int ConnectionCheckTimerMin;             // Таймер отсчёта проверки GPRS соединения
unsigned int DataSendTimerMin;                    // Таймер отсчёта интервал отправки данных
unsigned int NTPSyncTimerMin;                     // Таймер отсчёта интервал NTP синхронизации
unsigned int UnreadSMSChkTimerSec;                // Таймер принудительной проверки непрочитаных СМС
unsigned int BalanceChkTimerMin;                  // Таймер проверки баланса
unsigned int SysTimeSyncTimerMin;                 // Таймер синхронизации часов Ардуино с часами модема
unsigned char ForceModemReInit = 0;               // Флаг принудительной переинициализации модема в случае обнаружения проблем

/*
  Интервалы выполнения задач в минутах
*/
#define  ConnectionCheckPeriodMin (2)         // Проверки GPRS соединения
unsigned char DataSendPeriodMin = 15;         // Отправки данных
#define NTPSyncPeriodMin          (360)        // NTP синхронизации
#define UnreadSMSChkPeriodSec     (10)         // Принудительной проверки непрочитаных СМС
#define BalanceChkPeriodMin       (720)       // Проверки баланса
#define SysTimeSyncPeriodMin      (15)        // Синхронизации часов Ардуино с часами модема

unsigned long currentMillisMin;
unsigned long currentMillisSec;
unsigned long previousMillisMin; 
unsigned long previousMillisSec;  // Маркеры для отсчёта минут
unsigned int MinutesCounter;  // Счётчик минут

#define ONE_WIRE_BUS 10           // Номер линии, к которой подключены датчики температуры
#define TEMPERATURE_PRECISION 9   // Точноть 9 бит

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensor_1, sensor_2, sensor_3, sensor_4; // Используем 4 температурных датчика

/* 
  Настройки narodmon.ru
*/
#define NARODMON_HOST  "narodmon.ru"   // Адрес удаленного сервера сервиса narodmon.ru

void setup() {
	delay(4000); // Начальная задержка
	sensors.begin();   // Поиск 1-wire датчиков

	// считывание адресов 1-wire латчиков
	sensors.getAddress(sensor_1, 0);
	sensors.getAddress(sensor_2, 1);
	sensors.getAddress(sensor_3, 2);
	sensors.getAddress(sensor_4, 3);

	// установка разрядности датчиков:
	sensors.setResolution(sensor_1, TEMPERATURE_PRECISION);
	sensors.setResolution(sensor_2, TEMPERATURE_PRECISION);
	sensors.setResolution(sensor_3, TEMPERATURE_PRECISION);
	sensors.setResolution(sensor_4, TEMPERATURE_PRECISION);

	// опрос всех датчиков на шине
	sensors.requestTemperatures();

	while (Telit.Init(PIN)<0);    // Ждём инициализацию модема
	while (Telit.GetIMEI (IMEI) < 0);      // Копируем IMEI в ОЗУ по указателю IMEI   
	Narodmon.SetDeviceMACbyIMEI(IMEI);     // Конверитруем в MAC  
	while (Telit.ContextInit(ISP_IP) < 0); // Инициализируем контекст
	NTPSyncTime();
	TimeSync(TimeZone);
	Telit.NewSMSindic(1);                   // Разрешаем индикацию новых СМС
	Telit.WaitSMS();
	MinutesCounter = 0;                     //Сброс минут
	previousMillisSec=previousMillisMin=millis();                //Запоминание системного времени
 
	// Ставим большие значения для запуска событий при старте
	ConnectionCheckTimerMin = 16384;
	DataSendTimerMin =        16384;
	NTPSyncTimerMin =         0;
	UnreadSMSChkTimerSec=     16384; 
	BalanceChkTimerMin =      16384;
	SysTimeSyncTimerMin =     0;
}


void loop() {

  unsigned long deltaMillis;
  
  currentMillisMin = currentMillisSec = millis();  // Считывем текущее системное время
  if(currentMillisSec -  previousMillisSec > 1000) 
  { 
    UnreadSMSChkTimerSec  ++;// Прошла 1 сек 
    previousMillisSec = currentMillisSec=millis();
  }
  
  deltaMillis = currentMillisMin - previousMillisMin;   
  if(deltaMillis > 60000) // Прошло 60 сек
  { 
    int deltaMin;
    deltaMin = (int) (deltaMillis / 60000); // Определяем сколько минут прошло
    MinutesCounter += deltaMin;    // Увеличиваем счетчик минут
    // Инкремент счётчиков задания
    ConnectionCheckTimerMin  += deltaMin;
    DataSendTimerMin  += deltaMin;
    NTPSyncTimerMin  += deltaMin;
    //UnreadSMSChkTimerMin  += deltaMin;
    BalanceChkTimerMin  += deltaMin;
    SysTimeSyncTimerMin  += deltaMin;
    // Начало нового отсчёта 60 сек.
    currentMillisMin = previousMillisMin = millis();   
  }
  
  if (ForceModemReInit)
  {
    // Потребовалась переинициализация модема
    ForceModemReInit = 0;
    ModemReInit(); // Принудительно переинициализируем модем
    Telit.WaitSMS();  // Ждем СМС ( TODO: отключение/включение индикации!)
  }
  
  if (Telit.CheckSMS()== 1) // Приходила ли СМС?
  { //Есть новое СМС - считываем в буферы текст и номер отправителя
    Telit.ReadSMS(NewSMS_index, (char*) SMStxt, (char*) SenderID); //
    Telit.DeleteAllSMS();       // Удалить СМСки (накапливать не будем!)
    CommandProcessorSMS();      // Обработка команд в СМС
    Telit.WaitSMS();  // Ждем СМС ( TODO: отключение/включение индикации!)
  }    
  
  
  if (BalanceChkTimerMin >= BalanceChkPeriodMin)
  { // Истек интервал проверки баланса
    BalanceChkTimerMin = 0;
    /*
    #if (MTS_BALANCE == 1)
    if (Telit.ReadBalance((char*)(SMStxt), BalanceID, &MoneyBalance, PSTR("002C")) < 0) MoneyBalance = -9999;// Запрос балланса для МТС   
    #else
    if (Telit.ReadBalance((char*)(SMStxt), BalanceID, &MoneyBalance, PSTR("002E")) < 0) MoneyBalance = -9999;// Запрос балланса для других операторов   
    #endif
    */
    if (Telit.ReadBalance((char*)(SMStxt), BalanceID, &MoneyBalance) < 0) MoneyBalance = -9999;// Запрос балланса 
    if (MoneyBalance < MoneyBalanceTreshold) sendBalanceAlarmSMS(MoneyBalance); // Если балланс ниже порогового, отпрвляем предупредительную СМС
    unsigned char Try = 0;
    while (( Telit.CheckStatus() < 0) && (Try++ < 16)); // Нужно выждать паузу для регистрации в GPRS после проверки балланса.
    Telit.WaitSMS();  // Ждем СМС ( TODO: отключение/включение индикации!)    
  }
  
  if (UnreadSMSChkTimerSec >= UnreadSMSChkPeriodSec)
  { // Истек интервал принудительной проверки СМС
    UnreadSMSChkTimerSec = 0;
    if (Telit.ReadListedSMS((char*) SMStxt, (char*) SenderID) > 0)
    {
      // Нашли пропущенное СМС!     
      Telit.DeleteAllSMS();// Удалить СМСки (накапливать не будем!)
      CommandProcessorSMS();// Обработка команд в СМС
      Telit.WaitSMS();    // Ждем СМС ( TODO: отключение/включение индикации!)
    }
  }
  
  if (ConnectionCheckTimerMin >= ConnectionCheckPeriodMin)
  { // Истек интервал проверки GPRS соединения
    ConnectionCheckTimerMin = 0;
    // Пора проверить состояние GPRS подключения!  
    if ((Telit.CheckStatus() != 1)) 
    { // GPRS соединение не установлено!
     ModemReInit(); // Принудительно переинициализируем модем
    }
    Telit.WaitSMS();  // Ждем СМС ( TODO: отключение/включение индикации!)  
  }
   
  if (DataSendTimerMin  >= DataSendPeriodMin)
  { //Истек интервал отправки данных на сервер
    DataSendTimerMin=0;    
    SendNarodmonData();    //Отправить данные на сервер narodmon.ru
    Telit.WaitSMS();
  }    
  
  if (NTPSyncTimerMin  >= NTPSyncPeriodMin)
  { //Истек интервал cинхронизации с NTP
    NTPSyncTimerMin =0;
    NTPSyncTime();
    Telit.WaitSMS();
  }
  
  
  if (SysTimeSyncTimerMin >= SysTimeSyncPeriodMin)
  {// //Истек интервал синхронизации внутренних часов 
    SysTimeSyncTimerMin = 0;
    TimeSync(TimeZone);
    Telit.WaitSMS();
  }
  
}


String textMessage;
void sendBalanceAlarmSMS(int Money) {
  unsigned int TrySMS = 8; // Количество попыток отправки СМС
  if (Money != -9999)
  {
  textMessage = "Na vashem schete ";
  textMessage += String(Money, DEC);
  textMessage += " RUB!";
  }
  else
  {
      textMessage = "Oshibka opredeleniya balansa! ";
  }
  char Out[textMessage.length()+1];
  textMessage.toCharArray(Out,(textMessage.length())+1);
  //while(Telit.SendSMS(RemoteID, Out) < 0); //TODO: сделать счётчик попыток отправки
   while (TrySMS--)
   {     
     if (Telit.SendSMS(RemoteID, Out) > 0) {ForceModemReInit = 0; break;} // Успешно отправили - выход
     else ForceModemReInit = 1;
     delay(1000); // пауза между попытками
   }   
    unsigned char Try = 0;
    while (( Telit.CheckStatus() < 0) && (Try++ < 16)); // Нужно выждать паузу для регистрации в GPRS после отправки СМС.
}


float ftemp = +0.00;
float ReadSensorFloat (DeviceAddress sensor_addr)
{
          sensors.requestTemperaturesByAddress(sensor_addr);
          return sensors.getTempC(sensor_addr);
}
        
char s[16];
/*
  Отправка СМС с состоянием системы
*/
void sendDataStatus(void) {
  unsigned int TrySMS = 8; // Количество попыток отправки СМС
  textMessage = "Per.: ";
  textMessage += String(DataSendPeriodMin, DEC); // Печать текущего периода отсылки данных
  textMessage += "m.TempSens: ";
  textMessage += "S1= ";
  ftemp = ReadSensorFloat(sensor_1); // Печать показаний датчика 1 (-127 если не подключен)
//  ftemp += 1.0;
  textMessage += dtostrf(ftemp, 2, 2, s);
  textMessage += ",S2= ";
  ftemp = ReadSensorFloat(sensor_2); // Печать показаний датчика 1 (-127 если не подключен)
//  ftemp += 1.1;
  textMessage += dtostrf(ftemp, 2, 2, s);
  textMessage += ",S3= ";
  ftemp = ReadSensorFloat(sensor_3); // Печать показаний датчика 1 (-127 если не подключен)
 
  textMessage += dtostrf(ftemp, 2, 2, s);
  textMessage += ",S4= ";
//  ftemp += 4.3;
  ftemp = ReadSensorFloat(sensor_4); // Печать показаний датчика 1 (-127 если не подключен)
  textMessage += dtostrf(ftemp, 2, 2, s);  
  textMessage += ".Bal:";
  textMessage += String(MoneyBalance, DEC); // Печать баланса на счете
  textMessage += ". ";
  time_t t = now();  //Печать системного времени
  textMessage += uitostr_0(hour(t));
  textMessage += ":";
  textMessage += uitostr_0(minute(t));  
  textMessage += ":";
  textMessage += uitostr_0(second(t));
  textMessage += " ";
  textMessage += uitostr_0(day(t));
  textMessage += "/";  
  textMessage += uitostr_0(month(t));
  char Out[textMessage.length()+1];
  textMessage.toCharArray(Out,(textMessage.length())+1);
  // while(Telit.SendSMS(RemoteID, Out) < 0);  //Отправка СМС TODO: сделать счётчик попыток отправки
    
   while (TrySMS--)
   {
     if (Telit.SendSMS(RemoteID, Out) > 0) {ForceModemReInit = 0; break;}
     else ForceModemReInit = 1;
     delay(1000); // пауза между попытками
   }
  unsigned char Try = 0;
  while (( Telit.CheckStatus() < 0) && (Try++ < 16)); // Нужно выждать паузу для регистрации в GPRS после отправки СМС.
 
}

// Отправка данных на сервер narodmon 
void SendNarodmonData()
{
  unsigned char SensorsNum = 0; // Сброс счётчика работающих датчиков
    signed int SensorData;
    // считываем температуры, заносим в массив          
    if (ReadSensor (sensor_1, &SensorData))
    {  
              Narodmon.SetMACnByIndex(SensorsNum, sensor_1);
              Narodmon.SetDATAByIndex(SensorsNum, SensorData);
              SensorsNum++;
    }
    if (ReadSensor (sensor_2, &SensorData))
    {  
              Narodmon.SetMACnByIndex(SensorsNum, sensor_2);
              Narodmon.SetDATAByIndex(SensorsNum, SensorData);
              SensorsNum++;
    }
    if (ReadSensor (sensor_3, &SensorData))
    {  
              Narodmon.SetMACnByIndex(SensorsNum, sensor_3);
              Narodmon.SetDATAByIndex(SensorsNum, SensorData);
              SensorsNum++;
    }
    if (ReadSensor (sensor_4, &SensorData))
    {  
              Narodmon.SetMACnByIndex(SensorsNum, sensor_4);
              Narodmon.SetDATAByIndex(SensorsNum, SensorData);
              SensorsNum++;
    }
	 
    u8 TxCounter = 0;
   // попытка отправить данные 
   if (SensorsNum)
   {
             Narodmon.SetNumSensors(SensorsNum); // Сообщаем количество датчиков
             // Инициализация GPRS
             u8 Success = 0;             
             do
             {
              if (Telit.ContextActivation(LOGIN, LOGIN) > 0) //Инициализируем контекст
               {
                 // Успешно инициализировали GPRS          
                 // Открываем сокет 1
                 if (Telit.SocketOpen(1 , TCP, PSTR(NARODMON_HOST), 8283, 0) > 0)
                 {
                   // Сокет 1 открылся
                   //Пишем в сокет данные
                   Narodmon.TelnetSend(Telit.Write); 
                   // Закрываем сокет 1
                   if (Telit.SocketClose(1) > 0) Success = 1;
                   else Success = 0;
                   Telit.ContextDeactivation();                   
                 }  else Success = 0; // Что-то пошло не так (сокет не открылся)                  
                } else Success = 0;  // Контекст не активировался               
               if (!Success) {Telit.ContextDeactivation(); delay (5000);} // Подождем 5 секунд между попытками
               if (TxCounter++ == 4) // Пробуем отправить 4 раза
               {
                  TxCounter =0; 
                  return;
              }
             } while (!Success);             
     }
}

//Считывание 1-wire датчика (возврат 1 если успешно / 0 - не удачно)
signed char ReadSensor (DeviceAddress sensor_addr, signed int* pData)
{
          sensors.requestTemperaturesByAddress(sensor_addr);
          float tempS = sensors.getTempC(sensor_addr);
          if (tempS != DEVICE_DISCONNECTED)
          {
            signed int tempS10 = (signed int) (tempS * 10.0);
            *pData = tempS10;
            return 1;
          }
          return 0;
}      

// Повторная аварийная инициализация модема
signed char ModemReInit(void)
{
     while (Telit.Init(PIN)<0); // Делаем инициализацию
     while (Telit.ContextInit(ISP_IP) < 0); // Инициализируем контекст
     NTPSyncTime();
     TimeSync(TimeZone);
     Telit.NewSMSindic(1); // Убрать при проверке по опросу
     return 1;
}


//Синхронизируем время ардуино с модемом
signed int TimeSync(int TZ)
{
     int var_hr ; int var_min; int var_sec; int var_day; int var_month; int var_yr; 
     Telit.getTime(&var_hr, &var_min, &var_sec, &var_day, &var_month, &var_yr); // Считать время из модема
     setTime(var_hr,var_min,var_sec,var_day,var_month,var_yr); //Синхронизируем время в ардуино
     adjustTime ((long) TZ * 60 * 60); // поправка часового пояса
     return 1;
}


//Синхронизируем время в модеме с NTP сервером
signed char NTPSyncTime(void)
{
  int var_hr ; int var_min; int var_sec; int var_day; int var_month; int var_yr;
  if (Telit.ContextActivation(LOGIN, LOGIN) > 0) //Инициализируем контекст
  {
   if ( Telit.NTPSync(NTP_IP, SMStxt) > 0 ) 
   {
     Telit.ContextDeactivation(); 
    return 1;
    }
    else 
    {
      Telit.ContextDeactivation();
      return -1;
    }
  }  else {Telit.ContextDeactivation(); return -2;}
}

// преобразование числа в строку с добавлением нуля если число <10
String uitostr_0 (int a)
{
  if (a >= 10) return String(a, DEC);
  else return ("0"+String(a, DEC));
}


/*
  Пример обработчика команд в тексте входящих СМС
*/
int CommandProcessorSMS()
{
  long parameter;
    if ((strstr_P(SenderID, CommandID)) || (ALL_INCOMING_SMS_ENABLED)) 
    {  // Если номер отправителя совпал
      strSMS = String(SMStxt);
      if (strSMS.startsWith("#CHK_STATUS"))
      {
        sendDataStatus();
      }
      else if (strSMS.startsWith("#SET_DP:"))
      {
        parameter = strSMS.substring(8).toInt();
        if ((parameter > 0) && (parameter < 255)) DataSendPeriodMin = (unsigned char) parameter;
        sendDataStatus();        
      }
    }
}
