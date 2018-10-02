
#include <Arduino.h>
#include <ArduinoJson.h>
#include "mbedtls/aes.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "OneButton.h"
#include "driver/adc.h"
#include <Preferences.h>
#include <exception>

BLEServer *pServer = NULL;
BLEAdvertising *pAdvertising;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define MAGSPOOF_SERVICE_UUID "d9c28aa0-363b-4c5a-9be9-e41f7937786f"
#define MAGSPOOF_RX_CHARACTERISTIC_UUID "d9c28aa1-363b-4c5a-9be9-e41f7937786f"
#define TEST_LED_CHARACTERISTIC_UUID "d9c28aa2-363b-4c5a-9be9-e41f7937786f"
const uint16_t BATTERY_TX_CHARACTERISTIC_UUID = 0x2A19;

//Settings

// Measured in inches per second, or IPS
// Minimum 5 IPS, maximum 50 IPS
#define SWIPE_SPEED 10
#define PREFERENCES_APP_NAME "magspoofbt"

//Pin Numbers
#define BT_NOTIFY_LED LED_BUILTIN
#define COIL_ENABLE_PIN 25
#define COIL_A 26
#define COIL_B 27
#define BUTTON_PIN 14

//LED Blinks and their meanings
#define BLINK_BT_CONNECT 2  // blink 2 times on device connected
#define BLINK_BT_LED_TEST 5 // blink 5 times when testing the BT_NOTIFY_LED

//card schema
typedef struct
{
    char *tracks[3]; // track data
} card;

String cardToJson(card *c)
{
    String output;
    StaticJsonBuffer<400> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["track1"] = c->tracks[0];
    root["track2"] = c->tracks[1];
    root["track3"] = c->tracks[2];
    root.printTo(output);
    return output;
}

card *jsonToCard(String json)
{
    StaticJsonBuffer<400> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(json);

    String track1 = root["track1"];
    String track2 = root["track2"];
    String track3 = root["track3"];
    card *c = new card({
        {
            (char*)track1.c_str(),
            (char*)track2.c_str(),
            (char*)track3.c_str()
        }
    });
    return c;
}

//Global Instance Variables

OneButton button(BUTTON_PIN, true);

//<end> Global Instance Variables

// example card data for testing
const card test_card = {
    {"%B4444444444444444^ABE/LINCOLN^291110100000931?",
     ";4444444444444444=29111010000093100000?",
     0},
};
// Set a card's maximum size to 1kb
#define CARD_MAX_SIZE 1024

class MagSpoofUtil
{
  private:
    //Helper constants
    const uint16_t leading_zeros[3] = {10, 20, 10};
    const uint16_t trailing_zeros[3] = {10, 10, 10};
    const uint8_t track_density[3] = {210, 75, 210}; // bits per inch for each track
    const uint8_t track_sublen[3] = {32, 48, 48};
    const uint8_t track_bitlen[3] = {7, 5, 5};

    uint32_t track_period_us[3]; // bit period of each track in microseconds
                                 // initialized in calc_track_periods()

    bool f2f_pole; // used to track signal state during playback (F2F encoding)
    card *mcard;

  public:
    MagSpoofUtil(card *c)
    {
        calc_track_periods();
        mcard = c;
    }

  private:
    void calc_track_periods()
    {
        for (int i = 0; i < 3; i++)
            track_period_us[i] = (uint32_t)(1e6 / (SWIPE_SPEED * track_density[i]));
    }

    void invert_coil_pole()
    {
        f2f_pole ^= 1;
        digitalWrite(COIL_A, f2f_pole);
        digitalWrite(COIL_B, !f2f_pole);
    }

    void f2f_play_bit(bool bit, uint32_t period_us)
    {
        // play F2F encoded bit
        // see http://www.edn.com/Home/PrintView?contentItemId=4426351
        const uint32_t half_period = period_us / 2;
        // invert magnetic pole at start of period
        invert_coil_pole();

        delayMicroseconds(half_period);

        // at half period, determine whether to invert pole
        if (bit)
            invert_coil_pole();

        delayMicroseconds(half_period);
    }

    void play_zeros(uint8_t n, uint32_t period_us)
    {
        for (int i = 0; i < n; i++)
            f2f_play_bit(0, period_us);
    }
    void play_byte(
        uint8_t byte,
        uint8_t n_bits,
        uint32_t period_us,
        uint8_t *lrc)
    {
        bool parity = 1;
        for (int i = 0; i < n_bits; i++)
        {
            bool bit = byte & 1;
            parity ^= bit;
            // TIMING SENSITIVE
            f2f_play_bit(bit, period_us - 30); // subtract 30us to compensate for delay
                                               // caused by extra processing within this loop
            byte >>= 1;
            if (lrc)
                *lrc ^= bit << i;
        }
        f2f_play_bit(parity, period_us); // last bit is parity
    }
    void play_track(card *c, uint8_t track_num)
    {
        char *track = c->tracks[track_num];

        if (!track)
            return; // check that the track exists

        uint32_t period_us = track_period_us[track_num];
        char *track_byte;
        uint8_t lrc = 0;

        // lead playback with zeros
        play_zeros(leading_zeros[track_num], period_us);

        // play back track data
        for (track_byte = track; *track_byte != 0; track_byte++)
        {
            uint8_t tb = *track_byte - track_sublen[track_num];
            play_byte(tb, track_bitlen[track_num] - 1, period_us, &lrc);
        }

        // play LRC
        play_byte(lrc, track_bitlen[track_num] - 1, period_us, 0);

        // end playback
        play_zeros(trailing_zeros[track_num], period_us);
    }

  public:
    void play_card()
    {
        // let user know playback has begun by turning on LED
        digitalWrite(COIL_ENABLE_PIN, HIGH);
        digitalWrite(COIL_A, LOW);
        digitalWrite(COIL_B, LOW);

        f2f_pole = 0;

        for (int i = 0; i < 3; i++)
        {
            play_track(mcard, i);
            Serial.print("track ");
            Serial.print(i + 1);
            Serial.println("...");
        }

        // turn off H-Bridge and make sure coil is off
        digitalWrite(COIL_ENABLE_PIN, LOW);
        digitalWrite(COIL_A, LOW);
        digitalWrite(COIL_B, LOW);
    }
};

void blink(int pin, int times)
{
    // In case the LED was HIGH before blinking, simply invert
    // and restore HIGH state after running the blinks.
    bool origstate = digitalRead(pin);
    for (int i = 0; i < times; i++)
    {
        digitalWrite(pin, !origstate);
        delay(200);
        digitalWrite(pin, origstate);
        delay(200);
    }
}

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer){
        // deviceConnected = true;
    };

    void onDisconnect(BLEServer *pServer)
    {
        // deviceConnected = false;
    }
};

struct CardExistsException : public std::exception
{
    const char *what() const throw()
    {
        return "Card already exists";
    }
};

struct CardNotExistsException : public std::exception
{
    const char *what() const throw()
    {
        return "Card does not exist";
    }
};

class MagSpoofCallbacks : public BLECharacteristicCallbacks
{
  private:
    static card *selectedCard;
    const char *commands[3] = {"save", "delete", "setactive"};

    std::vector<String> splitStringToVector(String msg)
    {
        std::vector<String> subStrings;
        int j = 0;
        for (int i = 0; i < msg.length(); i++)
        {
            if (msg.charAt(i) == '|')
            {
                subStrings.push_back(msg.substring(j, i));
                j = i + 1;
            }
        }
        subStrings.push_back(msg.substring(j, msg.length())); //to grab the last value of the string
        return subStrings;
    }

    bool cardexists(String name)
    {
        // check if a value is returned from preferences for the name
        Preferences prefs;
        prefs.begin(PREFERENCES_APP_NAME, true);
        String cardjson;
        cardjson = prefs.getString(name.c_str(), "");
        prefs.end();
        return (cardjson != "");
    }

    void savecard(String name, card *c)
    {
        //Save a card to preferences with name as the key
        //Read an array from preferences with key "savedcards"
        //append this card's name to the resultant array, and save back to "savedcards"
        if (!cardexists(name))
        {
            Preferences prefs;
            prefs.begin(PREFERENCES_APP_NAME, false);
            Serial.println("Saving card as " + name);
            String json = cardToJson(c);
            Serial.println(json);
            prefs.putString(name.c_str(), json);
            prefs.end();
        }
        else
            throw CardExistsException();
    }

    void delcard(String name)
    {
        //Delete entry in preferences where name is the key
        //remove name from "savedcards" array stored in preferences.
        if (cardexists(name))
        {
            Preferences prefs;
            prefs.begin(PREFERENCES_APP_NAME, false);
            prefs.remove(name.c_str());
            prefs.end();
        }
        else
            throw CardNotExistsException();
    }

    void setactive(String name)
    {
        // set the preferences key "activecard" to the name of the newly active card
        // Check if "name" is defined in the preferences hash table first.
        if (cardexists(name))
        {
            Preferences prefs;
            prefs.begin(PREFERENCES_APP_NAME, false);
            prefs.putString("activecard", name);
            prefs.end();
        }
        else
            throw CardNotExistsException();
    }

  public:
    void onWrite(BLECharacteristic *pChar)
    {
        // Expected input for each command:
        // Json formatted string with these parameters:
        // name: Standard string
        // command: one of ["savecard", "delcard", "setactive"]
        // track1-3: String containting the data of each track
        String rxValue = pChar->getValue().c_str();
        Serial.println(rxValue);
        StaticJsonBuffer<400> jsonBuffer;
        Serial.print("...1");
        JsonObject &root = jsonBuffer.parseObject(rxValue);
        Serial.print("...2");
        Serial.println((const char *)root["command"]);
        Serial.print("...3");
        String command = root["command"];
        try
        {
            if (!root.success())
            {
                Serial.println("Could not read json");
                pChar->setValue("Could not read json");
                return;
            }
            if (command == "savecard")
            {
                String name = root["name"];
                String track1 = root["track1"];
                String track2 = root["track2"];
                String track3 = root["track3"];
                card c = {
                    {(char *)track1.c_str(),
                     (char *)track2.c_str(),
                     (char *)track3.c_str()}};
                savecard(name, &c);
            }
            else if (command == "delcard")
            {
                delcard(root["name"]);
            }
            else if (command == "setactive")
            {
                setactive(root["name"]);
            }
            else
            {
                pChar->setValue("Command not understood");
            }
        }
        catch (CardExistsException &e)
        {
            Serial.println("ERROR: Card already exists");
            pChar->setValue("ERROR: Card already exists");
        }
        catch (CardNotExistsException &e)
        {
            Serial.println("ERROR: Card does not exist");
            pChar->setValue("ERROR: Card does not exist");
        }
    }

    void onRead(BLECharacteristic *pChar)
    {
        // Get the name of the active card, and then pChar->setValue(<that name>);
        // Then, when the value of this characteristic is read,
        // it gets the name of the active card.
        Preferences prefs;
        prefs.begin(PREFERENCES_APP_NAME, true);
        pChar->setValue(prefs.getString("activecard", "_unknown_").c_str());
        prefs.end();
    }
};

class LedTestCallbacks : public BLECharacteristicCallbacks
{
    int querypin = BT_NOTIFY_LED;
    void onWrite(BLECharacteristic *pChar)
    {
        String rxValue = pChar->getValue().c_str();
        // querypin = rxValue.toInt();
        pinMode(querypin, OUTPUT);
        if (rxValue)
        {
            Serial.println("Received led TEST command");
            blink(querypin, BLINK_BT_LED_TEST);
        }
    }
};

// class BatteryCallbacks : public BLECharacteristicCallbacks
// {
//     int querypin = 36;
//     void onWrite(BLECharacteristic *pChar)
//     {
//         String rxValue = pChar->getValue().c_str();
//         querypin = rxValue.toInt();
//         Serial.print("Setting query pin to ");
//         Serial.println(querypin);
//     }
//     void onRead(BLECharacteristic *pChar)
//     {
//         Serial.println("battery level was read");
//         Serial.println(adc1_get_voltage(ADC1_CHANNEL_0));
//         int output;
//         float VBAT = (127.0f / 100.0f) * 3.30f * float(analogRead(querypin)) / 4096.0f; // LiPo battery
//         output = map(VBAT, 0, 4.2, 0, 100);
//         Serial.print("analog read of pin ");
//         Serial.print(querypin);
//         Serial.print(": ");
//         Serial.println(VBAT, 2);
//         Serial.print("percentage: ");
//         Serial.print(output);
//         Serial.println("%");
//         pChar->setValue(output);
//     }
// };

bool state = LOW;
void toggleBLEAdvertising()
{
    state = !state;
    digitalWrite(BT_NOTIFY_LED, state);
    if (state)
    {
        pServer->getAdvertising()->start();
        Serial.println("started advertising");
    }
    else
    {
        pServer->getAdvertising()->stop();
        Serial.println("stopped advertising");
    }
}

/*
Use this function as an example of how to call up an instance of
MagSpoofUtil to playback a card out the magnetic coil.
Dereference the card pointer, feed it by reference to the constructor,
then call MagSpoofUtil::play_card() to run the playback.
*/
void play_active_card()
{
    Serial.println("Playing active card...");
    Preferences prefs;
    prefs.begin(PREFERENCES_APP_NAME, true);

    String activecard = prefs.getString("activecard");
    Serial.println(activecard);
    card *c = new card(test_card);
    c = jsonToCard(prefs.getString(activecard.c_str()));
    Serial.println(c->tracks[0]);
    Serial.println(c->tracks[1]);
    MagSpoofUtil msu(c);
    msu.play_card();
    Serial.println("Done.");
    // card *c = new card(test_card);
    // cardToJson(c);
}

void setup()
{
    Serial.begin(9600);
    Serial.println("Starting BLE work!");

    pinMode(BT_NOTIFY_LED, OUTPUT);
    pinMode(COIL_A, OUTPUT);
    pinMode(COIL_B, OUTPUT);
    pinMode(COIL_ENABLE_PIN, OUTPUT);
    button.attachClick(play_active_card);
    button.attachDoubleClick(toggleBLEAdvertising);

    delay(500);

    // {
    //     Preferences prefs;
    //     prefs.begin(PREFERENCES_APP_NAME, false);
    //     prefs.clear();
    //     prefs.end();
    // }

    //Bluetooth stuffs
    {
        BLEDevice::init("MagSpoofBT");
        pServer = BLEDevice::createServer();
        pServer->setCallbacks(new MyServerCallbacks());

        BLEService *pMagSpoofService = pServer->createService(MAGSPOOF_SERVICE_UUID);

        // BLECharacteristic *pLEDCharacteristic = pMagSpoofService->createCharacteristic(
        //     TEST_LED_CHARACTERISTIC_UUID,
        //     BLECharacteristic::PROPERTY_WRITE_NR);
        // pLEDCharacteristic->setCallbacks(new LedTestCallbacks());

        BLECharacteristic *pMagSpoofCharacteristic = pMagSpoofService->createCharacteristic(
            MAGSPOOF_RX_CHARACTERISTIC_UUID,
            BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ);
        pMagSpoofCharacteristic->setCallbacks(new MagSpoofCallbacks());

        /*  
        *   Battery not working until we figure out how to pull correct voltage reading
        *   via a voltage divider attached to an ADC channel.
        */
        // BLECharacteristic *pBattChar = pMagSpoofService->createCharacteristic(
        //     BATTERY_TX_CHARACTERISTIC_UUID,
        //     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
        // pBattChar->addDescriptor(new BLE2902());
        // double battlevel;
        // pBattChar->setValue(battlevel);
        // pBattChar->setCallbacks(new BatteryCallbacks());

        pMagSpoofService->start();
    }

    Serial.println("Waiting a client connection to notify...");
    toggleBLEAdvertising();
}

void loop()
{
    button.tick();
    delay(100);
}