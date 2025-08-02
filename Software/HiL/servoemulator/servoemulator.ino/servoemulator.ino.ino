uint16_t deltaTime = 0;
uint16_t time1 = 0;
uint16_t lowVal = 545;// Ususally 1000us - Is set to 2000 for RC range calibarion procedure
uint16_t highVal = 2370;// Usually 2000us - Is set to 0 for RC range calibration procedure

void setup(void){
  pinMode(3,INPUT_PULLUP);    					//Hardware interrupt input on D3 with pull-up
  attachInterrupt(3, interruptRC, CHANGE);    	//Attach interrupt to the pin and react on both rising and falling edges
  Serial.begin(115200);
}

void loop(void)
{
  int16_t servoPos = map(deltaTime, lowVal, highVal, 0, 100);
  Serial.println(servoPos);
}

void interruptRC(void)
{
  if(digitalRead(3) == HIGH)// Check if input is high
  {
    time1 = micros();
  }
  else//Now it is low - calc time
  {
    deltaTime = micros()-time1;
  }
}
