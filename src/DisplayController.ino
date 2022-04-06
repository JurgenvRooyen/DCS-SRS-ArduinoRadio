#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#define STX '\x02'
#define ETX '\x03'
#define ACK '\x06' // Received transmission
#define NAK '\x15'  // Malformed recognised message
#define CAN '\x18'  // Unrecognised message type

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x);

/*
    MESSAGE STRUCTURE
    
    // HEADER //      - 5 bytes   
    STX markler     - 1 byte 
    Message Type    - 1 byte
    Radio Number    - 2 bytes
    Nonfixed size   - 1 byte
    
    // NON-FIXED //   - x bytes
    Value           - x bytes

    // FIXED END //   - 2 bytes
    Checksum        - 1 byte      
    ETX marker      - 1 byte
    ============================
      MESSAGE TYPES
    00    - Session start
    01    - Sync update
    02    - Freq update
    03    - Volume update
    04    - Select update
    FF    - Session stop    
*/

const char fixedHeaderSize = 5;
char messageHeader[fixedHeaderSize];

LiquidCrystal_I2C lcd(0x27 , 16, 2);

char radioFrequencies[11][6];
char radioVolume[11][1];

unsigned long lastComTime;
bool connected;
bool inSession;

void sendReply(char message = ACK)
{
  if(Serial.availableForWrite() > 2)
  {
    char finalMessage[2] = {message, ETX};
    Serial.write(finalMessage);
  }
}

bool verifyChecksum(char messageHeader[], char messagePayload[])
{
  // Extremely basic checksum
  char messageSum;
  int payloadSize = 2;
  uint8_t messageCheckSum = messagePayload[payloadSize-1];

  for(int i = 0; i < fixedHeaderSize; i++)
  {
    messageSum += messageHeader[i];
  }

  for(int i = 0; i < payloadSize-2; i++)
  {
    if(messagePayload[i] == ETX)
    {
      break;
    }

    messageSum+= messagePayload[i];
  }

  return (255 - messageSum) == messageCheckSum;
}

void UpdateSettings(int radioNum, char message[])
{
  for(int i = 0; i < 6; i++)
  {
    radioFrequencies[radioNum][i] = message[i];
  }
}

void SetDisplayRadio(int radio)
{
  lcd.clear();

  switch(radio)
  {
    case 0:
      lcd.print("Intercom");
      break;
    case 10:
      lcd.print("Radio 10");
      lcd.setCursor(0, 1);
      lcd.print(SetDisplayFrequency(radio));
      break;
    default:
      lcd.print("Radio 0");
      lcd.write(messageHeader[3]);
      lcd.setCursor(0, 1);
      lcd.print(SetDisplayFrequency(radio));
      break;
  }
}

String SetDisplayFrequency(int radio)
{
  String final;
  const String& ref = final;
  
  for(int i = 0; i < 6; i++)
  {
    if(i == 3)
    {
      final += '.';
    }
    final += radioFrequencies[radio][i];
  }
  final += "Mhz";

  return ref;
}

void sessionStart(char message[])
{
  inSession = true;
  for(int i = 0; i < 11; i++)
  {
    char radioSettings[6];
    int radioOffset = i*6;

    for(int j = 0; j < 6; j++)
    {
      radioSettings[j] = message[j+radioOffset];
    }

    UpdateSettings(i, radioSettings);
  }
}

void sessionStop()
{
  inSession = false;
  displayConnected();
}

void processIncomingSerial()
{
  while(Serial.available() > 0)
  {
    Serial.readBytes(messageHeader, fixedHeaderSize);
    uint8_t radioNum = (messageHeader[2]-48) * 10 + messageHeader[3]-48;
    uint8_t messageType = messageHeader[0];
    uint8_t messagePayloadLength = messageHeader[4];

    char messagePayload[messagePayloadLength+2];
    Serial.readBytesUntil(ETX, messagePayload, messagePayloadLength+2);

    bool checkPass = verifyChecksum(messageHeader, messagePayload);

    if(checkPass && 0 <= radioNum && radioNum < 11)
    {
      sendReply(NAK);
    }
    
    switch(messageType)
    {
      case 0:
        lastComTime = millis();
        sessionStart(messagePayload);
        break;
      case 1:
        lastComTime = millis();
        break;
      case 2:
        lastComTime = millis();
        UpdateSettings(radioNum, messagePayload);
        sendReply();
        break;
      case 3:
        lastComTime = millis();
        break;
      case 4:
        lastComTime = millis();
        SetDisplayRadio(radioNum);
        sendReply();
        break;
      case 255:
        lastComTime = millis();
        sessionStop();
        break;
      default:
        lastComTime = millis();
        sendReply(CAN);
        break;
    }
  }
}

void displayNotConnected()
{
  lcd.clear();
  lcd.backlight();
  lcd.print("Not Connected");
}

void displayConnected()
{
  lcd.clear();
  lcd.backlight();
  lcd.print("Waiting for");
  lcd.setCursor(0, 1);
  lcd.print("session start");
}

void checkConnected()
{
  if(lastComTime - millis() > 10000)
  {
    inSession = false;
    displayNotConnected();
    return;
  }
  else if(connected == false)
  {
    connected = true;
    displayConnected();
  }
}

void setup()
{
  lcd.begin();        
  displayNotConnected();
}

void loop()
{ 
  processIncomingSerial();
  delay(50);
}