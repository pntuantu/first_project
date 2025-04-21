#include <SPI.h>              // Thư viện giao tiếp SPI
#include <nRF24L01.h>         // Thư viện hỗ trợ NRF24L01
#include <RF24.h>             // Thư viện chính cho NRF24L01
#include <WiFi.h>             // Thư viện WiFi cho ESP32
#include <WiFiClient.h>       // Thư viện hỗ trợ giao tiếp WiFi
#include <FirebaseESP32.h>    // Thư viện Firebase cho ESP32
#include <time.h>             // Thư viện thời gian cho ESP32

// Định nghĩa chân SPI cho NRF24L01
#define SCK 18
#define MISO 19
#define MOSI 23
#define CSN 5
#define CE 2

// Thông tin WiFi
#define WIFI_SSID "IoT Lab"
#define WIFI_PASSWORD "IoT@123456"

// Thông tin Firebase
#define FIREBASE_HOST "https://doan1-smart-farm-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "mLnggNqjBY2D7TTtRgNagmNKhoBz8vpDjncaZgt0"

// NTP Server để đồng bộ thời gian
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 7 * 3600  // GMT+7 cho Việt Nam
#define DAYLIGHT_OFFSET_SEC 0

// Khởi tạo đối tượng
RF24 radio(CE, CSN);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const byte address[6] = "00001"; // Địa chỉ nhận dữ liệu
float temp, humi, tvoc, co2;

void setup() {
  Serial.begin(9600);
  delay(1000);

  // Kết nối WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Đang kết nối WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Đã kết nối WiFi!");

  // Đồng bộ thời gian với NTP
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.print("Đang đồng bộ thời gian...");
  while (time(nullptr) < 100000) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Đã đồng bộ thời gian!");

  // Cấu hình Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Khởi động SPI và NRF24L01
  SPI.begin(SCK, MISO, MOSI);
  if (!radio.begin()) {
    Serial.println("Module NRF24L01 khởi động thất bại!");
    while (1);
  }
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_MIN);
  radio.startListening();
  Serial.println("Module NRF24L01 sẵn sàng đọc dữ liệu!");
}

String getTimestamp() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(buffer);
}

void saveToHistory(String path, float value) {
  String timestamp = getTimestamp();
  
  // Tạo đối tượng FirebaseJson
  FirebaseJson json;
  json.set("timestamp", timestamp);
  json.set("value", value);

  // Đẩy dữ liệu lên Firebase
  if (Firebase.pushJSON(fbdo, path, json)) {
    Serial.println("Saved to history at " + path);
  } else {
    Serial.println("Failed to save to history: " + fbdo.errorReason());
  }
}

void loop() {
  static unsigned long lastSave = 0;
  const unsigned long saveInterval = 300000; // 5 phút (300,000 ms)

  if (radio.available()) {
    char receivedData[32] = "";
    radio.read(&receivedData, sizeof(receivedData));
    Serial.print("Dữ liệu nhận: ");
    Serial.println(receivedData);

    // Tách dữ liệu
    char *token = strtok(receivedData, ",");

    // Xử lý Temp
    if (token != NULL) {
      temp = atof(token);
      Serial.print("Temp: ");
      Serial.println(temp);

      // Xử lý Humi
      token = strtok(NULL, ",");
      if (token != NULL) {
        humi = atof(token);
        Serial.print("Humi: ");
        Serial.println(humi);

        // Xử lý TVOC
        token = strtok(NULL, ",");
        if (token != NULL) {
          tvoc = atof(token);
          Serial.print("TVOC: ");
          Serial.println(tvoc);

          // Xử lý CO2
          token = strtok(NULL, ",");
          if (token != NULL) {
            co2 = atof(token);
            Serial.print("CO2: ");
            Serial.println(co2);

            // Gửi dữ liệu hiện tại lên Firebase
            if (Firebase.setFloat(fbdo, "dht22/temp", temp)) {
              Serial.println("Gửi nhiệt độ thành công");
            } else {
              Serial.println("Lỗi gửi nhiệt độ: " + fbdo.errorReason());
            }

            if (Firebase.setFloat(fbdo, "dht22/humi", humi)) {
              Serial.println("Gửi độ ẩm thành công");
            } else {
              Serial.println("Lỗi gửi độ ẩm: " + fbdo.errorReason());
            }

            if (Firebase.setFloat(fbdo, "ens160/tvoc", tvoc)) {
              Serial.println("Gửi TVOC thành công");
            } else {
              Serial.println("Lỗi gửi TVOC: " + fbdo.errorReason());
            }

            if (Firebase.setFloat(fbdo, "ens160/co2", co2)) {
              Serial.println("Gửi CO2 thành công");
            } else {
              Serial.println("Lỗi gửi CO2: " + fbdo.errorReason());
            }

            // Lưu dữ liệu lịch sử mỗi 5 phút
            if (millis() - lastSave >= saveInterval) {
              saveToHistory("/dht22/temp_history", temp);
              saveToHistory("/dht22/humi_history", humi);
              saveToHistory("/ens160/tvoc_history", tvoc);
              saveToHistory("/ens160/co2_history", co2);
              lastSave = millis();
            }
          } else {
            Serial.println("Lỗi: Không nhận được CO2");
          }
        } else {
          Serial.println("Lỗi: Không nhận được TVOC");
        }
      } else {
        Serial.println("Lỗi: Không nhận được Humi");
      }
    } else {
      Serial.println("Lỗi: Không nhận được Temp");
    }
  }

  delay(1000); // Đợi 1 giây trước khi kiểm tra dữ liệu mới
}