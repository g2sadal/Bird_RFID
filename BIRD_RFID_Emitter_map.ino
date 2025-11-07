#include <SPI.h>
#include <MFRC522.h>
#include <HardwareSerial.h>
#include <map>
#include <string>

// ====================== RFID Setup ======================
#define RST_PIN 9
#define SS_PIN 10

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance
String prevUID = "";

// Struct to hold data for each UID
struct TagData {
  String uid;
  int count = 0;
  std::map<int, unsigned long> durations;  // visit -> duration in ms
};

// Tag and UID map setup
const int maxTags = 2;
TagData tags[maxTags];
std::map<String, int> uidIndex;  // UID -> tag index map

// ====================== XBee Configuration ======================
#define XBEE_BAUD 9600
HardwareSerial XBeeSerial(1);  // UART1 for ESP32 (GPIO16 RX, GPIO17 TX by default)

int this_node_ID = 4;  // Unique node ID
int hub_node_ID = 1;

bool started = false;
bool ended = false;
char incomingByte;
char msg[200];
byte index;

int query_minutes = 5;
unsigned long query_time = 60000 * query_minutes;
unsigned long prevTime = 0;
unsigned long currTime = 0;
unsigned long thresh = 1000;

// Function declarations
void receive_XBee();
void drain_XBee();
void send_XBee_2receiver();
void initUIDIndex();
void printAllTags();

//*****************************************************************************************//
void setup() {
  Serial.begin(115200);
  XBeeSerial.begin(XBEE_BAUD, SERIAL_8N1, 16, 17); // RX=16, TX=17
  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  initUIDIndex();
  drain_XBee();

  Serial.println(F("Ready to scan"));
}

//*****************************************************************************************//
void loop() {
  if (XBeeSerial.available()) {
    receive_XBee();
    return;
  }

  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  currTime = millis();
  unsigned long diff = currTime - prevTime;
  prevTime = currTime;

  // Read UID
  char uidChar[10];
  sprintf(uidChar, "%02X%02X%02X%02X",
          mfrc522.uid.uidByte[0],
          mfrc522.uid.uidByte[1],
          mfrc522.uid.uidByte[2],
          mfrc522.uid.uidByte[3]);
  String uid = String(uidChar);
  Serial.println(uid);

  // Check UID in map
  if (uidIndex.find(uid) == uidIndex.end()) {
    Serial.print("Unknown UID detected: ");
    Serial.println(uid);
    return;
  }

  int idx = uidIndex[uid];
  TagData &tag = tags[idx];

  if (uid == prevUID && diff < thresh && tag.count > 0) {
    tag.durations[tag.count] += diff;
  } else {
    tag.count++;
    tag.durations[tag.count] = 0;
    prevUID = uid;
  }

  Serial.printf("Node: %d\n", this_node_ID);
  printAllTags();
}

// ======================================================================
// Initialization
// ======================================================================
void initUIDIndex() {
  tags[0].uid = "01E83202";
  tags[1].uid = "D36138DA";

  uidIndex["01E83202"] = 0;
  uidIndex["D36138DA"] = 1;
}

// ======================================================================
// Printing Helper
// ======================================================================
void printAllTags() {
  Serial.println("--- Current Tags Data ---");
  for (int i = 0; i < maxTags; i++) {
    Serial.printf("UID: %s | Count: %d | Durations (ms): ",
                  tags[i].uid.c_str(), tags[i].count);

    for (auto &p : tags[i].durations) {
      Serial.printf("[%d]=%lu ", p.first, p.second);
    }
    Serial.println();
  }
  Serial.println("-------------------------");
}

// ======================================================================
// XBee Functions
// ======================================================================
void receive_XBee() {
  delay(200);
  started = false;
  ended = false;
  index = 0;
  msg[index] = '\0';

  while (XBeeSerial.available() > 0) {
    incomingByte = XBeeSerial.read();
    if (incomingByte == '<') {
      started = true;
      index = 0;
      msg[index] = '\0';
    } else if (incomingByte == '>') {
      ended = true;
      break;
    } else if (index < sizeof(msg) - 1) {
      msg[index++] = incomingByte;
      msg[index] = '\0';
    }
  }

  if (started && ended) {
    String receivedMsg(msg);
    int receiver_ID = receivedMsg.toInt();
    if (receiver_ID == this_node_ID) {
      send_XBee_2receiver();
    }

    index = 0;
    msg[index] = '\0';
    started = false;
    ended = false;
  }
}

void drain_XBee() {
  while (XBeeSerial.available() > 0) {
    XBeeSerial.read();
  }
}

void send_XBee_2receiver() {
  for (int i = 0; i < maxTags; i++) {
    const String &uid = tags[i].uid;
    bool last_id = (i == maxTags - 1);

    String times = "";
    for (auto &p : tags[i].durations) {
      int mins = (int)(p.second / 60000.0 + 0.5);
      if (mins < 0) mins = 0;
      if (mins > 99) mins = 99;
      char temp[4];
      sprintf(temp, "%02d", mins);
      times += temp;
    }

    snprintf(msg, sizeof(msg), "(%03d%s%03d%s%d)",
             hub_node_ID, uid.c_str(), tags[i].count,
             times.c_str(), last_id);

    XBeeSerial.println(msg);
    Serial.print("msg sent: ");
    Serial.println(msg);

    // Reset tag data
    tags[i].count = 0;
    tags[i].durations.clear();
  }
}
