/* ErnstiBox
*
* ..is a small cube for children which can play sounds or stories. The audio files are
* selected using RFID tags. 
*
* Author: Ernst M Lübeck
* Date: 2020-09-05
*/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#include <play_sd_mp3.h>

#include <MFRC522.h>

/* RFID module pins */
#define PIN_RFID_RST 5 
#define PIN_RFID_CS 8 // SDA

/* NRF12 A, prototype 1*/
// #define IRQ_PIN 5
// #define CE_PIN 6 
// #define CSN_PIN 8

//#define DEBUG_CPU_USAGE

/* Built-in SD card pins */
#define PIN_SDCARD_CS BUILTIN_SDCARD
#define PIN_SDCARD_MOSI 7  // not actually used
#define PIN_SDCARD_MISO 12
#define PIN_SDCARD_SCK 14  // not actually used

#define PLAYING 1
#define PAUSING 2
#define WAITING 3

// GUItool: begin automatically generated code
AudioPlaySdMp3           Mp3Player;       //xy=154,78
AudioOutputI2S           i2s1;           //xy=334,89s
AudioConnection          patchCord1(Mp3Player, 0, i2s1, 0);
AudioConnection          patchCord2(Mp3Player, 1, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=240,153
// GUItool: end automatically generated code

MFRC522 rfid(PIN_RFID_CS, PIN_RFID_RST); // Create RFID MFRC522 instance

int StPlayer = -1;
int uid_temp = -1; /* temporary uid */
int uid_k = -1; /* newest UID[k] */
int uid_kn1 = -1; /* last UID[k-1] */
boolean FlagCardPresent = 0;

void handleRfidCards(boolean* FlagCardPresent_, int* uid_k_);
int getRfidUid();

void setup() 
{
    Serial.begin(9600);
    while (!Serial);

    Serial.print("Init AudioShield..");
    AudioMemory(5);

    sgtl5000_1.enable();
    sgtl5000_1.volume(0.5);
    Serial.println(" Done.");

    SPI.setMOSI(PIN_SDCARD_MISO);
    SPI.setMOSI(PIN_SDCARD_MOSI);
    SPI.setSCK(PIN_SDCARD_SCK);

    Serial.print("Init MFRC522..");

    SPI.begin();			// Init SPI bus
	rfid.PCD_Init();		// Init MFRC522
	delay(4);				// Optional delay. Some board do need more time after init to be ready, see Readme
	rfid.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details

    Serial.println(" Done.");

    Serial.print("Access SD Card..");
    if (!(SD.begin(PIN_SDCARD_CS))) 
    {
        // stop here, but print a message repetitively
        while (1) 
        {   Serial.println();
            Serial.println("Unable to access the SD card");
            delay(500);
        }
    }
    Serial.println(" Done.");

    StPlayer = WAITING;
    Serial.println("WAITING");
}

void loop() 
{
    switch(StPlayer)
    {
        case PLAYING:

        while (Mp3Player.isPlaying()) 
        {
            handleRfidCards(&FlagCardPresent, &uid_temp);

            if(uid_temp != -1)
            {
                uid_k = uid_temp;
            }

            if(!FlagCardPresent)
            {   Mp3Player.pause(1);
                StPlayer = PAUSING;
                Serial.println("PAUSING");
                break;
            }

            delay(250);
        }
        if(StPlayer != PAUSING)
        {
            StPlayer = WAITING;
            Serial.println("WAITING");
        }
        
        break;

        case PAUSING: 

        handleRfidCards(&FlagCardPresent, &uid_temp);

        if(uid_temp != -1)
        {
            uid_k = uid_temp;
        }

        if(FlagCardPresent)
        {
            if(uid_k == uid_kn1)
            {
                Mp3Player.pause(0);            
                
                StPlayer = PLAYING;
                Serial.println("PLAYING");
            }
            else
            {
                StPlayer = WAITING;
                Serial.println("WAITING");
            }
        }
        
        break;

        case WAITING: 
          
        handleRfidCards(&FlagCardPresent, &uid_temp);

        if(uid_temp != -1)
        {
            uid_k = uid_temp;
        }

        if(uid_k == 105)
        {
            Mp3Player.play("Esel.mp3");

            StPlayer = PLAYING;
            Serial.println("PLAYING");
        }
        else if(uid_k == 249)
        {
            Mp3Player.play("Kuh.mp3");

            StPlayer = PLAYING;
            Serial.println("PLAYING");
        }
  
        break;

        default: StPlayer = WAITING; break;
    }

    uid_kn1 = uid_k;

    
#ifdef DEBUG_CPU_USAGE	
    Serial.print("Max Usage: ");
    Serial.print(Mp3Player.processorUsageMax());
    Serial.print("% Audio, ");

    Serial.print(AudioProcessorUsageMax());	 
    Serial.println("% All");
    
    AudioProcessorUsageMaxReset();
    Mp3Player.processorUsageMaxReset();
    Mp3Player.processorUsageMaxResetDecoder();
#endif 

}

void handleRfidCards(boolean* FlagCardPresent_, int* uid_k_)
{
    int uid = -1;

    uid = getRfidUid();

    if(uid == -1)
    {
        *(FlagCardPresent_) = 0;
    }
    else
    {
        *(FlagCardPresent_) = 1;
    }

    *(uid_k_) = uid;
}

int getRfidUid()
{
    int uid = -1;

    if (!rfid.PICC_IsNewCardPresent())
    {
        if (!rfid.PICC_IsNewCardPresent())
        {
            /* work around to check if card is still there */
        }
    }
    if(rfid.PICC_ReadCardSerial())
    {
        uid = rfid.uid.uidByte[0];
    }    

    return(uid);
}