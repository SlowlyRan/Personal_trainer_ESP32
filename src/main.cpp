#include "SparkFunLSM6DSO.h"
#include "Wire.h"
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include <Arduino.h>         // Provides Arduino framework functions and macros for the ESP32.
#include <HttpClient.h>       // HTTP client library for making HTTP requests.
#include <WiFi.h>         

// 加速度计参数
LSM6DSO myIMU;
int liftCount = 0;
float threshold_up = 1.1; // 举起阈值
float threshold_down = 0.8; // 放下阈值
bool isLifted = false;

// 定时器
const long interval = 1000;

// 引脚定义
const int buttonPin = 32; // 按钮引脚
const int ledPin = 13;    // LED 引脚
const int buzzerPin = 15; // 蜂鸣器引脚
int current_exercise = 0; // 当前运动
int total_exercise = 2; // 总运动数
bool counting = false;    // 是否在计数状态
bool countdownStarted = false; // 是否倒计时开始
int countdown = 3;        // 倒计时初始值
int currentSet = 1;       // 当前组数
int totalSets[] = {3,3};  // 总组数
bool restPeriod = false;  // 是否在休息间隔状态
int restCountdown = 0;   // 组间休息倒计时
bool restFinished = true; // 休息结束标志
unsigned long detectTime = 0;
unsigned long ledTime = 0;
bool detectState = false;
bool ledState = false;
const unsigned long detectDuration = 300; 
const unsigned long ledDuration = 100;    // LED blink duration in milliseconds
unsigned long lastUpdate = 0;

// Workout variables
const char *exercises[] = {"Push-ups", "Sit-ups"};
int weight[] = {0, 0};
int rep_number[] = {10, 15};
int resttime[] = {10,10};        // 组间休息时间


// State management
enum State { IDLE, COUNTDOWN, COUNTING, RESTING };
State currentState = IDLE;


constexpr uint32_t TFT_GREY = 0x5AEB; // Grey color
TFT_eSPI tft = TFT_eSPI();
char buffer[50]; // Buffer to hold the formatted string

// Network settings
const int kNetworkTimeout = 30 * 1000; // 30 seconds timeout
const int kNetworkDelay = 1000; // Delay before retrying the connection


const char kHostname[] = "35.232.27.80"; 
const int kPort = 5000;                  
const char kPath[] = "/spt/upload";  
const char contentType[] = "text/plain";
int device_id = 3;


String sensorData = "";  // 用来存储每个set的数据
unsigned long startTime = 0; // 记录运动开始的时间
int datapointsNumbers = 0; // 记录每个set的数据点数



void displayText(const char* text) {
  tft.fillScreen(TFT_GREY); // Clear the screen before displaying new data
  tft.setCursor(50, 10, 2);
  tft.setTextColor(TFT_WHITE, TFT_GREY); // White text with grey background
  tft.setTextSize(2);
  tft.print(text);
}


void connectWiFi() {
    Serial.println("Connecting to Wi-Fi...");
    char ssid[] = "iPhone3";      // Set Wi-Fi SSID
    char pass[] = "ranran11";    // Set Wi-Fi password
    WiFi.begin(ssid, pass);

    unsigned long startMillis = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (millis() - startMillis > kNetworkTimeout) {
            Serial.println("Failed to connect to Wi-Fi");
            return;
        }
    }
    Serial.println("\nWiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("MAC address: ");
    Serial.println(WiFi.macAddress());
}

void uploadData() {
    WiFiClient wifiClient;
    HttpClient http(wifiClient);
    
    char postData[20]; 
    snprintf(postData, sizeof(postData), "%d,%d\n",device_id, datapointsNumbers); // Format the string

    http.beginRequest();


    int err = http.post(kHostname, kPort, kPath);
    if (err == 0) {
        Serial.println("Started HTTP request");

        http.sendHeader("Content-Type", contentType); // 发送 Content-Type 头部
        http.sendHeader("Content-Length", sensorData.length() + strlen(postData)); // 发送 Content-Length 头部
        http.print(postData); // 发送数据
        Serial.println(postData);
        http.print(sensorData); // 发送数据
        http.endRequest(); // 结束请求
        Serial.print(datapointsNumbers);

        Serial.println("Request sent successfully.");

        // Get response status code
        err = http.responseStatusCode();
        if (err >= 0) {
            Serial.print("Response code: ");
            Serial.println(err);

            // Skip response headers
            err = http.skipResponseHeaders();
            if (err >= 0) {
                int bodyLen = http.contentLength();
                Serial.print("Content length: ");
                Serial.println(bodyLen);
                Serial.println("Body of the response follows:");

                unsigned long timeoutStart = millis();
                char c;
                // Read and print the response body
                while ((http.connected() || http.available()) && ((millis() - timeoutStart) < kNetworkTimeout)) {
                    if (http.available()) {
                        c = http.read();
                        Serial.print(c);
                        bodyLen--; // Decrease remaining content length
                        timeoutStart = millis(); // Reset timeout counter
                    } else {
                        delay(kNetworkDelay); // Wait for more data
                    }
                }
            } else {
                Serial.print("Failed to skip headers: ");
                Serial.println(err);
            }
        } else {
            Serial.print("Failed to get response status: ");
            Serial.println(err);
        }
    } else {
        Serial.print("Failed to connect: ");
        Serial.println(err);
    }

    sensorData = ""; 
    datapointsNumbers = 0; 
    startTime = 0; // 重置开始时间
    http.stop();
}



void setup() {
  Serial.begin(9600);
  delay(500);

  Wire.begin();
  delay(10);
  if (myIMU.begin())
    Serial.println("IMU Ready.");
  else {
    Serial.println("Could not connect to IMU.");
    Serial.println("Freezing");

   
  }


  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_GREY);

  if (myIMU.initialize(BASIC_SETTINGS))
    Serial.println("Loaded Settings.");

    connectWiFi();
        snprintf(buffer, sizeof(buffer), "Next: %s weight %d", exercises[current_exercise],weight[current_exercise]); // Format the string
    displayText(buffer);
    Serial.println(buffer);

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
}

void handleIdle() {   
    if (digitalRead(buttonPin) == LOW) {
        countdown = 3;
        currentState = COUNTDOWN;
        Serial.println("Countdown started...");
        displayText("Countdown started...");
        lastUpdate = millis();
    }
}

void handleCountdown() {
   unsigned long currentMillis = millis();
    if (currentMillis - lastUpdate >= interval) {
      lastUpdate = currentMillis;

      if (countdown > 0) {
        // 倒计时蜂鸣器提示
        Serial.print("Countdown: ");
        Serial.println(countdown);
        snprintf(buffer, sizeof(buffer), "Countdown:  %d", countdown); // Format the string
        displayText(buffer); // Pass the formatted string to the display function
        if (countdown == 1){
        tone(buzzerPin, 2500, 750); 
      }
      else{
        tone(buzzerPin, 1000, 500); 
      }
        countdown--;
      } else {
        // 倒计时结束，进入计数状态
        countdownStarted = false;
        counting = true;
        Serial.println("Start counting!");
        displayText("Start counting!");
        currentState = COUNTING;
      }
    }
    return; // 倒计时进行时，跳过其他逻辑
  }

void handleCounting()
 {

    // 获取加速度数据
    float accelX = myIMU.readFloatAccelX();
    float accelY = myIMU.readFloatAccelY();
    float accelZ = myIMU.readFloatAccelZ();
    float gyroX = myIMU.readFloatGyroX();
    float gyroY = myIMU.readFloatGyroY();
    float gyroZ = myIMU.readFloatGyroZ();
    unsigned long currentMillis = millis();
    if (startTime == 0) {
      startTime = currentMillis; // 记录运动开始的时间
    }
    unsigned long elapsedTime = currentMillis - startTime;
    char dataStr[80];  // 用于存储格式化后的加速度数据
    Serial.print("Elapsed Time: ");
    Serial.println(elapsedTime);

    snprintf(dataStr, sizeof(dataStr), "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n",accelX, accelY, accelZ,gyroX, gyroY, gyroZ,elapsedTime); // Format the string
    sensorData += dataStr; // 将数据添加到sensorData中  
    datapointsNumbers++; // 记录数据点数
    Serial.print("Data: ");
    Serial.println(dataStr);
    
    float accel = max(max(abs(accelX), abs(accelY)), abs(accelZ));


    // 判断是否大于阈值，表示举起
    if (accel > threshold_up && !isLifted) {
    //if (accel < threshold_down && isLifted) {
      isLifted = true; // 标记为举起状态
      Serial.println("Lifted");
    }

    // 判断是否恢复到接近重力加速度，表示放下
    if (accel < threshold_down && isLifted) {
    //if (accel > threshold_up && !isLifted) {
      isLifted = false; // 标记为放下状态
      liftCount++;      // 增加动作计数
      Serial.println("Put Down");
      Serial.print("Lift Count: ");
      Serial.println(liftCount);

      snprintf(buffer, sizeof(buffer), "Finised:  %d reps", liftCount); // Format the string
      displayText(buffer); // Pass the formatted string to the display function
      detectState = true;
      detectTime = currentMillis;

      tone(buzzerPin, 1000, 100); // 蜂鸣器短响 200ms

      ledState = true;
      ledTime = currentMillis;
    }

  if (detectState && currentMillis - detectTime > detectDuration) {
    detectState = false;}

   if (ledState && currentMillis - ledTime < ledDuration) {
    digitalWrite(ledPin, HIGH); // 点亮LED
  } else if (ledState && currentMillis - ledTime >= ledDuration) {
    digitalWrite(ledPin, LOW); // 熄灭LED
    ledState = false;      // 停止LED闪烁
  }


    // 检查是否达到组内重复次数
    if (liftCount >= rep_number[current_exercise]) {
      delay(100); // 防抖
      Serial.print("Completed ");
      Serial.print(currentSet);
      Serial.println(" set(s)!");
      snprintf(buffer, sizeof(buffer), "Completed  %d sets", currentSet); // Format the string
      displayText(buffer); // Pass the formatted string to the display function
      
      tone(buzzerPin, 3000, 2000); // 长响 2 秒
      noTone(buzzerPin); // 停止蜂鸣器
      uploadData(); // 上传数据

      liftCount = 0;    // 重置计数器

      // 判断是否完成所有组
      if (currentSet >= totalSets[current_exercise]) {
        uploadData(); // 上传数据

        if(current_exercise>=total_exercise){
        Serial.println("All sets completed! Well done!");
        displayText("All sets completed! Well done!");
        while (true); // 训练结束，程序停止
        }
        else{
        current_exercise++; // 进入下一个运动 
        currentSet =0;
        currentState = IDLE; // 回到空闲状态
        snprintf(buffer, sizeof(buffer), "Next: %s weight %d", exercises[current_exercise],weight[current_exercise]); // Format the string
        displayText(buffer);
        }
      } else {
        currentState = RESTING; 
        restFinished = false;   
        restCountdown = resttime[current_exercise];     // 重置休息倒计时
        currentSet++;           // 增加组数
        lastUpdate = millis();  // 重置时间
      }
    }

    delay(20); // 采样间隔 20ms
  }

void handleResting()
{
   unsigned long currentMillis = millis();
    if (currentMillis - lastUpdate >= interval) {
      lastUpdate = currentMillis;

      if (restCountdown > 0) {
        // 组间休息提示
        Serial.print("Rest time: ");
        Serial.println(restCountdown);
        snprintf(buffer, sizeof(buffer), "Rest time:  %d", restCountdown); // Format the string
        displayText(buffer); 
        restCountdown--;
      } else {
        tone(buzzerPin, 1750,100 );
        Serial.println("Rest finished. Press button to start next set.");
        displayText("Rest finished. Press button to start next set.");
        currentState = IDLE;
      }
    }
  }



void loop() {
    unsigned long currentMillis = millis();

    switch (currentState) {
        case IDLE:
            handleIdle();
            break;
        case COUNTDOWN:
            handleCountdown();
            break;
        case COUNTING:
            handleCounting();
            break;
        case RESTING:
            handleResting();
            break;
    }
}



