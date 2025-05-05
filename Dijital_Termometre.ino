#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_SHT31.h>

Adafruit_SHT31 sht31 = Adafruit_SHT31();
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


const uint8_t ileri_button = 14;
const uint8_t geri_button = 13;
bool ileri_onceki = LOW;
bool geri_onceki = LOW;
const int yesil_led = 16;
const int kirmizi_led = 17;

int counter = 0;
float sicaklik;
float nem;


#define GRAFIK_NOKTA 11
float sicaklik_gecmis[GRAFIK_NOKTA] = {0};
float nem_gecmis[GRAFIK_NOKTA] = {0};


void drawThermometer(float sicaklik) {
  const int x_center = 110, y_top = 10;
  const int tube_height = 35, tube_width = 10, bulb_radius = 8;

  u8g2.drawFrame(x_center - tube_width / 2, y_top, tube_width, tube_height);
  u8g2.drawCircle(x_center, y_top + tube_height + bulb_radius, bulb_radius, U8G2_DRAW_ALL);

  int fill_height = map((int)sicaklik, 0, 50, 0, tube_height);
  u8g2.drawBox(x_center - tube_width / 2 + 1, y_top + tube_height - fill_height, tube_width - 2, fill_height);
  u8g2.drawDisc(x_center, y_top + tube_height + bulb_radius, bulb_radius - 2, U8G2_DRAW_ALL);
}


void drawDrop(float nem) {
  const int x_center = 80, y_top = 10;
  const int drop_width = 14, drop_height = 40;

  u8g2.drawTriangle(
    x_center, y_top,
    x_center - drop_width / 2, y_top + drop_height / 2,
    x_center + drop_width / 2, y_top + drop_height / 2
  );
  u8g2.drawCircle(x_center, y_top + drop_height - 8, drop_width / 2, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);

  int dolu_h = map((int)nem, 0, 100, 0, drop_height - 6);
  u8g2.drawBox(x_center - drop_width / 2 + 2, y_top + drop_height - dolu_h - 6, drop_width - 4, dolu_h);
}


void drawSicaklikGrafik() {
  const int gx = 20, gy = 5, gw = 100, gh = 40;
  u8g2.drawFrame(gx, gy, gw, gh);

  u8g2.setCursor(0, gy + 8);  u8g2.print("27");
  u8g2.setCursor(0, gy + 18); u8g2.print("20");
  u8g2.setCursor(0, gy + 28); u8g2.print("14");
  u8g2.setCursor(0, gy + 38); u8g2.print("7");

  for (int i = 0; i < GRAFIK_NOKTA; i++) {
    u8g2.setCursor(gx + i * 9, gy + gh + 10);
    u8g2.print(i);
  }

  for (int i = 0; i < GRAFIK_NOKTA - 1; i++) {
    int y1 = map((int)sicaklik_gecmis[i], 7, 27, gy + gh, gy);
    int y2 = map((int)sicaklik_gecmis[i + 1], 7, 27, gy + gh, gy);
    u8g2.drawLine(gx + i * 9, y1, gx + (i + 1) * 9, y2);
  }
}


void drawNemGrafik() {
  const int gx = 20, gy = 5, gw = 100, gh = 40;
  u8g2.drawFrame(gx, gy, gw, gh);

  u8g2.setCursor(0, gy + 8);  u8g2.print("100");
  u8g2.setCursor(0, gy + 18); u8g2.print("75");
  u8g2.setCursor(0, gy + 28); u8g2.print("50");
  u8g2.setCursor(0, gy + 38); u8g2.print("25");

  for (int i = 0; i < GRAFIK_NOKTA; i++) {
    u8g2.setCursor(gx + i * 9, gy + gh + 10);
    u8g2.print(i);
  }

  for (int i = 0; i < GRAFIK_NOKTA - 1; i++) {
    int y1 = map((int)nem_gecmis[i], 0, 100, gy + gh, gy);
    int y2 = map((int)nem_gecmis[i + 1], 0, 100, gy + gh, gy);
    u8g2.drawLine(gx + i * 9, y1, gx + (i + 1) * 9, y2);
  }
}


void setup() {
  Wire.begin(21, 22);
  Serial.begin(9600);
  pinMode(ileri_button, INPUT);
  pinMode(geri_button, INPUT);
  pinMode(kirmizi_led,OUTPUT);
  pinMode(yesil_led,OUTPUT);
  if (!sht31.begin(0x44)) {
    Serial.println("SHT31 bulunamadi!");
    while (1);
  }

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tr);
}

void loop() {
 
  bool ileri_state = digitalRead(ileri_button);
  bool geri_state = digitalRead(geri_button);

  if (ileri_onceki == HIGH && ileri_state == LOW) {
    counter = (counter + 1) % 4;
  }
  if (geri_onceki == HIGH && geri_state == LOW) {
    counter = (counter - 1 + 3) % 3;
  }
  ileri_onceki = ileri_state;
  geri_onceki = geri_state;


  sicaklik = sht31.readTemperature();
  nem = sht31.readHumidity();
  if(sicaklik >= 25 || nem >= 75){
    digitalWrite(kirmizi_led ,HIGH);
    digitalWrite(yesil_led ,LOW);
  }
  else{
    digitalWrite(kirmizi_led,LOW);
    digitalWrite(yesil_led,HIGH);
  }

  for (int i = 0; i < GRAFIK_NOKTA - 1; i++) {
    sicaklik_gecmis[i] = sicaklik_gecmis[i + 1];
    nem_gecmis[i] = nem_gecmis[i + 1];
  }
  sicaklik_gecmis[GRAFIK_NOKTA - 1] = sicaklik;
  nem_gecmis[GRAFIK_NOKTA - 1] = nem;


  u8g2.clearBuffer();

  if (counter == 1) {
    u8g2.setCursor(0, 62);
    u8g2.print("Sicaklik Grafigi");
    drawSicaklikGrafik();
  }
  else if (counter == 2) {
    u8g2.setCursor(0, 62);
    u8g2.print("Nem Grafigi");
    drawNemGrafik();
  }
  else if (counter ==3){
    u8g2.setCursor(0,10);
    u8g2.print("Enes Emek");
  }
  else {
    u8g2.setCursor(0, 10);
    u8g2.print("Sicaklik:");
    u8g2.setCursor(0, 20);
    u8g2.print(sicaklik, 1);
    u8g2.print(" C");
    u8g2.setCursor(0, 30);
    u8g2.print("Nem: ");
    u8g2.setCursor(0, 40);
    u8g2.print(nem, 1);
    u8g2.print(" %");

    drawThermometer(sicaklik);
    drawDrop(nem);
  }

  u8g2.sendBuffer();
  delay(1000);
}
