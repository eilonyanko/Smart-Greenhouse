#include <Keypad.h>

/* Defines */
#define TEMPERATURE_FILTER_LEN 10
#define TEMPERATURE_DIFF_ERROR  10
#define LUX_FILTER_LEN 10
#define GREEN_LINE_MAX_RX_BUFFER 256

/* Arduino Pins */
const float aref_voltage = 3.3;
const int EntranceDoor_ENA = 2;
const int WaterValve_ENB = 3;
const int Shutter_ENA = 4;
const int DoorPushButton = 29;
const int EntranceDoor_in1 = 30;
const int EntranceDoor_in2 = 31;
const int Fan_in1 = 32;
const int Fan_in2 = 33;
const int Fan_in3 = 34;
const int Fan_in4 = 35;
const int WaterValve_in3 = 36;
const int WaterValve_in4 = 37;
const int Shutter_in1 = 38;
const int Shutter_in2 = 39;
const int Lights_in = 40;
const int TempSense1 = A0;
const int TempSense2 = A1;
const int LightSense = A2;

/* Local-time variables */
unsigned int Second = 0;
unsigned int Minute = 0;
unsigned int Hour = 10;
unsigned int Day = 4;

/* Software timers variables */
const unsigned int Timer3OneSecondPreload = 3036; // preload timer 65536-16MHz/256/1Hz
const unsigned int NumTimers = 15;
struct SoftTimer
{
  bool timerEnable;
  unsigned int durationSec;
  void (*callBackFunction)();
};

enum TimersNames
{
  CloseValve=0,
  CloseDoorDelay=1,
  WrongDoorKeyLock=2,
  DoorDriver=3,
  ShutterDriver=4,
  WaterValveDriver=5,
  CheckLightAfterShutterUp=6,
  CheckLightAfterLEDOff=7
};

struct SoftTimer Timers[15] = {{false,0,NULL},{false,0,NULL},{false,0,NULL},{false,0,NULL},{false,0,NULL},{false,0,NULL},{false,0,NULL},{false,0,NULL},{false,0,NULL},{false,0,NULL},{false,0,NULL},{false,0,NULL},{false,0,NULL},{false,0,NULL},{false,0,NULL}};

/* Configuration variables */
struct PlantDayProtocol
{
  bool         dayEnable;
  unsigned int irrigationHour;
  unsigned int irrigationMinute;
  unsigned int duration;
  unsigned int lightStartHour;
  unsigned int lightStartMinute;
  unsigned int lightEndHour;
  unsigned int lightEndMinute;
};
float PlantDryThreshold = 1.0;

struct PlantDayProtocol MyPlantProtocol[7] =  {{true,8,0,600,6,0,20,0},{false,8,0,600,6,0,20,0},{true,8,0,600,6,0,20,0},{false,8,0,600,6,0,20,0},{true,8,0,600,6,0,20,0},{false,8,0,600,6,0,20,0},{true,8,0,600,6,0,20,0}};
int LowTemperature=0;
int HighTemperature=200;
int LowLux = 300;
int ShutterUpDelay = 7;
int ShutterDownDelay = 6;

/* GreenLine variables */
char GLMsgBuffer[GREEN_LINE_MAX_RX_BUFFER];

enum GreenLine_state
{
   SearchSOM, //search start of message
   CollectMsg
};

enum GreenLine_state GLState = SearchSOM;
int GLMsgBufferIndex = 0;

/* Sensors data & Variables */
float TensiometerVoltage;
float BattetyVoltage;
int CurrentTemperature;
int TempFilter[TEMPERATURE_FILTER_LEN] = {0};
int TempFilterIndex = 0;
int TempFilterNumSamples = 0;
int TempFilterSum = 0;
int FilteredTemperature;
int CurrentLuminosity; //in LUX units
int LuxFilter[LUX_FILTER_LEN] = {0};
int LuxFilterIndex = 0;
int LuxFilterNumSamples = 0;
int LuxFilterSum = 0;
int FilteredLux;


/* State variables */
bool FanIsOn = true;
bool LightIsOn = true;
bool ShutterIsUp = true;
bool MakeItDarkerOneShot = true;
bool MakeItBrighterOneShot = true;
bool TimeOfDayNotInitialized = true;

/* XBee variables */
const byte START_DELIMETER = 0x7E;
const byte AT_COMMAND = 0x08;
byte XbeeRxMsg[50];
unsigned int MsgLength;
byte RxByte;
int XbeeRxMsgIndex = 0;
unsigned int RemoteAD0;
unsigned int RemoteAD1;
unsigned int XbeeRSSI;
const byte PanID[] = {0x31, 0x88};
const byte DestinationLow[] = {0x00, 0x00, 0x56, 0x78};
const byte MyAdress[] = {0x12, 0x34};
const byte CoordinatorEn[] = {0x01};
const byte APIEn[] = {0x01};
const byte AESEn[] = {0x01};
const byte AESKey[] = {0x52, 0x6F, 0x66, 0x6F, 0x72, 0x4C, 0x61, 0x6E, 0x64, 0x33, 0x31, 0x38, 0x38, 0x37, 0x39, 0x32};

enum rx_state
{
  FindDelimiter,
  SetLengthHigh,
  SetLengthLow,
  GetMsg
};

struct MsgAPI {
   byte StartDelimeter;
   unsigned int Length;
   byte FrameType;
   byte FrameID;
   char ATCommand[2];
   byte ParameterValue[20];
};

rx_state RxStateMachine = FindDelimiter;

/* Door and keypad variables */
char* password = "3188";

bool WrongKeyLock = false;
bool doorIsLocked;
bool DoorIsClosed = true;
bool IncorrectPassword = false;

const int WrongKeyDelaySeconds = 30;
const int DoorOpenDelaySeconds = 10;
const int WrongTrialsBeforeLock = 3;
int WrongTrialsCnt = WrongTrialsBeforeLock;

int PasswordIndex = 0;

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
{'1','2','3','A'},
{'4','5','6','B'},
{'7','8','9','C'},
{'*','0','#','D'}
};

byte rowPins[ROWS] = {41, 42, 43, 44}; //Four left sockets of the keypad
byte colPins[COLS] = {22, 23, 24, 25}; //Four right sockets of the keypad

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );


/***** Local Time Functions ****/

void SendLocalTime()
{
  char TimeString[10];
  
  Serial.print("~T=");
  sprintf(TimeString, "%02d:%02d", Hour, Minute);
  Serial.println(TimeString);
}

void HandleDayClock()
{
  Second++;
  if (Second == 60)
  {
      Second = 0;
      Minute++;
      if (Minute == 60) {Minute = 0; Hour++;}
      if (Hour == 24) {Hour = 0; Day++;}
      if (Day == 7) Day = 0;
      SendLocalTime();
  } 
}

/*** On/Off functions to the GreenHouse systems ***/

void WaterValveDriverDisable()
{
  digitalWrite(WaterValve_ENB, LOW);  
}

void OpenWaterValve()
{
  Serial.println("~D=Open Water Valve");
  digitalWrite(WaterValve_ENB, HIGH);
  digitalWrite(WaterValve_in3, HIGH);
  digitalWrite(WaterValve_in4, LOW);
  SetSoftTimer(WaterValveDriver, 2, &WaterValveDriverDisable);
}

void CloseWaterValve()
{
  Serial.println("~D=Close Water Valve");
  digitalWrite(WaterValve_ENB, HIGH);
  digitalWrite(WaterValve_in3, LOW);
  digitalWrite(WaterValve_in4, HIGH);
  SetSoftTimer(WaterValveDriver, 2, &WaterValveDriverDisable);  
}

void CloseWaterValveSetup()
{
  Serial.println("~D=Close Water Valve");
  analogWrite(WaterValve_ENB, 255);
  digitalWrite(WaterValve_in3, LOW);
  digitalWrite(WaterValve_in4, HIGH);
  delay(1000);
  analogWrite(WaterValve_ENB, 0);
}

void DoorDriverDisable()
{
  digitalWrite(EntranceDoor_ENA, LOW);  
}

void OpenDoor()
{
  PasswordIndex = 0;
  Serial.println("~D=Door opened");
  digitalWrite(EntranceDoor_ENA, HIGH);
  digitalWrite(EntranceDoor_in1, HIGH);
  digitalWrite(EntranceDoor_in2, LOW);
  SetSoftTimer(DoorDriver, 2, &DoorDriverDisable);
  DoorIsClosed = false;
}

void CloseDoor()
{
  PasswordIndex = 0;
  Serial.println("~D=Door closed");
  digitalWrite(EntranceDoor_ENA, HIGH);
  digitalWrite(EntranceDoor_in1, LOW);
  digitalWrite(EntranceDoor_in2, HIGH);
  SetSoftTimer(DoorDriver, 2, &DoorDriverDisable);   
  DoorIsClosed = true;  
}

void CloseDoorSetup()
{
  PasswordIndex = 0;
  Serial.println("~D=Door closed");
  digitalWrite(EntranceDoor_ENA, HIGH);
  digitalWrite(EntranceDoor_in1, LOW);
  digitalWrite(EntranceDoor_in2, HIGH);
  delay(1000);
  digitalWrite(EntranceDoor_ENA, LOW);
  DoorIsClosed = true;
}

void FansOn()
{
  if (FanIsOn)
    return;
  FanIsOn = true;
  Serial.println("~D=fans on");
  digitalWrite(Fan_in1, LOW);
  digitalWrite(Fan_in2, LOW);
  digitalWrite(Fan_in3, LOW);
  digitalWrite(Fan_in4, LOW);
}

void FansOff()
{
  if (!FanIsOn)
    return;
  FanIsOn = false; 
  Serial.println("~D=fans off");
  digitalWrite(Fan_in1, HIGH);
  digitalWrite(Fan_in2, HIGH);
  digitalWrite(Fan_in3, HIGH);
  digitalWrite(Fan_in4, HIGH);
}

void LightsOn()
{
  if (LightIsOn)
    return;
  LightIsOn = true;
  Serial.println("~D=Lights on");
  digitalWrite(Lights_in, LOW);
}

void LightsOff()
{
  if (!LightIsOn)
    return;
  LightIsOn = false;
  Serial.println("~D=Lights off");
  digitalWrite(Lights_in, HIGH);
}

void ShutterDriverDisable()
{
  analogWrite(Shutter_ENA, 0);
}

void ShutterUp()
{
  if (ShutterIsUp)
    return;
  ShutterIsUp = true;  
  Serial.println("~D=shutter up");
  analogWrite(Shutter_ENA, 79);
  digitalWrite(Shutter_in1, HIGH);
  digitalWrite(Shutter_in2, LOW);
  SetSoftTimer(ShutterDriver, ShutterUpDelay+1, &ShutterDriverDisable);
}

void ShutterDown()
{
  if (!ShutterIsUp)
    return;
  ShutterIsUp = false;
  Serial.println("~D=shutter down");
  analogWrite(Shutter_ENA, 65);
  digitalWrite(Shutter_in1, LOW);
  digitalWrite(Shutter_in2, HIGH);
  SetSoftTimer(ShutterDriver, ShutterDownDelay+1, &ShutterDriverDisable);
}

/*** Software Timers functions ***/
void SetSoftTimer(enum TimersNames timerName, unsigned int duration, void (*ptr)())
{
    Timers[timerName].durationSec = duration;
    Timers[timerName].timerEnable = true;
    Timers[timerName].callBackFunction = ptr;
}

void DisableSoftTimer(enum TimersNames timerName)
{
    Timers[timerName].timerEnable = false;
}

void CheckSoftTimers()
{
 for (int i=0;i<NumTimers;i++)
 {
    if (Timers[i].timerEnable)
    {
        Timers[i].durationSec--;
        if (Timers[i].durationSec == 0)
        {
           Timers[i].timerEnable = false;
          (*Timers[i].callBackFunction) ();
        }
     }
 }
}

/*** GreenLine functions ***/ 

void GreenSeeHandler()
{
  // handle GreenSee comunication
  if (Serial.available())
  { // If data comes in from GreenSee application, coolect it into a buffer
    char gLRxChar = Serial.read();
    GLStateMachine(gLRxChar);
  }
}

void GLStateMachine(char rxChar)
{
   switch(GLState)
   {
     case SearchSOM:
         if(rxChar == '~')
             GLState = CollectMsg;
         GLMsgBufferIndex = 0;
         break;
     case CollectMsg:
         GLMsgBuffer[GLMsgBufferIndex] = rxChar;
         GLMsgBufferIndex++;
         // check buffer overflow 
         if (GLMsgBufferIndex == GREEN_LINE_MAX_RX_BUFFER)
         {
            Serial.println("~D=Got from GreenSee too big message");
            GLState = SearchSOM;
         }
         if(rxChar == 10) //new line character, indicates end of message
         {
            GLState = SearchSOM;
            GLParseMsg();
          }
         break;
    }
}

void GLParseMsg()
{
  String s = "";
  int ind1, ind2 , ind3, ind4, ind5, ind6, ind7, ind8;
  int day;
  String dayS = "", dayEnS = "", hourS = "", minuteS = "", durationS = "", localDayS = "", localHourS = "", localMinuteS = "", localSecondS = "", hourS2 = "", minuteS2 = "", hourS3 = "", minuteS3 = "" ;
  
  for(int i = 2; i < GLMsgBufferIndex; i++)
       s += GLMsgBuffer[i];

  switch(GLMsgBuffer[0])
  {
    case 'I':
        ind1 = s.indexOf(',');
        dayS = s.substring(0, ind1);
        ind2 = s.indexOf(',', ind1+1);
        dayEnS = s.substring(ind1+1, ind2);
        ind3 = s.indexOf(',', ind2+1);
        hourS = s.substring(ind2+1, ind3);
        ind4 = s.indexOf(',', ind3+1);
        minuteS = s.substring(ind3+1, ind4);
        ind5 = s.indexOf(',', ind4+1);
        durationS = s.substring(ind4+1, ind5);
        ind6 = s.indexOf(',', ind5+1);
        hourS2 = s.substring(ind5+1, ind6);
        ind7 = s.indexOf(',', ind6+1);
        minuteS2 = s.substring(ind6+1, ind7);
        ind8 = s.indexOf(',', ind7+1);
        hourS3 = s.substring(ind7+1, ind8);
        minuteS3 = s.substring(ind8+1);
        day = dayS.toInt();
        MyPlantProtocol[day].dayEnable = dayEnS.toInt();
        MyPlantProtocol[day].irrigationHour = hourS.toInt();
        MyPlantProtocol[day].irrigationMinute = minuteS.toInt();
        MyPlantProtocol[day].duration = durationS.toInt();
        MyPlantProtocol[day].lightStartHour = hourS2.toInt();
        MyPlantProtocol[day].lightStartMinute = minuteS2.toInt();
        MyPlantProtocol[day].lightEndHour = hourS3.toInt();
        MyPlantProtocol[day].lightEndMinute = minuteS3.toInt();
        break;
    case 'L':
        LowTemperature = s.toInt();
        break;
    case 'H':
        HighTemperature = s.toInt();
        break;
    case 'U':
        PlantDryThreshold = s.toFloat();
        break;
    case 'V':
        LowLux = s.toInt();
        break;
    case 'Y':
        ShutterUpDelay = s.toInt();
        break;
    case 'Z':
        ShutterDownDelay = s.toInt();
        break;
    case 'T':
        ind1 = s.indexOf(',');
        localDayS = s.substring(0, ind1);
        ind2 = s.indexOf(',', ind1+1 );
        localHourS = s.substring(ind1+1, ind2);
        ind3 = s.indexOf(',', ind2+1 );
        localMinuteS = s.substring(ind2+1, ind3);
        localSecondS = s.substring(ind3+1);
        Day = localDayS.toInt();
        Hour = localHourS.toInt();
        Minute = localMinuteS.toInt();
        Second = localSecondS.toInt();
        SendLocalTime();
        TimeOfDayNotInitialized = false;
        break;
    case 'X':
        // GreenSee send tests 
        int testNum = s.toInt();
        switch(testNum)
        {
           case 1:
              FansOn();
              break;
           case 2:
              FansOff();
              break;
           case 3:
              LightsOn();
              break;
           case 4:
              LightsOff();
              break;
           case 5:
              ShutterUp();
              break;
           case 6:
              ShutterDown();
              break;
           case 7:
              OpenWaterValve();
              break;
           case 8:
              CloseWaterValve();
              break;
           default:
              Serial.println("~D=Unkonwn Test number " + s);
              break;
        }
        break;
     default:
        Serial.println("~D=Unkonwn GreenSee Msg " + s);
        break;
   }
}

/*** XBee functions ***/

unsigned int EndianSwap(int x)
{
  byte MSByte;
  byte LSByte;
  LSByte = x >> 8;
  MSByte = x;
  return ((MSByte << 8) + LSByte);
}

void SendAPIMsg(byte *msg, int len)
{
  byte Checksum = 0;
  Serial.print("~D=AT command send: ");
  for (int i=0;i<len;i++)
  {
    Serial1.write(msg[i]);
    if(i>2)
      Checksum += msg[i];
    Serial.print(msg[i], HEX);
    Serial.print(' ');
  }
  Checksum = 0xff - Checksum;
  Serial1.write(Checksum);
  Serial.print(Checksum, HEX);
  Serial.println(' ');
  delay(200);

  Serial.print("~D=AT command response: ");
  
  while(Serial1.available())
  {
    Serial.print(Serial1.read(), HEX);
    Serial.print(' ');
  }
  Serial.println(' ');
}

void PrepareATCommand(struct MsgAPI* msg, char *ATString, byte *params, int paramLength)
{
  msg->StartDelimeter = START_DELIMETER;
  msg->Length = EndianSwap(paramLength + 4);
  msg->FrameType = AT_COMMAND;
  msg->FrameID = 1;
  msg->ATCommand[0] = ATString[0];
  msg->ATCommand[1] = ATString[1];
  memcpy(msg->ParameterValue, params,paramLength);
}

void ConfigurateXBee()
{
  struct MsgAPI TxMsgATCommand;

  PrepareATCommand(&TxMsgATCommand, "FR", NULL, 0);
  SendAPIMsg((byte*) &TxMsgATCommand, 7);
  
  PrepareATCommand(&TxMsgATCommand, "ID", PanID, sizeof(PanID));
  SendAPIMsg((byte*) &TxMsgATCommand, sizeof(PanID) + 7);

  PrepareATCommand(&TxMsgATCommand, "DL", DestinationLow, sizeof(DestinationLow));
  SendAPIMsg((byte*) &TxMsgATCommand, sizeof(DestinationLow) + 7);

  PrepareATCommand(&TxMsgATCommand, "MY", MyAdress, sizeof(MyAdress));
  SendAPIMsg((byte*) &TxMsgATCommand, sizeof(MyAdress) + 7);

  PrepareATCommand(&TxMsgATCommand, "CE", CoordinatorEn, sizeof(CoordinatorEn));
  SendAPIMsg((byte*) &TxMsgATCommand, sizeof(CoordinatorEn) + 7);

  PrepareATCommand(&TxMsgATCommand, "AP", APIEn, sizeof(APIEn));
  SendAPIMsg((byte*) &TxMsgATCommand, sizeof(APIEn) + 7);

  PrepareATCommand(&TxMsgATCommand, "EE", AESEn, sizeof(AESEn));
  SendAPIMsg((byte*) &TxMsgATCommand, sizeof(AESEn) + 7);

  PrepareATCommand(&TxMsgATCommand, "KY", AESKey, sizeof(AESKey));
  SendAPIMsg((byte*) &TxMsgATCommand, sizeof(AESKey) + 7);

  PrepareATCommand(&TxMsgATCommand, "WR", NULL, 0);
  SendAPIMsg((byte*) &TxMsgATCommand, 7);
}

void XBeeHandler()
{
  while (Serial1.available())   //XBee/UART1/pins 18 and 19
  {
    RxByte = Serial1.read();
    switch(RxStateMachine)
    {
      case FindDelimiter: 
        if(RxByte == START_DELIMETER)
        {
          RxStateMachine = SetLengthHigh;
          XbeeRxMsgIndex = 0;
        }
        break;
        
      case SetLengthHigh:
        MsgLength = RxByte << 8;
        RxStateMachine = SetLengthLow;
        break;
        
      case SetLengthLow:
        MsgLength += RxByte;
        RxStateMachine = GetMsg;
        break;

      case GetMsg:
        XbeeRxMsg[XbeeRxMsgIndex++] = RxByte;
        MsgLength--;
        if(MsgLength == 0)
        {
          RxStateMachine = FindDelimiter;
       
          XbeeRSSI = XbeeRxMsg[3];
          Serial.print("~R=");
          Serial.println(XbeeRSSI);
          
          RemoteAD0 = (XbeeRxMsg[8] << 8) + XbeeRxMsg[9];
          Serial.print("~D=AD0=");
          Serial.print(RemoteAD0);
          TensiometerVoltage = (map(RemoteAD0, 0, 1023, 0, 3300))/1000.0;
          Serial.print(" Tensiometer voltage=");
          Serial.println(TensiometerVoltage, 3);
          Serial.print("~M=");
          Serial.println(TensiometerVoltage, 3);

          RemoteAD1 = (XbeeRxMsg[10] << 8) + XbeeRxMsg[11];
          Serial.print("~D=AD1=");
          Serial.print(RemoteAD1);
          BattetyVoltage = ((map(RemoteAD1, 0, 1023, 0, 3300))/1000.0)*3;
          Serial.print(" Battery voltage=");
          Serial.println(BattetyVoltage, 3);
          Serial.print("~B=");
          Serial.println(BattetyVoltage, 3);
          if(BattetyVoltage < 7.5)
            Serial.println("~D=Low Batterry!");
        }
        break;
    }
  }
}

/*** Irrigation plan function ***/

void CheckIrrigationPlan()
{
  if (TimeOfDayNotInitialized)
    return;
  if(MyPlantProtocol[Day].dayEnable && (MyPlantProtocol[Day].irrigationHour == Hour) && (MyPlantProtocol[Day].irrigationMinute == Minute) && (Second == 0) && (TensiometerVoltage > PlantDryThreshold))
  {
    OpenWaterValve();
    SetSoftTimer(CloseValve, MyPlantProtocol[Day].duration+1, &CloseWaterValve);
  } 
}

/*** Door System functions ***/
void KeyLockRelease()
{
  WrongKeyLock = false;
  WrongTrialsCnt = WrongTrialsBeforeLock;
  Serial.println("~D=Key lock released, reneter password");    
}

void GreenHouseDoorHandler()
{
    char key = keypad.getKey();

  // Check key pressed and open door if code is ok
  if(key && !WrongKeyLock) 
  {
    if (key == '*')
    {
      DisableSoftTimer(CloseDoorDelay);
      CloseDoor();
    }
  
    if(key == password[PasswordIndex])
    {
      PasswordIndex++;
    }
    else
    {
      IncorrectPassword = true;
      PasswordIndex++;
    }

    if(PasswordIndex == 4)
    {
      if(IncorrectPassword)
      {
        WrongTrialsCnt--;
        PasswordIndex = 0;
        IncorrectPassword = false;
        Serial.print("~D=Wrong password trials left: ");
        Serial.println(WrongTrialsCnt);
        if(WrongTrialsCnt == 0)
        {
           WrongKeyLock = true;
           Serial.println("~D=Wrong Key Lock, wait 30 seconds");
           SetSoftTimer(WrongDoorKeyLock, WrongKeyDelaySeconds+1, &KeyLockRelease);      
        }
      }
      else
      {
        OpenDoor();
        SetSoftTimer(CloseDoorDelay, DoorOpenDelaySeconds+1, &CloseDoor);
      }
    }
  }
  
  // internal push button pressed - open door
  if(!digitalRead(DoorPushButton) && DoorIsClosed)
  {
    OpenDoor();
    SetSoftTimer(CloseDoorDelay, DoorOpenDelaySeconds+1, &CloseDoor);
  }
}


/*** Temperature control functions ***/
float AnalogToTemp(int tempReading)
{
  float voltage = (tempReading * aref_voltage)/1023.0;
  float temperature = (voltage - 0.5) * 100.0 ;
  return(temperature);
}

void TempMeasurement()
{
  int tempReading1 = analogRead(TempSense1);  
  int tempReading2 = analogRead(TempSense2);
  float temp1 = AnalogToTemp(tempReading1);
  float temp2 = AnalogToTemp(tempReading2);
  CurrentTemperature = (int) ((temp1 + temp2)/2 + 0.5);
  
  // Filter the temperature by running avvarage
  if (abs(CurrentTemperature - FilteredTemperature) < TEMPERATURE_DIFF_ERROR || TempFilterNumSamples < TEMPERATURE_FILTER_LEN)
  {
    TempFilterSum -= TempFilter[TempFilterIndex];
    TempFilterSum += CurrentTemperature;
    TempFilter[TempFilterIndex] = CurrentTemperature;
    TempFilterIndex++;
    if(TempFilterIndex == TEMPERATURE_FILTER_LEN)
    {
      TempFilterIndex = 0;
    }
    if(TempFilterNumSamples < TEMPERATURE_FILTER_LEN)
      TempFilterNumSamples++;

    // finally the filtered temprature is the average of all samples
    FilteredTemperature = (TempFilterSum + TempFilterNumSamples/2.0) / TempFilterNumSamples; 
  }
  
  Serial.print("~C=");
  Serial.println(FilteredTemperature);

  if (FilteredTemperature >= HighTemperature)
  {
      FansOn();
  }

  if (FilteredTemperature <= LowTemperature)
  {
      FansOff(); 
  }
}

/*** Light control functions ***/
void LightLEDsAfterShutter()
{
  if (FilteredLux < LowLux)
    {
      LightsOn();         
    }
}

void ShutterDownAfterLEDs()
{
  ShutterDown();            
}

void LuminosityMeasurement()
{
  int currentDayMinutes = Hour * 60 + Minute;
  int lightStartDayMinutes = MyPlantProtocol[Day].lightStartHour * 60 + MyPlantProtocol[Day].lightStartMinute;
  int lightEndDayMinutes = MyPlantProtocol[Day].lightEndHour * 60 + MyPlantProtocol[Day].lightEndMinute;

  int ldrsensor=analogRead(LightSense);
  if(ldrsensor < 485)
    CurrentLuminosity = map(ldrsensor, 39, 485, 1000, 10);
  else
    CurrentLuminosity = map(ldrsensor, 485, 1013, 10, 0);

  
   // Filter the Lux by running avvarage
  
  LuxFilterSum -= LuxFilter[LuxFilterIndex];
  LuxFilterSum += CurrentLuminosity;
  LuxFilter[LuxFilterIndex] = CurrentLuminosity;
  LuxFilterIndex++;
  if(LuxFilterIndex == LUX_FILTER_LEN)
  {
    LuxFilterIndex = 0;
  }
  if(LuxFilterNumSamples < LUX_FILTER_LEN)
    LuxFilterNumSamples++;

  // finally the filtered temprature is the average of all samples
  FilteredLux = (LuxFilterSum + LuxFilterNumSamples/2.0) / LuxFilterNumSamples; 

  Serial.print("~L=");
  Serial.println(FilteredLux);

  // Light automation
  if (TimeOfDayNotInitialized)
    return;
    
  if(currentDayMinutes > lightStartDayMinutes && currentDayMinutes < lightEndDayMinutes)
  {
    /* Plant need light */
    MakeItDarkerOneShot = true;
    if (FilteredLux < LowLux && MakeItBrighterOneShot)
    {
      MakeItBrighterOneShot = false;
      ShutterUp();
      SetSoftTimer(CheckLightAfterShutterUp, ShutterUpDelay + 5, &LightLEDsAfterShutter);      
    }
  }
  else
  {
    MakeItBrighterOneShot = true;
    /* Plant need darkness */
    if (MakeItDarkerOneShot)
    {
      MakeItDarkerOneShot = false;
      LightsOff();
      SetSoftTimer(CheckLightAfterLEDOff, 3, &ShutterDownAfterLEDs);
    }    
  }
}

/*** Setup function ***/
void setup()
{
  pinMode(Fan_in1, OUTPUT);
  pinMode(Fan_in2, OUTPUT);
  pinMode(Fan_in3, OUTPUT);
  pinMode(Fan_in4, OUTPUT);
  pinMode(Lights_in, OUTPUT);
  FansOff();
  LightsOff();
  Serial.begin(9600);
  Serial1.begin(9600);  //XBee/UART1/pins 18 and 19
  Serial.println("~D=Hello GreenSee");
  pinMode(WaterValve_ENB, OUTPUT);
  pinMode(WaterValve_in3, OUTPUT);
  pinMode(WaterValve_in4, OUTPUT);
  pinMode(EntranceDoor_ENA, OUTPUT);
  pinMode(EntranceDoor_in1, OUTPUT);
  pinMode(EntranceDoor_in2, OUTPUT);
 
  pinMode(Shutter_ENA, OUTPUT);
  pinMode(Shutter_in1, OUTPUT);
  pinMode(Shutter_in2, OUTPUT);
  analogReference(EXTERNAL);
  pinMode(DoorPushButton, INPUT_PULLUP);

  CloseWaterValveSetup();
  CloseDoorSetup();

  ConfigurateXBee();

  Serial.println("~D=GreenDo V1.4 Ready");

  // initialize timer3 
  noInterrupts(); // disable all interrupts
  TCCR3A = 0;
  TCCR3B = 0;

  TCNT3 = Timer3OneSecondPreload;            
  TCCR3B |= (1 << CS12) ; // 256 prescaler 
  TIMSK3 |= (1 << TOIE3); // enable timer overflow interrupt
  interrupts(); // enable all interrupts
}

/*** Interrupt Service Routine ***/
ISR(TIMER3_OVF_vect)        
{
  TCNT3 = Timer3OneSecondPreload; 
  HandleDayClock();
  CheckIrrigationPlan();
  TempMeasurement(); 
  LuminosityMeasurement();
  CheckSoftTimers(); 
  XBeeHandler();
  
}

/*** Loop function ***/ 
void loop()
{
  GreenSeeHandler();
  GreenHouseDoorHandler();
}
