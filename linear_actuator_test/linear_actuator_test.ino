const int forwards = 7;
const int backwards = 6;//assign relay INx pin to arduino pin
void setup() {
pinMode(forwards, OUTPUT);//set relay as an output
pinMode(backwards, OUTPUT);//set relay as an output
}
void loop() {
 digitalWrite(forwards, LOW);
 digitalWrite(backwards, HIGH);//Activate the relay one direction, they must be different to move the motor
 delay(500); // wait 2 seconds
 digitalWrite(forwards, HIGH);
 digitalWrite(backwards, HIGH);//Deactivate both relays to brake the motor
 delay(500);// wait 2 seconds
 digitalWrite(forwards, HIGH);
 digitalWrite(backwards, LOW);//Activate the relay the other direction, they must be different to move the motor
 delay(500);// wait 2 seconds
 digitalWrite(forwards, HIGH);
 digitalWrite(backwards, HIGH);//Deactivate both relays to brake the motor
 delay(500);// wait 2 seconds
}