#include <Dictionary.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>

// ======================RFID Setup ======================

#define RST_PIN 9  // Configurable, see typical pin layout above
#define SS_PIN 10  // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
String prevUID = "";

// Struct to hold data for each UID
struct TagData {
  String uid;
  int count = 0;
  Dictionary &d = *(new Dictionary());  // dictionary of durations
};

const int maxTags = 2;
TagData tags[maxTags];
Dictionary &uidIndex = *(new Dictionary());  // dictionary of uids and corresponding array indices


// ====================== XBee Configuration (Hardware Serial) ======================
#define XBEE_BAUD 9600

int Rx = 2;
int Tx = 3;
SoftwareSerial Serial1(Rx, Tx);

int this_node_ID = 4;  // MANUAL INPUT REQUIRED: Unique ID for this node
int hub_node_ID = 1;

bool started = false;
bool ended = false;
char incomingByte;
char msg[500];
byte index;


int query_minutes = 5;
unsigned long query_time = 60000 * query_minutes;  // Time Interval for querying nodes
unsigned long prevTime = 0;
unsigned long currTime = 0;
unsigned long thresh = 1000;

void receive_XBee();
void drain_XBee();
void send_XBee();
// ======================================================================


//*****************************************************************************************//
void setup() {
  //Serial.begin(9600);                              // Initialize serial communications with the PC
  Serial1.begin(9600);
  SPI.begin();                                     // Init SPI bus
  mfrc522.PCD_Init();                              // Init MFRC522 card
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);  // Set antenna to max gain

  initUIDIndex();  // Initialize UID indices

  // Initialize xbee
  Serial.begin(XBEE_BAUD);
  drain_XBee();

  Serial.println(F("Ready to scan"));  //shows in serial that it is ready to read
}

void loop() {
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  // Serial.println(Serial1.available());
  if (Serial1.available()) {
    Serial.println("Serial available");
    receive_XBee();
    return;
  }


  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  // Difference in time between readings
  currTime = millis();
  unsigned long diff = currTime - prevTime;
  prevTime = currTime;


  // Read ID from Tag
  char uidChar[10];
  sprintf(uidChar, "%02X%02X%02X%02X", mfrc522.uid.uidByte[0], mfrc522.uid.uidByte[1], mfrc522.uid.uidByte[2], mfrc522.uid.uidByte[3]);
  String uid = String(uidChar);
  Serial.println(uid);

  TagData &tag = tags[uidIndex[uid].toInt()];
  if (uid == prevUID && diff < thresh && tag.count > 0) {
    String visits = String(tag.count);
    unsigned long duration = tag.d[visits].toInt() + diff;
    tag.d(visits, String(duration));
  } else {
    tag.count = tag.count + 1;
    tag.d(String(tag.count), String(0));
    prevUID = uid;
  }

  Serial.print("node: ");
  Serial.println(this_node_ID);
  printAllTags();
}
// ======================================================================
// RFID
// ======================================================================
// MANUAL INPUT REQUIRED: input list of known tags being used
void initUIDIndex() {
  tags[0].uid = "01E83202";
  tags[1].uid = "D36138DA";
  uidIndex("01E83202", String(0));
  uidIndex("D36138DA", String(1));
}

void printAllTags() {
  Serial.println("--- Current Tags Data ---");
  for (int i = 0; i < maxTags; i++) {
    Serial.print("UID: ");
    Serial.print(tags[i].uid);
    Serial.print(" | Count: ");
    Serial.print(tags[i].count);
    Serial.print(" | Duration Per Visit: ");
    Serial.print(tags[i].d.json());
    Serial.println(" seconds");
  }
  Serial.println("-------------------------");
}

// ======================================================================
// XBee
// ======================================================================

void receive_XBee() {
  delay(200);  // Wait for data
  started = false;
  ended = false;
  index = 0;
  msg[index] = '\0';

  // Store incoming message
  while (Serial1.available() > 0) {
    incomingByte = Serial1.read();
    if (incomingByte == '<') {
      started = true;
      index = 0;
      msg[index] = '\0';
    } else if (incomingByte == '>') {

      ended = true;
      break;
    } else {
      if (index < 99) {
        msg[index++] = incomingByte;
        msg[index] = '\0';
      }
    }
  }
  // If sent ID matches local ID, start sending info
  if (started && ended) {
    String receivedMsg(msg);
    int receiver_ID = receivedMsg.toInt();
    Serial.println(receiver_ID);
    if (receiver_ID == this_node_ID) {
      // Serial.println("correct node");
      send_XBee_2receiver();
    }

    index = 0;
    msg[index] = '\0';
    started = false;
    ended = false;
  }
}

void drain_XBee() {
  while (Serial1.available() > 0) {
    Serial1.read();
  }
}

void send_XBee_2receiver() {
  /*Message structure: <NODE_ID, UID, count, num seconds * num count, last message, end_message>
    this_node_id = 3 int
    Receiver_NODE_ID = 3 int
    UID = 6 char
    count = 3 int
    num mins = 2 int
    last_id = 0 or 1 
    end_message = 0 or 1
  */

  // iterate through tags
  for (int i = 0; i < maxTags; i++) {
    String uid = tags[i].uid;
    char uidTemp[9] = "";
    uid.toCharArray(uidTemp, 9);
    int num_count = tags[i].count;
    int count_len = num_count * 2 + 1;
    bool last_id;

    // If no visits recorded, generate empty message
    if (num_count < 1) {
      last_id = (i == (maxTags - 1));
      // Serial.print("last id: ");
      // Serial.println(last_id);
      sprintf(msg, "(%03d%8s%03d%1d)", hub_node_ID, uidTemp, num_count, last_id);
      // Serial.println(msg);
    } 
    else {
      char times[count_len] = "";
      char temp[2] = "";
      last_id = (i == (maxTags - 1));

      // Create string portion for timing of each visit
      for (int j = 0; j < num_count; j++) {
        float dur = tags[i].d[j].toInt();
        int mins = dur / 60000.0 + 0.5;
        sprintf(temp, "%02d", mins);
        sprintf(times, "%s%s", times, temp);
      }
      // Serial.print("last id: ");
      // Serial.println(last_id);
      sprintf(msg, "(%03d%8s%03d%s%1d)", hub_node_ID, uidTemp, num_count, times, last_id);
    }

    Serial1.println(msg);
    Serial.print("msg sent: ");
    Serial.println(msg);

    tags[i].count = 0;
    delete &tags[i].d;
    tags[i].d = *(new Dictionary());
  }
}
