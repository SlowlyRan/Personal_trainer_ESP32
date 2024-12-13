#include "SparkFunLSM6DSO.h"
#include "Wire.h"
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <Arduino.h>        
#include <HttpClient.h>       
#include <WiFi.h>         
#include <String.h>


struct Exercise {
  String name;
  float weight;  
  int sets;      
  int reps_per_set; 
  int rest_time;  
};

LSM6DSO myIMU;
int liftCount = 0;
float threshold_up = 1.1; 
float threshold_down = 0.8; 
bool isLifted = false;
#define MAX_WORKOUTS 10
String plan_id;  
Exercise workouts[MAX_WORKOUTS];  
int workout_count = 0;            
const long interval = 1000;


const int buttonPin = 32; 
const int ledPin = 13;    // LED 引脚
const int buzzerPin = 15; // 蜂鸣器引脚
int current_exercise = 0; // 当前运动
int total_exercise = 2; // 总运动数
bool counting = false;    // 是否在计数状态
bool countdownStarted = false; // 是否倒计时开始
int countdown = 3;        // 倒计时初始值
int currentSet = 1;       // 当前组数
int restCountdown = 0;   
bool restFinished = true; // 休息结束标志
unsigned long detectTime = 0;
unsigned long ledTime = 0;
bool detectState = false;
bool ledState = false;
const unsigned long detectDuration = 300; 
const unsigned long ledDuration = 100;    
unsigned long lastUpdate = 0;




// State management
enum State { IDLE, COUNTDOWN, COUNTING, RESTING };
State currentState = IDLE;


constexpr uint32_t TFT_GREY = 0x5AEB; // Grey color
TFT_eSPI tft = TFT_eSPI();
char buffer[70]; // Buffer to hold the formatted string

// Network settings
const int kNetworkTimeout = 30 * 1000; // 30 seconds timeout
const int kNetworkDelay = 1000; // Delay before retrying the connection


const char kHostname[] = "35.232.27.80"; 
const int kPort = 5000;                  
const char kPath[] = "/spt/upload";  
const char pPath[] = "/spt/plan";  
const char contentType[] = "text/plain";
int device_id = 1;


String sensorData = ""; 
unsigned long startTime = 0; 
int datapointsNumbers = 0; 




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
    String plan_id2="A64gD";
    char postData[50]; 
    snprintf(postData, sizeof(postData), "%d,%d\n",
    device_id, datapointsNumbers);
    char postData2[200]; 
    snprintf(postData2, sizeof(postData), "%d,%d,%s,%d,%s,%d,%d,%.2f\n",
             device_id, datapointsNumbers, plan_id, current_exercise,
             workouts[current_exercise].name, currentSet,
             workouts[current_exercise].reps_per_set, workouts[current_exercise].weight);
    Serial.println(postData2);


    http.beginRequest();

    int err = http.post(kHostname, kPort, kPath);
    if (err == 0) {
        Serial.println("Started HTTP request");

        http.sendHeader("Content-Type", contentType); 
        http.sendHeader("Content-Length", sensorData.length() + strlen(postData2)); 
        http.print(postData2); 
        Serial.println(postData);
        http.print(sensorData); 
        http.endRequest(); 
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
    startTime = 0; 
    http.stop();
}

String downloadData() {
    WiFiClient wifiClient;
    HttpClient http(wifiClient);
    String responseBody = "";


    char postData[5]; 
    snprintf(postData, sizeof(postData), "%d\n",device_id);

    http.beginRequest();

    int err = http.post(kHostname, kPort, pPath);
    if (err == 0) {
        Serial.println("Started HTTP request");

        http.sendHeader("Content-Type", contentType);
        http.sendHeader("Content-Length", strlen(postData));
        
        http.print(postData);
        Serial.println(postData);
        http.endRequest();
        Serial.println("Request sent successfully.");

        err = http.responseStatusCode();
        if (err >= 0) {
            Serial.print("Response code: ");
            Serial.println(err);

            err = http.skipResponseHeaders();
            if (err >= 0) {
                int bodyLen = http.contentLength();
                Serial.print("Content length: ");
                Serial.println(bodyLen);

                unsigned long timeoutStart = millis();

                while ((http.connected() || http.available()) && ((millis() - timeoutStart) < kNetworkTimeout)) {
                    if (http.available()) {
                        char c = http.read();
                        responseBody += c;
                        timeoutStart = millis(); 
                    } else {
                        delay(kNetworkDelay); 
                    }
                }

                Serial.println("Response body: ");
                Serial.println(responseBody);
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



    http.stop();
    return responseBody;
}




void parseWorkouts(String data) {
  int startIndex = 0;
  int endIndex = data.indexOf(',');
  
  plan_id = data.substring(startIndex, endIndex);
  startIndex = endIndex + 1;
  endIndex = data.indexOf(',', startIndex);
  workout_count = data.substring(startIndex,endIndex).toInt();
  Serial.println(workout_count);  
  startIndex = endIndex + 1;

  
  for (int i = 0; i < workout_count; i++) {
      endIndex = data.indexOf(',', startIndex);
      String name = data.substring(startIndex, endIndex);
      startIndex = endIndex + 1;
      endIndex = data.indexOf(',', startIndex);
      float weight = data.substring(startIndex, endIndex).toFloat();
      startIndex = endIndex + 1;
      endIndex = data.indexOf(',', startIndex);
      int sets = data.substring(startIndex, endIndex).toInt();
      startIndex = endIndex + 1;
      endIndex = data.indexOf(',', startIndex);
      int reps_per_set = data.substring(startIndex, endIndex).toInt();
      startIndex = endIndex + 1;
      endIndex = data.indexOf(',', startIndex);
      int rest_time = data.substring(startIndex, endIndex).toInt();
      startIndex = endIndex + 1;

      workouts[i] = {name, weight, sets, reps_per_set, rest_time};
    
}}

void printWorkouts() {
  Serial.print("Plan ID: ");
  Serial.println(plan_id);
  Serial.print("Workout Count: ");
  Serial.println(workout_count);
  
  for (int i = 0; i < workout_count; i++) {
    Serial.print("Workout: ");
    Serial.print(workouts[i].name);
    Serial.print(", Duration: ");
    Serial.print(workouts[i].weight);
    Serial.print(", Sets: ");
    Serial.print(workouts[i].sets);
    Serial.print(", Reps per set: ");
    Serial.print(workouts[i].reps_per_set);
    Serial.print(", Rest time: ");
    Serial.println(workouts[i].rest_time);
  }
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


  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  String input_data = downloadData();
  
  parseWorkouts(input_data);
  
  printWorkouts();
  snprintf(buffer, sizeof(buffer), "Next: %s   weight %.2f ,press the button when you are ready", workouts[current_exercise].name,workouts[current_exercise].weight); 
  displayText(buffer);
  lastUpdate = millis();
  Serial.println(buffer);
}

void handleIdle() {   
    unsigned long currentMillis = millis();

    if (digitalRead(buttonPin) == LOW) {
        countdown = 3;
        currentState = COUNTDOWN;
        Serial.println("Countdown started...");
        displayText("Countdown started...");
        lastUpdate = millis();
    }
    if (currentMillis - lastUpdate >= interval) {
          tone(buzzerPin, 1750,200);
          lastUpdate = currentMillis;
          noTone(buzzerPin); 
          
    }
    

}

void handleCountdown() {
   unsigned long currentMillis = millis();
    if (currentMillis - lastUpdate >= interval) {
      lastUpdate = currentMillis;

      if (countdown > 0) {
        Serial.print("Countdown: ");
        Serial.println(countdown);
        snprintf(buffer, sizeof(buffer), "Countdown:  %d", countdown); // Format the string
        displayText(buffer); // Pass the formatted string to the display function
        if (countdown == 1){
        tone(buzzerPin, 2500, 500); 
      }
      else{
        tone(buzzerPin, 1000, 500); 
      }
        countdown--;
      } else {
        countdownStarted = false;
        counting = true;
        Serial.println("Start counting!");
        displayText("Start counting!");
        currentState = COUNTING;
      }
    }
    return; 
  }

void handleCounting()
 {

    float accelX = myIMU.readFloatAccelX();
    float accelY = myIMU.readFloatAccelY();
    float accelZ = myIMU.readFloatAccelZ();
    float gyroX = myIMU.readFloatGyroX();
    float gyroY = myIMU.readFloatGyroY();
    float gyroZ = myIMU.readFloatGyroZ();
    unsigned long currentMillis = millis();
    if (startTime == 0) {
      startTime = currentMillis; 
    }
    unsigned long elapsedTime = currentMillis - startTime;
    char dataStr[80];  
    //Serial.print("Elapsed Time: ");
    //Serial.println(elapsedTime);

    snprintf(dataStr, sizeof(dataStr), "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n",accelX, accelY, accelZ,gyroX, gyroY, gyroZ,elapsedTime); // Format the string
    sensorData += dataStr;  
    datapointsNumbers++; 
   
    
    float accel = max(max(abs(accelX), abs(accelY)), abs(accelZ));


    if (accel > threshold_up && !isLifted) {
      isLifted = true; 
      Serial.println("Lifted");
    }

    if (accel < threshold_down && isLifted) {
      isLifted = false; 
      liftCount++;      
      Serial.println("Put Down");
      Serial.print("Lift Count: ");
      Serial.println(liftCount);

      snprintf(buffer, sizeof(buffer), "Finised:  %d    reps", liftCount); // Format the string
      displayText(buffer); // Pass the formatted string to the display function
      detectState = true;
      detectTime = currentMillis;

      tone(buzzerPin, 1000, 100); 

      ledState = true;
      ledTime = currentMillis;
    }

  if (detectState && currentMillis - detectTime > detectDuration) {
    detectState = false;}

   if (ledState && currentMillis - ledTime < ledDuration) {
    digitalWrite(ledPin, HIGH); 
  } else if (ledState && currentMillis - ledTime >= ledDuration) {
    digitalWrite(ledPin, LOW); 
    ledState = false;      
  }


    if (liftCount >= workouts[current_exercise].reps_per_set) {
      delay(100); 
      Serial.print("Completed ");
      Serial.print(currentSet);
      Serial.println(" set(s)!");
      snprintf(buffer, sizeof(buffer), "Completed     %d sets", currentSet); 
      displayText(buffer); 
      
      tone(buzzerPin, 3000, 2000); 
      noTone(buzzerPin); 
      uploadData(); 

      liftCount = 0;   

      if (currentSet >= workouts[current_exercise].sets) {

        if(current_exercise>=workout_count-1){
        Serial.print(currentSet);
        Serial.println("All sets completed! Well done!");
        displayText("All sets completed! Well done!");
        while (true); 
        }
        else{
        current_exercise++; 
        currentSet =1;
        currentState = IDLE; 
        snprintf(buffer, sizeof(buffer), "Next: %s   weight %.2f ,press the button when you are ready", workouts[current_exercise].name,workouts[current_exercise].weight); // Format the string
        displayText(buffer);
        }
      }
       else {
        currentState = RESTING; 
        restFinished = false;   
        restCountdown = workouts[current_exercise].rest_time;    
        currentSet++;           
        lastUpdate = millis();  
      }
    }

    delay(20); 
  }

void handleResting()
{
   unsigned long currentMillis = millis();
    if (currentMillis - lastUpdate >= interval) {
      lastUpdate = currentMillis;

      if (restCountdown > 0) {
        Serial.print("Rest time: ");
        Serial.println(restCountdown);
        snprintf(buffer, sizeof(buffer), "Rest time:  %d", restCountdown); // Format the string
        displayText(buffer); 
        restCountdown--;
      } else {
        tone(buzzerPin, 1750,100);
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



