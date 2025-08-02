const int dataPin = 0;  // Data pin connected to HX711 module
const int clockPin = 4; // Clock pin connected to HX711 module
int weight = 0;
void setup() {
  pinMode(clockPin, INPUT);
  pinMode(dataPin, OUTPUT);
  digitalWrite(clockPin, HIGH); // Clock starts high
  Serial.begin(115200);
  Serial.setTimeout(2);
}

void loop() {
  // Simulate HX711 data reading process with increasing values
   if (Serial.available() > 0) {
    weight = Serial.parseInt();//send with no line ending via serial
   }
  // Add some delay to simulate the time taken for HX711 to read data
  sendHX711Data(-250*weight);//0 to 30000g is acceptable
  delay(10); // Adjust delay time as needed
}

void sendHX711Data(long data) {
  // Send data bit by bit
  digitalWrite(dataPin, LOW);
  for (int i = 23; i >= 0; i--) {
    while(digitalRead(clockPin)==LOW);
    digitalWrite(dataPin, (data >> i) & 0x01); // Shift data and send MSB first
    while(digitalRead(clockPin)==HIGH);
  }
  while(digitalRead(clockPin)==LOW);
  digitalWrite(dataPin, HIGH);
}