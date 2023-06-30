#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "HX711.h"
#include "MAX30100_PulseOximeter.h"
#include <LiquidCrystal_I2C.h>

// ID Gay
const char* ID = "m1q2a6g2gs754gd3";

// Khai báo thông tin WiFi
const char* ssid = "PHAN VAN THIEN";
const char* password = "0798106167";

// Khai báo thông tin MQTT broker
const char* mqtt_server = "103.130.211.150";
const int mqtt_port = 10040;
const char* mqtt_user = "phunghx";
const char* mqtt_pass = "nckh";

// Khởi tạo đối tượng WiFiClient và PubSubClient
WiFiClient espClient;
PubSubClient client(espClient);

#define btnPin 14
#define REPORTING_PERIOD_MS     1000

const int LOADCELL_DOUT_PIN = 0;
const int LOADCELL_SCK_PIN = 2;

HX711 scale;
LiquidCrystal_I2C lcd(0x27, 20, 4);
PulseOximeter pox;

uint32_t tsLastReport = 0;
uint8_t btn_prev;

// Khai bao topic
const char* topicoxi = "/patient/m1q2a6g2gs754gd3/mornitor/oxi";
const char* topicheart = "/patient/m1q2a6g2gs754gd3/mornitor/heartRate";
const char* topicluc = "/patient/m1q2a6g2gs754gd3/mornitor/grip";
const char* topicavg = "/patient/m1q2a6g2gs754gd3/estimate/avg";
const char* topictimMach = "/patient/m1q2a6g2gs754gd3/estimate/timMach";
const char* topicnhoiMau = "/patient/m1q2a6g2gs754gd3/estimate/nhoiMau";
const char* topicdotQuy = "/patient/m1q2a6g2gs754gd3/estimate/dotQuy";

// Du doan suc khoe
float saveMale = 30.2;
float saveFe = 24.3;
bool isMale = true;
float arrMaxStrength[5];
bool Dudoan = false;
int resetmax = 20;

void PushArr(float arr[], float data)
{
  int i = 0;
  while (i < sizeof(arr))
  {
    arr[i] = arr[i + 1];
    i++;
  }
  arr[i] = data;
}
void setup_wifi()
{
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//Start of DuDoanSucKhoe
float FindAvgGripStrength()
{
  float sum = 0;
  for (int i = 0; i < 5; i++)
  {
    sum += arrMaxStrength[i];
  }
  return sum / 5.0;
}

void DuDoanSucKhoe()
{
  float gripStrengthAvg = FindAvgGripStrength();
  String avg = String(gripStrengthAvg);
  lcd.setCursor(0, 3);
  lcd.print("AVG: ");
  lcd.print(avg + " KG");
  int heSo = 0;
  if (isMale == true)
  {
    if (saveMale >= gripStrengthAvg)
    {
      heSo = (saveMale - gripStrengthAvg) / 5;

    }
  }
  else if (isMale == false)
  {
    if (saveFe >= gripStrengthAvg)
    {
      heSo = (saveFe - gripStrengthAvg) / 5;
    }
  }
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(String(heSo * 17) + "%" + " ti le benh tim");
  lcd.setCursor(0, 1);
  lcd.print(String(heSo * 7) + "%" + " ti le dau tim");
  lcd.setCursor(0, 2);
  lcd.print(String(heSo * 9) + "%" + " ti le dot quy");

  client.publish(topicavg,String(gripStrengthAvg).c_str());
}
void SetGripStrength()
{
  for (int i = 0; i < 5; i++)
  {
    String number = String(i + 1);
    lcd.setCursor(0, 1);
    lcd.print("Lan " + number);
    float luc;
    do
    {
      luc = scale.get_units();
      arrMaxStrength[i] = luc;
    } while (luc < 5);

    while (luc > 5)
    {
      luc = scale.get_units();
      char lucNamTay[4];
      if (luc > arrMaxStrength[i])
      {
        arrMaxStrength[i] = luc;
        lcd.setCursor(0, 2);
        lcd.print("Luc: ");
        lcd.print(dtostrf(luc, 3, 0, lucNamTay));
      }
    }
  }
  DuDoanSucKhoe();
  delay(3000);
  lcd.clear();
  Dudoan = false;
}
//End of DuDoanSucKhoe()

bool GetValueFromMax()
{
  float oxi = pox.getSpO2();
  float heart = pox.getHeartRate();
  do {
    pox.update();
    oxi = pox.getSpO2();
    heart = pox.getHeartRate();
  }while(oxi == 0);
  
  if (pox.getSpO2() != 0)
  {
    lcd.setCursor(0, 1);
    lcd.print("HeartRate: " + String(heart) + "bpm ");
    lcd.setCursor(0, 2);
    lcd.print("Oxigen: " + String(oxi) + "%");
    client.publish( topicheart,String(heart).c_str() );
    client.publish( topicoxi,String(oxi).c_str() );
    return true;
  }
  return false;
}

void GetValueFromHX()
{
  float luc = scale.get_units();
  if ( luc > 0)
  {
    Serial.println(luc);
    lcd.setCursor(0, 3);
    lcd.print("GripStrength:" + String(luc) + "KG");
    client.publish(topicluc,String(luc).c_str());
  }
}
void connectMQTT(){
  client.setServer(mqtt_server, mqtt_port);
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 1 seconds");
      delay(1000);
    }
  }
}
void setupMax(){
  if (!pox.begin())
    {
        Serial.println("FAILED");
        for (;;);
    }
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
}
void setup() {
  // Khởi tạo Serial
  Serial.begin(9600);
  //Lcd and nut bam chuyen che do do
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Setting up");
  
  pinMode(btnPin, INPUT_PULLUP);
  btn_prev = digitalRead(btnPin);
  // Kết nối WiFi
  setup_wifi();
  // Kết nối MQTT broker
  connectMQTT();
  //linh kien
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(103450);
  setupMax();
}

void loop() {
  uint8_t btn = digitalRead(btnPin);
    if (btn == LOW && btn_prev == HIGH)
    {
        Dudoan=true;
    }
    btn_prev = digitalRead(btnPin);
    if (Dudoan == true)
    {
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Du Doan Suc Khoe");
        SetGripStrength();
        if (!pox.begin())
        {
            Serial.println("FAILED");
            for (;;);
        }
        else
        {
            Serial.println("SUCCESS");
        }
        pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    }
    pox.update();
    lcd.setCursor(0,0);
    lcd.print("Theo Doi Suc Khoe");
    if (millis() - tsLastReport > REPORTING_PERIOD_MS)
    {
        if(GetValueFromMax()==true)
        {
            GetValueFromHX();
            setupMax();
        }
        else
        {
            lcd.clear();
            lcd.setCursor(0,1);
            lcd.print("Chua the lay gia tri");
        }
        tsLastReport = millis();
    }
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();
}
