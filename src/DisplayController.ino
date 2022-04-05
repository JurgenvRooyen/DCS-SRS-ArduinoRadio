#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#define STX '\x02'
#define ETX '\x03'
#define ACK '\x06' // Received transmission
#define NAK '\x15'  // Malformed recognised message
#define CAN '\x18'  // Unrecognised message type

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
    00    - Sync update
    01    - Freq update
    02    - Volume update
    03    - Select update
    FF    - Session close    
*/

const char fixedHeaderSize = 5; //length plus checksum
char messageHeader[fixedHeaderSize];

LiquidCrystal_I2C lcd(0x27 , 16, 2);
char radioFrequencies[11][6];
char radioVolume[11][1];

unsigned long lastComTime;

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
  int payloadSize = sizeof(messagePayload)/sizeof(messagePayload[0]);
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
  for(int i = 0; i < 7; i++)
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
        break;
      case 1:
        lastComTime = millis();
        UpdateSettings(radioNum, messagePayload);
        sendReply();
        break;
      case 2:
        lastComTime = millis();
        break;
      case 3:
        lastComTime = millis();
        SetDisplayRadio(radioNum);
        sendReply();
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

void checkConnected()
{
  if(lastComTime - millis() > 10000)
  {
    displayNotConnected();
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