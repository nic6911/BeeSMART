uint16_t deltaTime = 0;
uint16_t time1 = 0;
uint16_t lowVal = 545;// Ususally 1000us - Is set to 2000 for RC range calibarion procedure
uint16_t highVal = 2370;// Usually 2000us - Is set to 0 for RC range calibration procedure
const int dataPin = 3;  // Data pin connected to HX711 module
const int clockPin = 1; // Clock pin connected to HX711 module
int weight = 0;
int temp;
int indx[2];
int ii = 0;
String array;

void setup(void){
  pinMode(5,INPUT_PULLUP);    					//Hardware interrupt input on D3 with pull-up
  attachInterrupt(5, interruptRC, CHANGE);    	//Attach interrupt to the pin and react on both rising and falling edges
  pinMode(clockPin, INPUT);
  pinMode(dataPin, OUTPUT);
  digitalWrite(clockPin, HIGH); // Clock starts high
  Serial.begin(250000);
  Serial.setTimeout(1);
}

void loop() {
}

void readData() {
  // Simulate HX711 data reading process with increasing values
  /* if (Serial.available() > 0) {
    temp = Serial.parseInt();//send with no line ending via serial
    while(Serial.available())
    Serial.read();
   }*/

  while(Serial.available()){
    array = Serial.readString();
  }
  indx[0] = 0;
  for (int i = 1; i <= 1; i++) {
    indx[i] = array.indexOf(',',indx[i-1]+1);
  }
  temp = array.substring(indx[0],indx[1]).toInt();
  if(temp>=0 && temp<30000){
    weight = temp;
  }
  sendHX711Data(-250*weight);//0 to 30000g is acceptable
}

void sendServoData(){
  if(deltaTime >= lowVal*0.9 && deltaTime <= highVal*1.1){
    int16_t servoPos = map(deltaTime, lowVal, highVal, 0, 100);
    Serial.println(servoPos);
  }
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

void interruptRC(void)
{
  if(digitalRead(5) == HIGH)// Check if input is high
  {
    time1 = micros();
  }
  else//Now it is low - calc time
  {
    deltaTime = micros()-time1;
    if(ii==2){
      sendServoData();
    }
    else if(ii==5){
      readData();
      ii=1;
    }
    ii++;
  }
}
