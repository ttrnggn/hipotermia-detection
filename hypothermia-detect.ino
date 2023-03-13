#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "LowPower.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define DS18B20PIN 2
#define wakeUpPin 3
#define buzzerPin 4

MAX30105 particleSensor;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
OneWire oneWire(DS18B20PIN);
DallasTemperature sensor(&oneWire);

const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred
float beatsPerMinute;
int beatAvg;
int isiBPM;
int hitungSamaSuhu = 0;

unsigned long mulai = 0;
unsigned long berhenti = 0;
unsigned long mulai2 = 0;
unsigned long berhenti2 = 0;
unsigned long berhenti3 = 0;
bool hitung = false;
bool hitung2 = false;
bool sudahklasifikasi = false;
bool dapatBPM = false;
bool dapatSuhu = false;

float tempinC;
float defu;
float isiSuhu;
float isiSuhuPrev;
float suhufix;
String simpan;

bool sleep = false;

float anggotaJantung[3]; //0:L, 1:N, 2:C
float anggotaSuhu[4]; //0:SD, 1:D, 2:AD, 3:N
float anggotaOutput[4]; //0:berat, 1:sedang, 2:ringan, 3:normal
float alpha[8];
float z[8];
float Eaizi = 0;
float Eai = 0;

ISR(TIMER1_COMPA_vect)
{
  TCNT1H = 0x0B;
  TCNT1L = 0xDC;
  sensor.requestTemperatures();
  tempinC = sensor.getTempCByIndex(0);
  
  isiSuhuPrev = isiSuhu;
  isiSuhu = tempinC;
  if (isiSuhu == isiSuhuPrev){
    hitungSamaSuhu++;
  }
  else {
    hitungSamaSuhu = 0;
  }
  
}

void keanggotaanJantung(float x){
  //lambat
  if (x <= 0 || x >= 60){
    anggotaJantung[0] = 0;
  }
  else if (x >= 50 && x <= 60){
    anggotaJantung[0] = ((60 - x) / (60 - 50));
  }
  else if (x >= 0 && x <= 50){
    anggotaJantung[0] = 1;
  }

  //Normal/sedang
  if(x <= 50 || x >= 110){
    anggotaJantung[1] = 0;
  }
  else if(x >= 100 && x <= 110){
    anggotaJantung[1] = ((110 - x) / (110 - 100));
  }
  else if(x >= 50 && x <= 60){
    anggotaJantung[1] = ((x - 50) / (60 - 50));
  }
  else if(x >= 60 && x <= 100){
    anggotaJantung[1] = 1;
  }

  //Cepat
  if(x <= 100 || x >= 200){
    anggotaJantung[2] = 0;
  }
  else if(x >= 100 && x <= 110){
    anggotaJantung[2] = ((x - 100) / (110 - 100));
  }
  else if(x >= 110 && x <= 200){
    anggotaJantung[2] = 1;
  }
}

void keanggotaanSuhu(float y){
  //Sangat Dingin
  if (y <= 20 || y >= 29){
    anggotaSuhu[0] = 0;
  }
  else if (y >= 28 && y <= 29){
    anggotaSuhu[0] = ((29 - y) / (29 - 28));
  }
  else if (y >= 20 && y <= 28){
    anggotaSuhu[0] = 1;
  }

  //Dingin
  if(y <= 28 || y >= 33){
    anggotaSuhu[1] = 0;
  }
  else if(y >= 32 && y <= 33){
    anggotaSuhu[1] = ((33 - y) / (33 - 32));
  }
  else if(y >= 28 && y <= 29){
    anggotaSuhu[1] = ((y - 28) / (29 - 28));
  }
  else if(y >= 29 && y <= 32){
    anggotaSuhu[1] = 1;
  }

  //Agak Dingin
  if(y <= 32 || y >= 36){
    anggotaSuhu[2] = 0;
  }
  else if(y >= 35 && y <= 36){
    anggotaSuhu[2] = ((36 - y) / (36 - 35));
  }
  else if(y >= 32 && y <= 33){
    anggotaSuhu[2] = ((y - 32) / (33 - 32));
  }
  else if(y >= 33 && y <= 35){
    anggotaSuhu[2] = 1;
  }
  
  //Normal/Sedang
  if(y <= 35 || y >= 38){
    anggotaSuhu[3] = 0;
  }
  else if(y >= 37 && y <= 38){
    anggotaSuhu[3] = ((38 - y) / (38 - 37));
  }
  else if(y >= 35 && y <= 36){
    anggotaSuhu[3] = ((y - 35) / (36 - 35));
  }
  else if(y >= 36 && y <= 37){
    anggotaSuhu[3] = 1;
  }
}

void inferensi(){
  //Rule 1 = Suhu Sangat Dingin, detak Lambat, maka level berat
  alpha[0] = min(anggotaSuhu[0], anggotaJantung[0]);
  if(alpha[0] == 0){
    z[0] = 0.25;
  }
  else if(alpha[0] == 1){
    z[0] = 0;
  }
  else z[0] = 0.25 - (alpha[0] * (0.25 - 0));

  //Rule 2 = Suhu dingin, detak jantung lambat, maka level sedang
  alpha[1] = min(anggotaSuhu[1], anggotaJantung[0]);
  if(alpha[1] == 0){
    z[1] = 0.50;
  }
  else if(alpha[1] == 1){
    z[1] = 0.25;
  }
  else z[1] = 0.50 - (alpha[1] * (0.50 - 0.25));
  
  //Rule 3 = suhu agak dingin, detak jantung lambat, maka level sedang
  alpha[2] = min(anggotaSuhu[2], anggotaJantung[0]);
  if(alpha[2] == 0){
    z[2] = 0.50;
  }
  else if(alpha[2] == 1){
    z[2] = 0.25;
  }
  else z[2] = 0.50 - (alpha[2] * (0.50 - 0.25));
  
  //Rule 4 = Suhu agak dingin, detak jantung sedang, maka level ringan
  alpha[3] = min(anggotaSuhu[2], anggotaJantung[1]);
  if(alpha[3] == 0){
    z[3] = 0.75;
  }
  else if(alpha[3] == 1){
    z[3] = 0.50;
  }
  else z[3] = 0.75 - (alpha[3] * (0.75 - 0.5));
  
  //Rule 5 = Suhu agak dingin, detak jantung cepat, maka level ringan
  alpha[4] = min(anggotaSuhu[2], anggotaJantung[2]);
  if(alpha[4] == 0){
    z[4] = 0.75;
  }
  else if(alpha[4] == 1){
    z[4] = 0.50;
  }
  else z[4] = 0.75 - (alpha[4] * (0.75 - 0.5));
  
  //Rule 6 = Suhu sedang, detak jantung lambat, maka level normal
  alpha[5] = min(anggotaSuhu[3], anggotaJantung[0]);
  if(alpha[5] == 0){
    z[5] = 1;
  }
  else if(alpha[5] == 1){
    z[5] = 0.75;
  }
  else z[5] = 1 - (alpha[5] * (1 - 0.75));


  //Rule 7 = Suhu sedang, detak jantung sedang, maka level normal
  alpha[6] = min(anggotaSuhu[3], anggotaJantung[1]);
  if(alpha[6] == 0){
    z[6] = 1;
  }
  else if(alpha[6] == 1){
    z[6] = 0.75;
  }
  else z[6] = 1 - (alpha[6] * (1 - 0.75));


  //Rule 8 = Suhu sedang, detak jantung cepat, maka level normal
    alpha[7] = min(anggotaSuhu[3], anggotaJantung[2]);
  if(alpha[7] == 0){
    z[7] = 1;
  }
  else if(alpha[7] == 1){
    z[7] = 0.75;
  }
  else z[7] = 1 - (alpha[7] * (1 - 0.75));
}

float deffuzifikasi(){
  for(int i=0; i<8; i++){
    Eaizi += (alpha[i]*z[i]);
    Eai += alpha[i];
  }
  return Eaizi/Eai;
}

String convert (float z){
  if (z >= 0 && z < 0.25){
    return "HB";
  }
  else if (z >= 0.25 && z < 0.50){
    return "HS";
  }
  else if (z >= 0.50 && z < 0.75){
    return "HR";
  }
  else if (z >= 0.75 && z <= 1){
    return " N";
  }
  else return " N";
}

void bunyiRingan(){
  digitalWrite(buzzerPin, LOW);
  delay(500);
  digitalWrite(buzzerPin, HIGH);
  delay(500);
  digitalWrite(buzzerPin, LOW);
  delay(500);
  digitalWrite(buzzerPin, HIGH);
  delay(1000);
}
void bunyiSedang(){
  digitalWrite(buzzerPin, LOW);
  delay(100);
  digitalWrite(buzzerPin, HIGH);
  delay(100);
  digitalWrite(buzzerPin, LOW);
  delay(100);
  digitalWrite(buzzerPin, HIGH);
  delay(1000);
}
void bunyiBerat(){
  digitalWrite(buzzerPin, LOW);
}
void setup() {
  //Serial.begin(115200);
  pinMode(wakeUpPin, INPUT_PULLUP); 
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH);
  cli(); // disable interrupts during setup
  sensor.begin();
  // Initialize sensor
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    for (;;);
  }
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    while (1);
  }

  particleSensor.setup(); //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED

  TCCR1A = 0;
  TCCR1B = 1 << CS12 | 0 << CS11 | 1 << CS10; //setting 1024 prescaler
  TCNT1H = 0x0B;    // setting nilai awal register counter
  TCNT1L = 0xDC;
  TIFR1 = 1 << OCF1A; // Enable timer 1 flag interrupt
  TIMSK1 = 1 << OCIE1A; // Enable Timer 1 interrupt

  sei(); // re-enable interrupts
}

void wakeUp(){
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Wake up from sleep");
}

void loop() {
  if(sleep){
    sleep = false;
    beatAvg = 0;
    display.clearDisplay();
    display.setCursor(0,4);
    display.setTextSize(1);
    display.print("BPM : ");
    display.setTextSize(2);
    display.setCursor(36,0);
    display.print(isiBPM);
    display.setTextSize(1);
    display.setCursor(0,20);
    display.print("Suhu: ");
    display.setTextSize(2);
    display.setCursor(36,16);
    display.print(suhufix, 1);
    display.setTextSize(3);
    display.setCursor(90,5);
    display.print(simpan);
    display.display();
    delay(3000);
  }
  unsigned long sekarang = millis();
  unsigned long sekarang2 = millis();
  
  display.clearDisplay();

  long irValue = particleSensor.getIR();

  if (irValue < 50000) {
    if(!hitung){
      mulai = sekarang;
    }
    if(sekarang-mulai >= 2000){
      digitalWrite(buzzerPin, HIGH);
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.setCursor(15, 0);
      display.print("Please Put Your");
      display.setCursor(45, 16);
      display.print("Finger");
      sudahklasifikasi = false;
      dapatBPM = false;
      dapatSuhu = false;
      hitung2 = false;
      beatAvg = 0;
    }
    hitung = true;
  }
  else {
    //Serial.println(irValue);
     if (checkForBeat(irValue) == true) {
      //We sensed a beat!
      long delta = millis() - lastBeat;
      lastBeat = millis();
  
      beatsPerMinute = 60 / (delta / 1000.0);
  
      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
        rateSpot %= RATE_SIZE; //Wrap variable
  
        //Take average of readings
        beatAvg = 0;
        for (byte x = 0 ; x < RATE_SIZE ; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
     }
    hitung = false;
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    // Display static text
    display.print("BPM : ");
    if(dapatBPM != true){
      display.println(beatAvg);
    }
    else display.println(isiBPM);
    display.print("Suhu: ");
    display.println(isiSuhu, 1);
    //Serial.println(isiSuhu);
    if (hitungSamaSuhu == 5){
      suhufix = isiSuhu;
      dapatSuhu = true;
    }
    
    //display.println(hitung2);
    if(hitung2){
      berhenti2 = sekarang2 - mulai2;
      if(berhenti2 >= 60000 && dapatBPM != true){
        //lempar ke klasifikasi beat nya
        isiBPM = beatAvg;
        dapatBPM = true;
      }
     }
  
    if(dapatBPM != false && dapatSuhu != false && sudahklasifikasi != true){
      keanggotaanJantung(isiBPM);
      keanggotaanSuhu(suhufix);
      inferensi();
      defu = deffuzifikasi();
      sudahklasifikasi = true;
    }
    
    if(sudahklasifikasi){
      display.clearDisplay();
      display.setCursor(0,4);
      display.setTextSize(1);
      display.print("BPM : ");
      display.setTextSize(2);
      display.setCursor(36,0);
      display.print(isiBPM);
      display.setTextSize(1);
      display.setCursor(0,20);
      display.print("Suhu: ");
      display.setTextSize(2);
      display.setCursor(36,16);
      display.print(suhufix, 1);
      display.setTextSize(3);
      display.setCursor(90,5);
      simpan = convert(defu);
      display.print(simpan);
      display.display();
      if (simpan == "HB"){
        bunyiBerat();
      }
      else if (simpan == "HS"){
        bunyiSedang();
      }
      else if (simpan == "HR"){
        bunyiRingan();
      }
    }
    if(!hitung2){
      mulai2 = sekarang;
    }
    
    hitung2 = true;

  }
  
  if(hitung){
    berhenti = sekarang-mulai;
    if(berhenti >= 30000){
      display.clearDisplay();
      display.display();
      particleSensor.setup(0, 4, 3, 50, 110, 16384);
      sleep = true;
      hitung = false;
      attachInterrupt(1, wakeUp, FALLING);
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); 
      detachInterrupt(1); 
      particleSensor.setup();
    }
  }
  display.display();
}
