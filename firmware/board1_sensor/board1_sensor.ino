

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <math.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C


#define SO2_PIN 34
#define CO_PIN  35
#define O3_PIN  32
#define VOC_PIN 33
#define NO2_PIN 36
#define DHT_PIN 27
#define DHT_TYPE DHT22


#define LED_GREEN  13
#define LED_YELLOW 14
#define LED_RED    12
#define BUZZER     25
#define TX_TO_BOARD2 26


float CO_baseline  = 0.30;
float O3_baseline  = 0.50;
float NO2_baseline = 3.00;
float SO2_baseline = 2.30;


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BMP280 bmp;
DHT dht(DHT_PIN, DHT_TYPE);


bool bmpFound = false;
HardwareSerial pmsSerial(2);


struct PMSData { uint16_t pm1, pm25, pm10; bool valid; } pms = {0,0,0,false};


float g_temp=0, g_pres=0, g_hum=0;
float g_co_v=0, g_o3_v=0, g_voc_v=0, g_so2_v=0, g_no2_v=0;
float g_coC=0, g_o3C=0, g_no2C=0, g_so2C=0;
int   g_aqi=0;


void readPMS() {
  pms.valid = false;
  while (pmsSerial.available() >= 32) {
    if (pmsSerial.peek() != 0x42) { pmsSerial.read(); continue; }
    uint8_t buf[32];
    pmsSerial.readBytes(buf, 32);
    if (buf[1] != 0x4D) continue;
    uint16_t sum = 0;
    for (int i = 0; i < 30; i++) sum += buf[i];
    uint16_t checksum = (buf[30] << 8) | buf[31];
    if (sum != checksum) continue;
    pms.pm1  = (buf[4] << 8) | buf[5];
    pms.pm25 = (buf[6] << 8) | buf[7];
    pms.pm10 = (buf[8] << 8) | buf[9];
    pms.valid = true;
  }
}


float readAvg(int pin, float mult) {
  long sum = 0;
  for (int i = 0; i < 20; i++) { sum += analogRead(pin); delay(2); }
  return (sum / 20.0 / 4095.0) * 3.3 * mult;
}


float calcAQI(float C, float Clow, float Chigh, float Ilow, float Ihigh) {
  if (C < Clow) C = Clow;
  if (C > Chigh) C = Chigh;
  return ((Ihigh - Ilow) / (Chigh - Clow)) * (C - Clow) + Ilow;
}
float aqiPM25(float c){
  if(c<=12.0)return calcAQI(c,0,12,0,50);
  if(c<=35.4)return calcAQI(c,12.1,35.4,51,100);
  if(c<=55.4)return calcAQI(c,35.5,55.4,101,150);
  if(c<=150.4)return calcAQI(c,55.5,150.4,151,200);
  if(c<=250.4)return calcAQI(c,150.5,250.4,201,300);
  return calcAQI(c,250.5,500.4,301,500);
}
float aqiPM10(float c){
  if(c<=54)return calcAQI(c,0,54,0,50);
  if(c<=154)return calcAQI(c,55,154,51,100);
  if(c<=254)return calcAQI(c,155,254,101,150);
  if(c<=354)return calcAQI(c,255,354,151,200);
  if(c<=424)return calcAQI(c,355,424,201,300);
  return calcAQI(c,425,604,301,500);
}
float aqiCO(float c){
  if(c<=4.4)return calcAQI(c,0,4.4,0,50);
  if(c<=9.4)return calcAQI(c,4.5,9.4,51,100);
  if(c<=12.4)return calcAQI(c,9.5,12.4,101,150);
  if(c<=15.4)return calcAQI(c,12.5,15.4,151,200);
  if(c<=30.4)return calcAQI(c,15.5,30.4,201,300);
  return calcAQI(c,30.5,50.4,301,500);
}
float aqiO3(float c){
  if(c<=54)return calcAQI(c,0,54,0,50);
  if(c<=70)return calcAQI(c,55,70,51,100);
  if(c<=85)return calcAQI(c,71,85,101,150);
  if(c<=105)return calcAQI(c,86,105,151,200);
  if(c<=200)return calcAQI(c,106,200,201,300);
  return calcAQI(c,201,504,301,500);
}
float aqiNO2(float c){
  if(c<=53)return calcAQI(c,0,53,0,50);
  if(c<=100)return calcAQI(c,54,100,51,100);
  if(c<=360)return calcAQI(c,101,360,101,150);
  if(c<=649)return calcAQI(c,361,649,151,200);
  if(c<=1249)return calcAQI(c,650,1249,201,300);
  return calcAQI(c,1250,2049,301,500);
}
float aqiSO2(float c){
  if(c<=35)return calcAQI(c,0,35,0,50);
  if(c<=75)return calcAQI(c,36,75,51,100);
  if(c<=185)return calcAQI(c,76,185,101,150);
  if(c<=304)return calcAQI(c,186,304,151,200);
  if(c<=604)return calcAQI(c,305,604,201,300);
  return calcAQI(c,605,1004,301,500);
}


float co_ppm(float v){float d=v-CO_baseline;return d>0?d*10.0:0;}
float o3_ppb(float v){float d=v-O3_baseline;return d>0?d*50.0:0;}
float no2_ppb(float v){float d=v-NO2_baseline;return d>0?d*50.0:0;}
float so2_ppb(float v){float d=v-SO2_baseline;return d>0?d*50.0:0;}


const char* category(int aqi){
  if(aqi<=50)return "Good";
  if(aqi<=100)return "Moderate";
  if(aqi<=150)return "Unhealthy SG";
  if(aqi<=200)return "Unhealthy";
  if(aqi<=300)return "V.Unhealthy";
  return "Hazardous";
}
const char* advice(int aqi){
  if(aqi<=50)return "Air clean. Enjoy outside.";
  if(aqi<=100)return "OK. Sensitive take care.";
  if(aqi<=150)return "Sensitive limit outdoor.";
  if(aqi<=200)return "Reduce outdoor. Mask up.";
  if(aqi<=300)return "Avoid outdoors. Use mask.";
  return "Stay indoors. Emergency!";
}


void setLEDs(int aqi){
  digitalWrite(LED_GREEN,LOW);
  digitalWrite(LED_YELLOW,LOW);
  digitalWrite(LED_RED,LOW);
  if(aqi<=100)digitalWrite(LED_GREEN,HIGH);
  else if(aqi<=200)digitalWrite(LED_YELLOW,HIGH);
  else digitalWrite(LED_RED,HIGH);
  if(aqi>200)tone(BUZZER,1000,300);
}


void sendToBoard2(){
  String line = String(g_aqi) + "|";
  line += String(category(g_aqi)) + "|";
  line += String(pms.pm25) + "|";
  line += String(pms.pm10) + "|";
  line += String(pms.pm1) + "|";
  line += String(g_co_v,2) + "|";   // RAW VOLTAGE
  line += String(g_o3_v,2) + "|";   // RAW VOLTAGE
  line += String(g_no2_v,2) + "|";
  line += String(g_so2_v,2) + "|";
  line += String(g_voc_v,2) + "|";
  line += String(g_temp,1) + "|";
  line += String(g_hum,0) + "|";
  line += String(g_pres,0);
  Serial1.println(line);
  Serial.print("Sent: "); Serial.println(line);
}


void updateReadings(){
  g_co_v  = readAvg(CO_PIN,2.0);
  g_o3_v  = readAvg(O3_PIN,2.0);
  g_voc_v = readAvg(VOC_PIN,2.0);
  g_so2_v = readAvg(SO2_PIN,2.0);
  g_no2_v = readAvg(NO2_PIN,1.0);


  // BMP280 with bounds checking
 if(bmpFound){
  float raw_t = bmp.readTemperature();
  float raw_p = bmp.readPressure()/100.0F;
  if(raw_t > 5 && raw_t < 55)    g_temp = raw_t;
  if(raw_p > 950 && raw_p < 1050) g_pres = raw_p;
}


  float h = dht.readHumidity();
  if(!isnan(h)) g_hum = h;


  readPMS();


  // Convert for AQI calculation (internal use only)
  g_coC  = co_ppm(g_co_v);
  g_o3C  = o3_ppb(g_o3_v);
  g_no2C = no2_ppb(g_no2_v);
  g_so2C = so2_ppb(g_so2_v);


  // AQI uses all 6 pollutants as per supervisor
  float aqi = aqiPM25(pms.pm25);
  float t;
  t=aqiPM10(pms.pm10); if(t>aqi)aqi=t;
  t=aqiCO(g_coC);      if(t>aqi)aqi=t;
  t=aqiO3(g_o3C);      if(t>aqi)aqi=t;
  t=aqiNO2(g_no2C);    if(t>aqi)aqi=t;
  t=aqiSO2(g_so2C);    if(t>aqi)aqi=t;
  g_aqi = (int)(aqi+0.5);


  setLEDs(g_aqi);
  sendToBoard2();


  Serial.print("AQI:");Serial.print(g_aqi);
  Serial.print(" PM2.5:");Serial.print(pms.pm25);
  Serial.print(" PM10:");Serial.print(pms.pm10);
  Serial.print(" CO_V:");Serial.print(g_co_v,2);
  Serial.print(" O3_V:");Serial.print(g_o3_v,2);
  Serial.print(" T:");Serial.print(g_temp,1);
  Serial.print(" H:");Serial.print(g_hum,0);
  Serial.print(" P:");Serial.println(g_pres,0);
}


void pageName(){
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2); display.setCursor(18,18); display.println("AQ-Sense");
  display.setTextSize(1); display.setCursor(20,44); display.println("Air Monitor BD");
  display.display();
}
void pageEnv(){
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0);  display.println("-- ENVIRONMENT --");
  display.setCursor(0,16); display.print("Temp:     "); display.print(g_temp,1); display.println(" C");
  display.setCursor(0,28); display.print("Humidity: "); display.print(g_hum,0); display.println(" %");
  display.setCursor(0,40); display.print("Pressure: "); display.print(g_pres,0); display.println(" hPa");
  display.display();
}
void pagePM(){
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0);  display.println("-- PARTICULATE --");
  display.setCursor(0,16); display.print("PM2.5: "); display.print(pms.pm25); display.println(" ug/m3");
  display.setCursor(0,28); display.print("PM10 : "); display.print(pms.pm10); display.println(" ug/m3");
  display.setCursor(0,40); display.print("PM1.0: "); display.print(pms.pm1);  display.println(" ug/m3");
  display.display();
}
void pageCO_O3(){
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0);  display.println("-- CO / OZONE --");
  display.setCursor(0,16); display.print("CO: "); display.print(g_co_v,2); display.println(" V");
  display.setCursor(0,28); display.print("O3: "); display.print(g_o3_v,2); display.println(" V");
  display.setCursor(0,48); display.println("*Raw voltage,");
 
  display.display();
}
void pageNO2SO2(){
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0);  display.println("-- NO2 / SO2 --");
  display.setCursor(0,16); display.print("NO2: "); display.print(g_no2_v,2); display.println(" V");
  display.setCursor(0,28); display.print("SO2: "); display.print(g_so2_v,2); display.println(" V");
  display.setCursor(0,48); display.println("*Qualitative");
 
  display.display();
}
void pageVOC(){
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0);  display.println("-- VOC INDEX --");
  display.setCursor(0,16); display.print("VOC: "); display.print(g_voc_v,2); display.println(" V");
  display.setCursor(0,40); display.println("*MQ-135 composite");
  display.setCursor(0,48); display.println(" VOC index value,");
  display.setCursor(0,56); display.println(" not absolute ppm");
  display.display();
}
void pageAQI(){
  float a_pm25=aqiPM25(pms.pm25);
  float a_pm10=aqiPM10(pms.pm10);
  float a_co=aqiCO(g_coC);
  float a_o3=aqiO3(g_o3C);
  float a_no2=aqiNO2(g_no2C);
  float a_so2=aqiSO2(g_so2C);
  String dominant="PM2.5";
  float highest=a_pm25;
  if(a_pm10>highest){highest=a_pm10;dominant="PM10";}
  if(a_co>highest){highest=a_co;dominant="CO";}
  if(a_o3>highest){highest=a_o3;dominant="O3";}
  if(a_no2>highest){highest=a_no2;dominant="NO2";}
  if(a_so2>highest){dominant="SO2";}


  display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1); display.setCursor(0,0);
  display.println("AIR QUALITY INDEX");
  display.setTextSize(3); display.setCursor(0,14);
  display.println(g_aqi);
  display.setTextSize(1); display.setCursor(0,46);
  display.print("Driver: "); display.println(dominant);
  display.setCursor(0,56); display.print(">> "); display.println(category(g_aqi));
  display.display();
}
void pageGauge(){
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE);
  int cx=64, cy=58, r=44;
  for(int a=0;a<=180;a+=4){float rad=a*PI/180.0;int x=cx-cos(rad)*r;int y=cy-sin(rad)*r;display.drawPixel(x,y,SSD1306_WHITE);}
  int bounds[]={0,50,100,150,200,300};
  for(int i=0;i<6;i++){float frac=bounds[i]/300.0;if(frac>1)frac=1;float a=180-(frac*180);float rad=a*PI/180.0;int x1=cx-cos(rad)*r,y1=cy-sin(rad)*r;int x2=cx-cos(rad)*(r-6),y2=cy-sin(rad)*(r-6);display.drawLine(x1,y1,x2,y2,SSD1306_WHITE);}
  int av=g_aqi;if(av>300)av=300;float frac=av/300.0;float a=180-(frac*180);float rad=a*PI/180.0;
  int nx=cx-cos(rad)*(r-4),ny=cy-sin(rad)*(r-4);
  display.drawLine(cx,cy,nx,ny,SSD1306_WHITE); display.fillCircle(cx,cy,3,SSD1306_WHITE);
  display.setTextSize(1); display.setCursor(0,0); display.print("AQI "); display.print(g_aqi);
  display.setCursor(60,0); display.print(category(g_aqi));
  display.display();
}
void pageAdvice(){
  display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
  display.setCursor(0,0);  display.println("-- HEALTH ADVICE --");
  display.setCursor(0,18); display.print("AQI "); display.print(g_aqi);
  display.print(" "); display.println(category(g_aqi));
  display.setCursor(0,38); display.println(advice(g_aqi));
  display.display();
}


int page=0;
const int NUM_PAGES=9;
unsigned long lastPage=0;
unsigned long lastRead=0;


void setup(){
  Serial.begin(115200);
  delay(500);
  analogReadResolution(12);
  pmsSerial.begin(9600,SERIAL_8N1,16,17);
  Serial1.begin(9600,SERIAL_8N1,-1,TX_TO_BOARD2);
  pinMode(LED_GREEN,OUTPUT);
  pinMode(LED_YELLOW,OUTPUT);
  pinMode(LED_RED,OUTPUT);


  if(!display.begin(SSD1306_SWITCHCAPVCC,SCREEN_ADDRESS)){while(true);}


  if(bmp.begin(0x76))bmpFound=true;
  else if(bmp.begin(0x77))bmpFound=true;
  if(bmpFound){
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  }
  dht.begin();
  pageName();
  delay(1500);
  updateReadings();
}


void loop(){
  if(millis()-lastRead > 5000){ updateReadings(); lastRead=millis(); }
  if(millis()-lastPage > 3000){ page=(page+1)%NUM_PAGES; lastPage=millis(); }
  switch(page){
    case 0: pageName();    break;
    case 1: pageEnv();     break;
    case 2: pagePM();      break;
    case 3: pageCO_O3();   break;
    case 4: pageNO2SO2();  break;
    case 5: pageVOC();     break;
    case 6: pageAQI();     break;
    case 7: pageGauge();   break;
    case 8: pageAdvice();  break;
  }
  delay(50);
}

