#include "Arduino.h"
#include "Audio.h"
#include "SD.h"
#include "SPI.h"
#include "Wire.h"

#define SD_MISO_PIN     4
#define SD_MOSI_PIN     6
#define SD_SCK_PIN      5
#define SD_CS_PIN       7

#define I2S_BCLK_PIN     5
#define I2S_LRCK_PIN     6
#define I2S_DOUT_PIN     7

#define HP_DETECT_PIN    19  
#define AUTO_PAUSE_PIN   20  

#define I2C_SDA_PIN      38
#define I2C_SCL_PIN      39
#define CIRQUE_I2C_ADDR  0x2A 

Audio audio;
bool headphoneConnected = false;
int currentVolume = 12;         
unsigned long lastTrackpadPoll = 0;
const int pollInterval = 30;   

struct TrackpadData {
    bool valid;
    bool touchDown;
    int8_t deltaX;
    int8_t deltaY;
};


void initTrackpad() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
    
    
    Wire.beginTransmission(CIRQUE_I2C_ADDR);
    Wire.write(0x05);
    Wire.write(0x00);
    Wire.endTransmission();
    delay(10);
}

TrackpadData readTrackpad() {
    TrackpadData data = {false, false, 0, 0};
    
    byte count = Wire.requestFrom(CIRQUE_I2C_ADDR, 4);
    if (count == 4) {
        byte status = Wire.read();
        data.deltaX = (int8_t)Wire.read();
        data.deltaY = (int8_t)Wire.read();
        Wire.read();
        
        data.touchDown = (status & 0x01); 
        data.valid = true;
    }
    return data;
}

// Manage Trackpad Inputs to Change Tracks/Volume
void handleNavigation() {
    if (millis() - lastTrackpadPoll < pollInterval) return;
    lastTrackpadPoll = millis();

    TrackpadData pad = readTrackpad();
    if (!pad.valid || !pad.touchDown) return;

    // Volume Adjustment via Vertical Swipe (Y-Axis Threshold)
    if (pad.deltaY > 15) {
        if (currentVolume < 21) {
            currentVolume++;
            audio.setVolume(currentVolume);
            Serial.printf("Volume Up: %d\n", currentVolume);
        }
    } else if (pad.deltaY < -15) {
        if (currentVolume > 0) {
            currentVolume--;
            audio.setVolume(currentVolume);
            Serial.printf("Volume Down: %d\n", currentVolume);
        }
    }
    if (pad.deltaX > 25) {
        Serial.println("Trackpad Gestured: Next Track Triggered");
    } else if (pad.deltaX < -25) {
        Serial.println("Trackpad Gestured: Previous Track Triggered");
    }
}

void handleHeadphoneState() {
    bool currentJackState = (digitalRead(HP_DETECT_PIN) == HIGH);

    if (currentJackState != headphoneConnected) {
        headphoneConnected = currentJackState;

        if (headphoneConnected) {
            Serial.println("🎧 Headphones Plugged In! Enabling Analog Stage...");
            
            digitalWrite(AUTO_PAUSE_PIN, HIGH); 
            delay(50); 
            
            if (!audio.isRunning()) {
                audio.pauseResume(); 
            }
        } 
        else {
            Serial.println("🔇 Headphones Disconnected! Entering Auto-Pause Mode...");
            
            if (audio.isRunning()) {
                audio.pauseResume(); 
            }
            
            digitalWrite(AUTO_PAUSE_PIN, LOW); 
        }
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(HP_DETECT_PIN, INPUT); 
    pinMode(AUTO_PAUSE_PIN, OUTPUT);
    
    digitalWrite(AUTO_PAUSE_PIN, LOW);

    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("Error: MicroSD Card Mount Failed!");
        while (true);
    }
    Serial.println("MicroSD Mounted Successfully.");

    initTrackpad();

    audio.setPinout(I2S_BCLK_PIN, I2S_LRCK_PIN, I2S_DOUT_PIN);
    audio.setVolume(currentVolume);

    headphoneConnected = (digitalRead(HP_DETECT_PIN) == HIGH);
    if (headphoneConnected) {
        digitalWrite(AUTO_PAUSE_PIN, HIGH);
        Serial.println("Booting System: Audio Stage Enabled.");
    }

    audio.connecttoFS(SD, "/music.mp3");
}

void loop() {
    if (headphoneConnected) {
        audio.loop();
    }

    handleHeadphoneState();

    handleNavigation();
}

void audio_info(const char *info) {
    Serial.printf("Audio Engine Update: %s\n", info);
}