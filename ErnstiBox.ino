/* ErnstiBox
*
* ..is a small cube for children which can play sounds or stories. The audio files are
* selected using RFID tags and played from an SD card to be changed over time. 
*
* Todo:
* 1. Touch buttons for volume control
* 2. Auto shut-off after some time in WAITING/PAUSE
* 3. Fade out sound before pausing
*
* Author: Ernst M Lübeck
* Date: 2020-09-05
*/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FastLED.h> 

#include <play_sd_mp3.h>

#include <MFRC522.h>

/* Power Button */
#define PIN_POWER_HOLD 6
#define PIN_BUTTON_STATE 3

/* Push button pins */
#define PIN_PB_UP 3 /* Also switches power MOSFET */
#define PIN_PB_OK 2
#define PIN_PB_DOWN  22

#define PIN_RGB_LED 1

#define PIN_VBAT A1

/* Audio amplifier !shutdown */
#define PIN_PAM_SD 4

/* RFID module pins */
#define PIN_RFID_RST 9 
#define PIN_RFID_CS 16 // SDA

//#define DEBUG_CPU_USAGE
//#define DEBUG_RFID_UID

/* Built-in SD card pins */
#define PIN_SDCARD_CS 10
#define PIN_SDCARD_MOSI 11  // not actually used
#define PIN_SDCARD_MISO 12
#define PIN_SDCARD_SCK 14  // not actually used

#define COLOR_ON CRGB(220, 220, 220)
#define COLOR_CARD_PRESENT CRGB(0, 0, 0) //CRGB(255, 220, 255)

/* AUDIO SHIELD PIN MAPPING Teensy 4.0 */
// I2C SDA = 18
// I2C SCL = 19
// I2S DOUT TX = 8
// I2S DIN RX = 7
// I2S LRCLK =  20 (44.1 kHz)
// BLCK = 21 (1.41 MHz)
// MCLK = 23 (11.29 MHz)
// SD SPI MOSI = 11
// SD SPI CS = 10
// SD SPI MISO = 12
// SD SPI SCK = 13
// VOL POT = 15 (A1)

/* Main states */
#define PLAYING 1
#define PAUSING 2
#define WAITING 3
#define SHUTDOWN 4

#define VOLUME 0.5 /* 4 Ohm speaker only 0.1! */
#define RFID_POLLING_DELAY 100 /* [ms], delay between RFID tak polling event */
#define TIME_SHUTDOWN 300000 /* [ms], time in idle to turn off device */

/* RDID card UID to sound mapping */
#define SOUND_DONKEY 105
#define SOUND_COW 249
#define SOUND_CAT 41
#define SOUND_DOG 227

// GUItool: begin automatically generated code
AudioPlaySdMp3           Mp3Player;       //xy=154,78
AudioOutputI2S           i2s1;           //xy=334,89s
AudioMixer4              mixer1; 
AudioConnection          patchCord1(Mp3Player, 0, mixer1, 0);
AudioConnection          patchCord2(Mp3Player, 1, mixer1, 1);
AudioConnection          patchCord3(mixer1, 0, i2s1, 0);
AudioConnection          patchCord4(mixer1, 0, i2s1, 1);

AudioControlSGTL5000     sgtl5000;     //xy=240,153
// GUItool: end automatically generated code

MFRC522 rfid(PIN_RFID_CS, PIN_RFID_RST); // Create RFID MFRC522 instance

CRGB leds[1];

int StPlayer = -1;
int uid_temp = -1; /* temporary uid */
int uid_k = -1; /* newest UID[k] */
int uid_kn1 = -1; /* last UID[k-1] */
boolean FlagCardPresent = 0;

unsigned long TiIdleStart = 0;

void handleRfidCards(boolean* FlagCardPresent_, int* uid_k_);
int getRfidUid();

void setup() 
{
    /* Keep MOSFET on power module turned on */
    pinMode(PIN_POWER_HOLD, OUTPUT);
    digitalWrite(PIN_POWER_HOLD, HIGH);

    /* Init RGB LED */
    FastLED.addLeds<WS2812, PIN_RGB_LED, GRB>(leds, 1);
    FastLED.setBrightness(255);
    leds[0] = COLOR_ON;
    FastLED.show();

    pinMode(PIN_PAM_SD, OUTPUT);
    digitalWrite(PIN_PAM_SD, HIGH);

    /* Power button state */
    pinMode(PIN_BUTTON_STATE, INPUT);
    digitalWrite(PIN_BUTTON_STATE, HIGH);

    Serial.begin(9600);
    //while (!Serial); /* does not work without PC! */
    delay(250);

    Serial.print("Init AudioShield..");
    AudioMemory(5);

    sgtl5000.enable();
    sgtl5000.volume(0.5);
    sgtl5000.lineOutLevel(29); /* 20 = 2.14 Vpp, 31 = 1.16 Vpp */

    float LvlSoundDB = -16;
    float LvlSoundLin = pow(10, LvlSoundDB/20);

    mixer1.gain(0, LvlSoundLin); /* mp3 L */
    mixer1.gain(1, LvlSoundLin); /* mp3 R */
    mixer1.gain(2, 0.0); /* not connected */
    mixer1.gain(3, 0.0); /* not connected */

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

    while (!(SD.begin(PIN_SDCARD_CS))) 
    {   
        Serial.println("Unable to access the SD card");
        delay(500);
    }

    Serial.println(" Done.");

    StPlayer = WAITING;

    TiIdleStart = millis();
}

void loop() 
{
    switch(StPlayer)
    {
        case PLAYING:

        leds[0] = COLOR_CARD_PRESENT;
        FastLED.show();

        while (Mp3Player.isPlaying()) 
        {
            TiIdleStart = millis();
            handleRfidCards(&FlagCardPresent, &uid_temp);

            if(uid_temp != -1)
            {
                uid_k = uid_temp;
            }

            if(!FlagCardPresent)
            {   Mp3Player.pause(1);
                StPlayer = PAUSING;
                TiIdleStart = millis();
                break;
            }

            delay(RFID_POLLING_DELAY);
        }
        if(StPlayer != PAUSING)
        {
            StPlayer = WAITING;
            TiIdleStart = millis();
        }
        
        break;

        case PAUSING: 

        handleRfidCards(&FlagCardPresent, &uid_temp);

        leds[0] = COLOR_ON;
        FastLED.show();

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
            }
            else
            {
                StPlayer = WAITING;
                TiIdleStart = millis();
            }
        }
        
        break;

        case WAITING: 
          
        handleRfidCards(&FlagCardPresent, &uid_temp);

        if(uid_temp != -1)
        {
            uid_k = uid_temp;
        }

        if(uid_k == SOUND_DONKEY)
        {
            Mp3Player.play("Donkey.mp3");

            StPlayer = PLAYING;
        }
        else if(uid_k == SOUND_COW)
        {
            Mp3Player.play("Cow.mp3");

            StPlayer = PLAYING;
        }
        else if(uid_k == SOUND_CAT)
        {
            Mp3Player.play("Cat.mp3");

            StPlayer = PLAYING;
        }
        else if(uid_k == SOUND_DOG)
        {
            Mp3Player.play("Dog.mp3");

            StPlayer = PLAYING;
        }
  
        break;

        case SHUTDOWN:
            /* Turn off MOSFET on power module */
            digitalWrite(PIN_PAM_SD, LOW);
            digitalWrite(PIN_POWER_HOLD, LOW);
            
            StPlayer = WAITING; 
            delay(2000);
        break;

        default: 

        StPlayer = WAITING; 
        TiIdleStart = millis();
        break;
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

    delay(RFID_POLLING_DELAY);

    if((millis()-TiIdleStart) >= TIME_SHUTDOWN)
    {
        StPlayer = SHUTDOWN; 
        TiIdleStart = millis();
    }

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

#ifdef DEBUG_RFID_UID
    Serial.print("UID = ");
    Serial.println(uid);
#endif

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