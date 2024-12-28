#include <WiFiS3.h>
#include <ArduinoHttpClient.h>
#include "arduinoFFT.h"

// WiFi Credentials
const char* ssid = "Luke's Dog and Bone";
const char* password = "bakedbeanz";

// ThingSpeak Configuration
const char* THINGSPEAK_SERVER = "api.thingspeak.com";
const int THINGSPEAK_PORT = 80;
const char* THINGSPEAK_WRITE_API_KEY = "7WL88R28KR8C2SF6";
const char* THINGSPEAK_READ_API_KEY = "0QC4GFO5IJ4RY5IE";
const unsigned long CHANNEL_ID = 2791684;

// Server details for local server
IPAddress serverIP(10, 191, 152, 250);
const int serverPort = 8000;

// Timing intervals
const unsigned long REFRESH_INTERVAL = 250;     // 250ms = 4 readings per second
const unsigned long THINGSPEAK_INTERVAL = 15000; // 15 seconds between ThingSpeak uploads
unsigned long lastReadingTime = 0;
unsigned long lastThingSpeakTime = 0;

// Create WiFi clients
WiFiClient wifi;
WiFiClient thingSpeakClient;
HttpClient httpClient = HttpClient(wifi, serverIP, serverPort);

// Pin Definitions
const int LED_LOW_FREQ = 2;
const int LED_IN_TUNE = 3;
const int LED_HIGH_FREQ = 4;
const int SPEAKER_PIN = A1;

// Improved FFT settings
const uint16_t SAMPLES = 256;
const double SAMPLING_FREQ = 2048;
const double AMPLITUDE_THRESHOLD = 30;

// Frequency bounds for noise filtering
const double MIN_FREQ = 75.0;
const double MAX_FREQ = 350.0;

// Guitar frequencies (E2 to E4)
const double NOTES[] = {82.41, 110.0, 146.8, 196.0, 246.9, 329.6};
const char* NOTE_NAMES[] = {"E2", "A2", "D3", "G3", "B3", "E4"};
const uint8_t NOTE_COUNT = 6;

// More forgiving tuning tolerance
const double TUNE_TOLERANCE = 75.0;

// Increased oversampling for more stable readings
const int OVERSAMPLE = 32;
const int FREQ_SAMPLES = 3;

// FFT variables
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// Function Prototypes
void setLEDs(bool lowFreq, bool inTune, bool highFreq);
void playInTuneTone();
void sendDataToServer(double frequency, String status);
bool uploadToThingSpeak(double frequency, String status, int closestNote, double cents);
void processTunerReading();

// LED Control Function
void setLEDs(bool lowFreq, bool inTune, bool highFreq) {
    digitalWrite(LED_LOW_FREQ, lowFreq);
    digitalWrite(LED_IN_TUNE, inTune);
    digitalWrite(LED_HIGH_FREQ, highFreq);
}

// Speaker Tone Function
void playInTuneTone() {
    tone(SPEAKER_PIN, 1000, 200);
}

// Send Data to Local Server
void sendDataToServer(double frequency, String status) {
    String jsonPayload = "{\"frequency\": " + String(frequency, 2) + 
                         ", \"status\": \"" + status + "\"}";
    
    Serial.println("Sending data to local server...");
    
    httpClient.beginRequest();
    httpClient.post("/");
    httpClient.sendHeader("Content-Type", "application/json");
    httpClient.sendHeader("Content-Length", jsonPayload.length());
    httpClient.beginBody();
    httpClient.print(jsonPayload);
    httpClient.endRequest();

    int statusCode = httpClient.responseStatusCode();
    Serial.print("Local Server Response Status: ");
    Serial.println(statusCode);
}

// Updated ThingSpeak Upload Function with timing control
bool uploadToThingSpeak(double frequency, String status, int closestNote, double cents) {
    unsigned long currentTime = millis();
    
    // Check if enough time has passed since last upload
    if (currentTime - lastThingSpeakTime < THINGSPEAK_INTERVAL) {
        return false;  // Skip upload if not enough time has passed
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected! Attempting to reconnect...");
        WiFi.begin(ssid, password);
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 10) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("\nFailed to reconnect WiFi!");
            return false;
        }
        Serial.println("\nWiFi reconnected!");
    }

    int statusValue = (status == "In Tune") ? 1 : 
                     (status == "Too Low") ? 0 : 2;

    unsigned long timestamp = currentTime / 1000;  // Convert to seconds

    String postData = "api_key=";
    postData += THINGSPEAK_WRITE_API_KEY;
    postData += "&field1=";
    postData += String(frequency, 2);
    postData += "&field2=";
    postData += String(statusValue);
    postData += "&field3=";
    postData += NOTE_NAMES[closestNote];
    postData += "&field4=";
    postData += String(cents, 2);
    postData += "&field5=";
    postData += String(timestamp);

    Serial.println("\n--- ThingSpeak Upload Debug ---");
    Serial.println("Data to send:");
    Serial.println("Field 1 (Frequency): " + String(frequency, 2) + " Hz");
    Serial.println("Field 2 (Status): " + String(statusValue));
    Serial.println("Field 3 (Note): " + String(NOTE_NAMES[closestNote]));
    Serial.println("Field 4 (Cents): " + String(cents, 2));
    Serial.println("Field 5 (Timestamp): " + String(timestamp) + " s");

    if (!thingSpeakClient.connect(THINGSPEAK_SERVER, THINGSPEAK_PORT)) {
        Serial.println("ThingSpeak connection failed");
        return false;
    }

    thingSpeakClient.println("POST /update HTTP/1.1");
    thingSpeakClient.println("Host: api.thingspeak.com");
    thingSpeakClient.println("Connection: close");
    thingSpeakClient.println("Content-Type: application/x-www-form-urlencoded");
    thingSpeakClient.print("Content-Length: ");
    thingSpeakClient.println(postData.length());
    thingSpeakClient.println();
    thingSpeakClient.println(postData);

    unsigned long timeout = millis();
    while (thingSpeakClient.available() == 0) {
        if (millis() - timeout > 5000) {
            Serial.println(">>> ThingSpeak Client Timeout!");
            thingSpeakClient.stop();
            return false;
        }
    }

    Serial.println("ThingSpeak Server Response:");
    bool success = false;
    while (thingSpeakClient.available()) {
        String line = thingSpeakClient.readStringUntil('\r');
        Serial.print(line);
        if (line.indexOf("200 OK") != -1) {
            success = true;
        }
    }

    thingSpeakClient.stop();
    
    if (success) {
        lastThingSpeakTime = currentTime;  // Update last upload time
        Serial.println("\nData successfully uploaded to ThingSpeak!");
    } else {
        Serial.println("\nFailed to upload data to ThingSpeak");
    }

    return success;
}

void setup() {
    Serial.begin(9600);
    while (!Serial);

    pinMode(LED_LOW_FREQ, OUTPUT);
    pinMode(LED_IN_TUNE, OUTPUT);
    pinMode(LED_HIGH_FREQ, OUTPUT);
    pinMode(SPEAKER_PIN, OUTPUT);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    Serial.println("Testing ThingSpeak connection...");
    if (uploadToThingSpeak(0.0, "In Tune", 0, 0.0)) {
        Serial.println("ThingSpeak connection test successful!");
    } else {
        Serial.println("ThingSpeak test failed!");
    }

    lastReadingTime = millis();  // Initialize timing variables
    lastThingSpeakTime = millis();
}

void processTunerReading() {
    // Sampling with improved noise reduction
    for (uint16_t i = 0; i < SAMPLES; i++) {
        unsigned long microseconds = micros();
        
        long sum = 0;
        for(int j = 0; j < OVERSAMPLE; j++) {
            sum += analogRead(A0);
            delayMicroseconds(5);
        }
        int sample = sum / OVERSAMPLE;
        
        vReal[i] = sample;
        vImag[i] = 0;
        
        while(micros() < (microseconds + (1000000UL/SAMPLING_FREQ))) {
            // Precise timing wait
        }
    }

    // DC offset removal and scaling
    double avgValue = 0;
    for(uint16_t i = 0; i < SAMPLES; i++) {
        avgValue += vReal[i];
    }
    avgValue /= SAMPLES;
    for(uint16_t i = 0; i < SAMPLES; i++) {
        vReal[i] = (vReal[i] - avgValue) * 0.5;
    }
    
    // FFT Processing with frequency averaging
    FFT.windowing(FFT_WIN_TYP_BLACKMAN_HARRIS, FFT_FORWARD);
    FFT.compute(FFT_FORWARD);
    FFT.complexToMagnitude();

    // Average multiple frequency readings for stability
    double totalFreq = 0;
    for(int i = 0; i < FREQ_SAMPLES; i++) {
        totalFreq += FFT.majorPeak();
    }
    double peakFrequency = totalFreq / FREQ_SAMPLES;
    
    // Frequency validation
    if (peakFrequency < MIN_FREQ || peakFrequency > MAX_FREQ) {
        setLEDs(false, false, false);
        return;
    }

    // Find closest note
    int closestNote = -1;
    double minDiff = 1000;
    
    for (int i = 0; i < NOTE_COUNT; i++) {
        double diff = abs(1200 * log2(peakFrequency / NOTES[i]));
        if (diff < minDiff) {
            minDiff = diff;
            closestNote = i;
        }
    }

    if (closestNote >= 0) {
        double cents = 1200 * log2(peakFrequency / NOTES[closestNote]);
        String status;
        static bool wasInTune = false;
        
        if (abs(cents) < TUNE_TOLERANCE) {
            status = "In Tune";
            setLEDs(false, true, false);
            if (!wasInTune) {
                playInTuneTone();
                wasInTune = true;
            }
        } else {
            wasInTune = false;
            if (cents < 0) {
                status = "Too Low";
                setLEDs(true, false, false);
            } else {
                status = "Too High";
                setLEDs(false, false, true);
            }
        }
        
        // Send data to both servers
        sendDataToServer(peakFrequency, status);
        uploadToThingSpeak(peakFrequency, status, closestNote, cents);
        
        // Debug output
        Serial.print("Frequency: ");
        Serial.print(peakFrequency, 2);
        Serial.print(" Hz, Note: ");
        Serial.print(NOTE_NAMES[closestNote]);
        Serial.print(", Cents Off: ");
        Serial.print(cents, 1);
        Serial.print(", Status: ");
        Serial.println(status);
    }
}

// Updated loop with non-blocking timing
void loop() {
    unsigned long currentTime = millis();
    
    // Check if it's time for a new reading
    if (currentTime - lastReadingTime >= REFRESH_INTERVAL) {
        processTunerReading();
        lastReadingTime = currentTime;
    }
}
