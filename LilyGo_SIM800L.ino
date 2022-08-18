//------------------------------------------------------------------------------------
// LIBRARIES
//------------------------------------------------------------------------------------

#include <TinyGPS++.h>
#include <Wire.h>

//------------------------------------------------------------------------------------
// IC CHIP VARIABLES
//------------------------------------------------------------------------------------

#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

//------------------------------------------------------------------------------------
// TTGO T-CALL PINS
//------------------------------------------------------------------------------------

#define SIM800 Serial1

#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22

//------------------------------------------------------------------------------------
// NUMBER PHONE
//------------------------------------------------------------------------------------

//+52 Country Code México
const String PHONE = "+521122334455";

//------------------------------------------------------------------------------------
// RX TX GPS
//------------------------------------------------------------------------------------

#define RXD2 32
#define TXD2 33

//------------------------------------------------------------------------------------
// TINY GPS PLUS
//------------------------------------------------------------------------------------

#define GPS Serial2
TinyGPSPlus gps;

//------------------------------------------------------------------------------------
// SMS VARIABLES
//------------------------------------------------------------------------------------

String smsStatus = "";
String senderNumber = "";
String receivedDate = "";
String msg = "";

//------------------------------------------------------------------------------------
// GPS VARIABLES
//------------------------------------------------------------------------------------

double lat = 0.00;
double lon = 0.00;
double speed = 0.00;
bool locationIsValid = false;

//------------------------------------------------------------------------------------
// SPEED ALARM
//------------------------------------------------------------------------------------

#define INTERVAL_SPEED_TIME 300000L // every 5 minutes it will send a message
const double MAX_SPEED = 60.00;     // km/h
unsigned long last_speed = 0;

bool setPowerBoostKeepOn(int en)
{
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en)
  {
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  }
  else
  {
    Wire.write(0x35); // 0x37 is default reg value
  }
  return Wire.endTransmission() == 0;
}

void setup()
{

  Serial.begin(115200);
  Serial.println("ESP32 LilyGo V1.3 serial initialize");

  // Keep power when running from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isOk = setPowerBoostKeepOn(1);
  Serial.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);
  delay(2000);

  // Set serial config SIM800L pins
  SIM800.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  Serial.println("[SIM800L]: Serial initialize");
  delay(3000);

  // Set serial config SIM800L pins
  GPS.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("[GPS]: Serial initialize");

  SIM800.println("AT+CMGF=1"); // SMS text mode
  delay(1000);
  SIM800.println("AT+CMGD=1,4"); // delete all saved SMS
  delay(1000);
}

static void searchGPS(unsigned long ms)
{

  unsigned long start = millis();

  do
  {

    while (GPS.available() > 0)
    {
      gps.encode(GPS.read());

      if (gps.location.isUpdated())
      {

        if (gps.location.isValid())
        {

          locationIsValid = true;

          lat = gps.location.lat();
          lon = gps.location.lng();
          speed = gps.speed.kmph();
        }
        else
        {
          speed = 0.00;
          locationIsValid = false;
        }
      }
    }

  } while (millis() - start < ms);
}

void loop()
{
  while (SIM800.available())
  {
    parseData(SIM800.readString());
  }

  while (Serial.available())
  {
    SIM800.println(Serial.readString());
  }

  searchGPS(1000);
  speedAlarm();
}

void speedAlarm()
{

  if (millis() - last_speed >= INTERVAL_SPEED_TIME)
  {

    if (locationIsValid)
    {
      if ((speed >= MAX_SPEED))
      {
        sendSpeed(true);
        last_speed = millis();
      }
    }
  }
}

void parseData(String buff)
{
  Serial.println(buff);

  unsigned int len, index;
  //////////////////////////////////////////////////
  // Remove sent "AT Command" from the response string.
  index = buff.indexOf("\r");
  buff.remove(0, index + 2);
  buff.trim();
  //////////////////////////////////////////////////

  //////////////////////////////////////////////////
  if (buff != "OK")
  {
    index = buff.indexOf(":");
    String cmd = buff.substring(0, index);
    cmd.trim();

    buff.remove(0, index + 2);
    // Serial.println(buff);

    if (cmd == "+CMTI")
    {
      // get newly arrived memory location and store it in temp
      // temp = 4
      index = buff.indexOf(",");
      String temp = buff.substring(index + 1, buff.length());
      temp = "AT+CMGR=" + temp + "\r";
      // AT+CMGR=4 i.e. get message stored at memory location 4
      SIM800.println(temp);
    }
    else if (cmd == "+CMGR")
    {
      extractSms(buff);
      senderNumber = ("+52" + senderNumber);
      Serial.println("[Sender Number]: " + senderNumber);
      Serial.println("[PHONE]: " + PHONE);

      if (senderNumber == PHONE)
      {
        Serial.println("[Command]: " + msg);

        if (msg == "location")
        {

          sendLocation();
        }
        else if (msg == "speed")
        {
          sendSpeed(false);
        }
      }

      SIM800.println("AT+CMGD=1,4"); // delete all saved SMS
      delay(1000);
      smsStatus = "";
      senderNumber = "";
      receivedDate = "";
      msg = "";
    }
  }
}

void extractSms(String buff)
{
  unsigned int index;
  Serial.println(buff);

  index = buff.indexOf(",");
  smsStatus = buff.substring(1, index - 1);
  buff.remove(0, index + 2);

  senderNumber = buff.substring(0, 10);
  buff.remove(0, 19);

  receivedDate = buff.substring(0, 20);
  buff.remove(0, buff.indexOf("\r"));
  buff.trim();

  index = buff.indexOf("\n\r");
  buff = buff.substring(0, index);
  buff.trim();
  msg = buff;
  buff = "";
  msg.toLowerCase();
}

void sendLocation()
{

  if (locationIsValid) // If locationIsValid is true
  {
    Serial.println("[Location]: Valid");
    Serial.print("[GPS]: ");
    Serial.print("Latitude= ");
    Serial.print(lat, 6);
    Serial.print(" Longitude= ");
    Serial.println(lon, 6);

    delay(300);

    SIM800.print("AT+CMGF=1\r");
    delay(1000);
    SIM800.print("AT+CMGS=\"" + PHONE + "\"\r");
    delay(1000);
    SIM800.print("http://maps.google.com/maps?q=loc:");
    SIM800.print(gps.location.lat(), 6);
    SIM800.print(",");
    SIM800.print(gps.location.lng(), 6);
    delay(100);
    SIM800.write(0x1A); // ascii code for ctrl-26 //SIM800.println((char)26); //ascii code for ctrl-26
    delay(1000);
    Serial.println("[GPS]: Location SMS Sent Successfully.");
  }
  else
  {
    Serial.println("[Location]: No Valid");
    SIM800.print("AT+CMGF=1\r");
    delay(1000);
    SIM800.print("AT+CMGS=\"" + PHONE + "\"\r");
    delay(1000);
    SIM800.print("[Location]: not available at the moment");
    delay(100);
    SIM800.write(0x1A); // ascii code for ctrl-26 //SIM800.println((char)26); //ascii code for ctrl-26
    delay(1000);
  }
}

void sendSpeed(bool speed_alarm)
{

  if (locationIsValid) // If locationIsValid is true
  {

    Serial.print("[GPS]: ");
    Serial.print("Speed km/h= ");
    Serial.println(gps.speed.kmph());
    delay(300);
    SIM800.print("AT+CMGF=1\r");
    delay(1000);
    SIM800.print("AT+CMGS=\"" + PHONE + "\"\r");
    delay(1000);

    if (speed_alarm)
    {
      SIM800.print("Speed Alarm km/h: ");
    }
    else
    {
      SIM800.print("Speed km/h: ");
    }

    SIM800.print(speed);
    SIM800.print(" http://maps.google.com/maps?q=loc:");
    SIM800.print(lat, 6);
    SIM800.print(",");
    SIM800.print(lon, 6);
    delay(100);
    SIM800.write(0x1A); // ascii code for ctrl-26 //SIM800.println((char)26); //ascii code for ctrl-26
    delay(1000);
    Serial.println("[Speed]: SMS Sent Successfully");
  }
  else
  {

    SIM800.print("AT+CMGF=1\r");
    delay(1000);
    SIM800.print("AT+CMGS=\"" + PHONE + "\"\r");
    delay(1000);
    SIM800.print("[Speed]: not available at the moment");
    delay(100);
    SIM800.write(0x1A); // ascii code for ctrl-26 //SIM800.println((char)26); //ascii code for ctrl-26
    delay(1000);
    Serial.println("[Speed]: not available at the moment");
  }
}
