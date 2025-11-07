#include <Dictionary.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>


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
int num_nodes = 3; // USER DEFINED


// ====================== XBee Configuration (Hardware Serial) ======================
#define XBEE_BAUD 9600

int Rx = 2;
int Tx = 3;
SoftwareSerial Serial1(Rx, Tx);

int this_node_ID = 1;  // Unique ID for this node

bool started = false;
bool ended = false;
char incomingByte;
char msg[100];
byte index;

int detector_ID;
bool send_data;
int num_count;
bool last_id = 0;
String uidTemp;

unsigned long query_time = 60000 * 1;  // Time Interval for querying nodes
unsigned long prev_time;
unsigned long curr_time;

void receive_XBee();
void drain_XBee();
void send_XBee(bool send_data);
// ======================================================================


//*****************************************************************************************//
void setup() {
  Serial.begin(9600);                              // Initialize serial communications with the PC
  Serial1.begin(9600);
  SPI.begin();                                     // Init SPI bus
  mfrc522.PCD_Init();                              // Init MFRC522 card
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);  // Set antenna to max gain

  initUIDIndex();  // Initialize UID indices

  // Initialize xbee
  Serial1.begin(XBEE_BAUD);
  drain_XBee();

  Serial.println(F("Ready to scan"));  //shows in serial that it is ready to read
}

void loop() {
  /*Every _DEFINED INTERVAL_ units of time:
  iterate through emitter nodes
    send message to node #N to start transmitting info
    listen for messages from node # N
    store info
  consolidate all data
  package to update home node  
  */
  // Serial.println("Waiting...");
  curr_time = millis();
  if ((curr_time - prev_time) > query_time) {
    // Serial.println("inside time loop");
    for (int i = 2; i < num_nodes+2; i++) { 
    //   Serial.print(i);
    //   Serial.println(": inside receive loop");
      if(i == 3){continue;}
      send_XBee_2emitter(i);
      receive_XBee();   
      delay(1000);
    }
    
    printAllTags();
    prev_time = millis();
  }
}

// MANUAL INPUT REQUIRED: input list of known tags being used
void initUIDIndex () {
  tags[0].uid = "01E83202";
  tags[0].d(String(0),String(0));
  tags[1].uid = "D36138DA";
  tags[1].d(String(0),String(0));
  uidIndex("01E83202", String(0));
  uidIndex("D36138DA", String(1));
}

// ======================================================================
// XBee
// ======================================================================

void receive_XBee() {
  int last_id = 0;
  while (!last_id) {
    delay(200);  // Wait for data
    started = false;
    ended = false;
    index = 0;
    msg[index] = '\0';

    while (Serial1.available() > 0) {
      incomingByte = Serial1.read();
      if (incomingByte == '(') {
        started = true;
        index = 0;
        msg[index] = '\0';
      } else if (incomingByte == ')') {
        ended = true;
        break;
      } else {
        if (index < 99) {
          msg[index++] = incomingByte;
          msg[index] = '\0';  // adds null to the end
        }
      }
    }

    if (started && ended) {
      String receivedMsg(msg);
      // Serial.println(msg);
      int receiver_ID = receivedMsg.substring(0, 3).toInt();
      if (receiver_ID == this_node_ID) {
        uidTemp = receivedMsg.substring(3, 11);
        TagData &tag = tags[uidIndex[uidTemp].toInt()];
        int num_count = receivedMsg.substring(11, 14).toInt();
        for (int i = 0; i < num_count; i++) {
          int startInd = 14 + 2 * i;
          int endInd = 14 + 2 * (i + 1);
          String time = receivedMsg.substring(startInd, endInd);
          tag.count = tag.count + 1;
          tag.d(String(tag.count), String(time));
        }
        last_id = receivedMsg.substring(num_count * 2 + 14, receivedMsg.length()).toInt();
        // Serial.print("last id: ");
        // Serial.println(last_id);
      }
    }
  }
}

void drain_XBee() {
  while (Serial1.available() > 0) {
    Serial1.read();
  }
}

void send_XBee_2emitter(int emitter_ID) {
  sprintf(msg, "<%03d>", emitter_ID);
  Serial1.println(msg);
  Serial.print("send: ");
  Serial.println(msg);
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
