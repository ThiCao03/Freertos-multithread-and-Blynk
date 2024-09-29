#define BLYNK_TEMPLATE_ID "TMPL6r6ct1x7v"
#define BLYNK_TEMPLATE_NAME "Caodinhthi"
#define BLYNK_AUTH_TOKEN "sdoehfHPOuscKOdKdMZFkxbxvdVvEQLo"

#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT_U.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT11
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Relay, LED và nút bấm
#define RELAY_PIN 4
#define LED_RED_PIN 14
#define LED_GREEN_PIN 12  // LED xanh
#define BUTTON_PIN 13
WidgetLED LED_ON_APP(V2);

// WiFi
#define WIFI_SSID "Thi 83 HTQ"
#define WIFI_PASSWORD "Thi2k3er123456"
#define WIFI_CHANNEL 6

// Trạng thái hệ thống
enum SystemState { NORMAL, WATERING, MANUAL_WATERING };
SystemState systemState = NORMAL;

// Ngưỡng
volatile float temp_threshold_low = 25;
volatile float temp_threshold_high = 35;
volatile float humidity_threshold_low = 60;
volatile float humidity_threshold_high = 80;

// Biến toàn cục
volatile float humidity = 50;
volatile float temperature = 0;
bool buttonPressed = false;
volatile int blynkButtonState = 0;  // Biến lưu trạng thái của nút Blynk (pin V3)

// Biến kiểm tra và kết nối lại Blynk
unsigned long lastConnectionAttemptTime = 0;
unsigned long connectionInterval = 10000;  // Khoảng thời gian thử kết nối lại (10 giây)

// Hàm kiểm tra và kết nối lại Blynk
void reconnectBlynk() {
    if (!Blynk.connected() && (millis() - lastConnectionAttemptTime > connectionInterval)) {
        Serial.println("Attempting to reconnect to Blynk...");
        Blynk.connect();
        lastConnectionAttemptTime = millis();  // Cập nhật thời gian thử kết nối lại
    }
}

// Hàm cập nhật trạng thái OLED và LED
void updateOLEDandLED(const char* stateMessage, bool relayOn) {
    // Cập nhật LED
    digitalWrite(RELAY_PIN, relayOn ? HIGH : LOW);
    digitalWrite(LED_GREEN_PIN, relayOn ? HIGH : LOW);
    digitalWrite(LED_RED_PIN, relayOn ? LOW : HIGH);

    // Cập nhật OLED
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(stateMessage);
    display.display();
}

// Tác vụ đo DHT và hiển thị OLED
void task_readDHT22(void *pvParameters) {
    while (true) {
        humidity = dht.readHumidity();
        temperature = dht.readTemperature();

        if (!isnan(humidity) && !isnan(temperature)) {
            Blynk.virtualWrite(V0, temperature);  // Gửi nhiệt độ lên Blynk
            Blynk.virtualWrite(V1, humidity);     // Gửi độ ẩm lên Blynk
            if (systemState == NORMAL) {
                display.clearDisplay();
                display.setTextSize(1);
                display.setTextColor(SSD1306_WHITE);
                display.setCursor(0, 0);
                display.print("Temp: ");
                display.print(temperature);
                display.println(" C");
                display.print("Humidity: ");
                display.print(humidity);
                display.println(" %");
                if (temperature > temp_threshold_high || temperature < temp_threshold_low || humidity > humidity_threshold_high) {
                    display.println("DONT NEED WATERING");
                }
                display.display();
            }
        } else {
            Serial.println("Failed to read from DHT sensor!");
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// Tác vụ điều khiển tưới nước tự động
void task_controlWatering(void *pvParameters) {
    while (true) {
        if (!isnan(humidity) && !isnan(temperature) && humidity < humidity_threshold_low && temperature > temp_threshold_low && temperature < temp_threshold_high && systemState == NORMAL && !buttonPressed) {
            systemState = WATERING;
            updateOLEDandLED("Watering", true);  // Cập nhật OLED và LED

            vTaskDelay(5000 / portTICK_PERIOD_MS);

            systemState = NORMAL;
            updateOLEDandLED("Normal", false);  // Cập nhật OLED và LED
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// Tác vụ đọc nút bấm để điều khiển tưới nước thủ công
void task_readButton(void *pvParameters) {
    while (true) {
        if (digitalRead(BUTTON_PIN) == LOW) {
            if (!buttonPressed) {
                buttonPressed = true;

                if (systemState == NORMAL) {
                    systemState = MANUAL_WATERING;
                    updateOLEDandLED("Manual Watering", true);  // Cập nhật OLED và LED

                    vTaskDelay(5000 / portTICK_PERIOD_MS);

                    systemState = NORMAL;
                    updateOLEDandLED("Normal", false);  // Cập nhật OLED và LED
                }
            }
        } else {
            buttonPressed = false;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// Tác vụ xử lý tưới nước từ Blynk
void task_blynkWatering(void *pvParameters) {
    while (true) {
        if (blynkButtonState == 1 && systemState == NORMAL) {
            systemState = MANUAL_WATERING;
            updateOLEDandLED("Blynk Watering", true);  // Cập nhật OLED và LED

            vTaskDelay(5000 / portTICK_PERIOD_MS);

            systemState = NORMAL;
            updateOLEDandLED("Normal", false);  // Cập nhật OLED và LED

            // Reset lại trạng thái nút bấm sau khi tưới nước xong
            blynkButtonState = 0;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// Cập nhật trạng thái nút Blynk (pin V3)
BLYNK_WRITE(V3) {
    blynkButtonState = param.asInt();  // Cập nhật trạng thái nút từ Blynk
    if(blynkButtonState == 1){
      LED_ON_APP.on();
    } else {
      LED_ON_APP.off();
    }
}

void setup() {
    Serial.begin(115200);

    // Khởi động màn hình OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed");
        for (;;);
    }
    display.display();
    delay(1000);
    display.clearDisplay();

    // Khởi động DHT
    dht.begin();

    // Khởi tạo relay, LED và nút bấm
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Bật LED đỏ ban đầu
    digitalWrite(LED_RED_PIN, HIGH);
    digitalWrite(LED_GREEN_PIN, LOW);

    // Kết nối WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    // Kết nối Blynk
    Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);

    // Tạo các tác vụ FreeRTOS
    xTaskCreate(task_readDHT22, "DHT22 Task", 4096, NULL, 1, NULL);
    xTaskCreate(task_controlWatering, "Watering Task", 4096, NULL, 1, NULL);
    xTaskCreate(task_readButton, "Button Task", 2048, NULL, 1, NULL);
    xTaskCreate(task_blynkWatering, "Blynk Watering Task", 2048, NULL, 1, NULL);
}

void loop() {
    // Chạy các chức năng của Blynk
    if (Blynk.connected()) {
        Blynk.run();
    }
    
    // Kiểm tra và kết nối lại nếu mất kết nối
    reconnectBlynk();
}
