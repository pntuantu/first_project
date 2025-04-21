#include <SPI.h>          // Giao tiếp SPI cho NRF24L01
#include <RF24.h>         // Điều khiển NRF24L01
#include <nRF24L01.h>     // Module NRF24L01
#include <DHT.h>          // Thư viện DHT22
#include <DFRobot_ENS160.h> // Thư viện ENS160
#include <Wire.h>         // Giao tiếp I2C
#include <LiquidCrystal_I2C.h> // Thư viện LCD I2C

// Định nghĩa chân SPI cho NRF24L01
#define SCK 18
#define MISO 19
#define MOSI 23
#define CSN 5
#define CE 2

// Định nghĩa chân và loại cảm biến DHT
#define DHTPIN 4
#define DHTTYPE DHT22

// Định nghĩa chân I2C
#define SDA_PIN 21
#define SCL_PIN 22

// Khởi tạo đối tượng
RF24 radio(CE, CSN);
DHT dht(DHTPIN, DHTTYPE);
DFRobot_ENS160_I2C ENS160(&Wire, /*I2CAddr*/ 0x53); // ENS160 địa chỉ 0x53
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD địa chỉ 0x27, 16x2

const byte address[6] = "00001"; // Địa chỉ truyền NRF24L01
float nhietDo, doAm;
uint16_t TVOC, eCO2;

void setup() {
  Serial.begin(9600);
  delay(1000);

  // Khởi động DHT
  dht.begin();

  // Khởi động LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Khoi dong...");
  
  // Khởi động SPI cho NRF24L01
  SPI.begin(SCK, MISO, MOSI);
  if (!radio.begin()) {
    Serial.println("Module NRF24L01 khoi dong that bai");
    lcd.clear();
    lcd.print("NRF24L01 loi!");
    while (1);
  }
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();
  Serial.println("NRF24L01 san sang gui du lieu");

  // Khởi động I2C cho ENS160 và LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(50000); // Tốc độ I2C 50kHz
  while (NO_ERR != ENS160.begin()) {
    Serial.println("Loi giao tiep ENS160, kiem tra ket noi");
    Wire.beginTransmission(0x53);
    int error = Wire.endTransmission();
    Serial.print("Ma loi I2C: ");
    Serial.println(error); // 0: OK, 2: NACK, 4: Other
    lcd.clear();
    lcd.print("ENS160 loi!");
    delay(3000);
  }
  Serial.println("ENS160 khoi tao thanh cong!");
  
  // Đặt chế độ ENS160
  ENS160.setPWRMode(ENS160_STANDARD_MODE);

  // Chờ Warm-Up (30s)
  Serial.println("Dang cho ENS160 Warm-Up (3 phut)...");
  lcd.clear();
  lcd.print("ENS160 Warm-Up...");
  for (int i = 0; i < 180; i++) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nENS160 san sang!");
  lcd.clear();
}


void loop() {
  // Đọc dữ liệu từ DHT22
  nhietDo = dht.readTemperature();
  doAm = dht.readHumidity();
  
  // Kiểm tra dữ liệu DHT22
  if (isnan(nhietDo) || isnan(doAm)) {
    Serial.println("Loi doc DHT22!");
    nhietDo = 25.0; // Giá trị mặc định
    doAm = 50.0;
  }

  // Bù nhiệt độ và độ ẩm cho ENS160
  ENS160.setTempAndHum(nhietDo, doAm);

  // Đọc trạng thái ENS160
  uint8_t Status = ENS160.getENS160Status();
  Serial.print("Trang thai ENS160: ");
  Serial.println(Status); // 0: Normal, 1: Warm-Up, 2: Initial Start-Up

  // Đọc dữ liệu từ ENS160
  TVOC = ENS160.getTVOC();
  eCO2 = ENS160.getECO2();

  // In dữ liệu ra Serial Monitor
  Serial.print("Nhiet do: ");
  Serial.print(nhietDo);
  Serial.println(" C");
  Serial.print("Do am: ");
  Serial.print(doAm);
  Serial.println(" %");
  Serial.print("TVOC: ");
  Serial.print(TVOC);
  Serial.println(" ppb");
  Serial.print("eCO2: ");
  Serial.print(eCO2);
  Serial.println(" ppm");

  // Hiển thị trên LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(nhietDo, 1);
  lcd.print("C H:");
  lcd.print(doAm, 1);
  lcd.print("%");
  lcd.setCursor(0, 1);
  lcd.print("VOC:");
  lcd.print(TVOC);
  lcd.print(" CO2:");
  lcd.print(eCO2);

  // Định dạng và gửi dữ liệu qua NRF24L01
  char data[32];
  snprintf(data, sizeof(data), "%.2f,%.2f,%u,%u", nhietDo, doAm, TVOC, eCO2);
  Serial.print("Du lieu gui: ");
  Serial.println(data);
  radio.write(&data, sizeof(data));

  Serial.println();
  delay(5000); // Cập nhật mỗi 5 giây
}