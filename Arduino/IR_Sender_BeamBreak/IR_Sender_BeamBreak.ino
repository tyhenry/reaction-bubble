#define CODE 1200
// CODE = burst length in microseconds
// so IR LED will burst for CODE microseconds, then pause for 50 milliseconds, and repeat

int irLedPin = 1; //IR on Pin 1

//counters for micros()
unsigned long time0 = 0;
unsigned long time1 = 0;

void setup(){
  
  pinMode(irLedPin, OUTPUT);
  
  cli(); //turn off interrupts
  
  //set timer 1 register values for CTC mode, PB1 toggle output, and no prescaler
  TCCR1 = 0b10010001;
  TCNT1 = 0;//initialize Timer 1 counter value to 0
  
  //set timer1 (8-bit) compare match interrupt to 76000 Hz  (i.e. 38kHz x 2 for full square wave)
  OCR1C = 104;// = (8000000 / (76000*1)) - 1 == i.e. (8MHz / (76Khz*prescaler) - 1 [because count begins with 0]
  
  sei(); //turn interrupts back on
}

void loop(){
  
  time1 = micros();
  
  //if CODE microseconds have elapsed...
  if ((time1 - time0) >= CODE){
     
     //disable compare output mode (+ Pin 9 is now accessible)
     TCCR1 &= ~(1 << COM1A0);
     
     //make sure LED pin is off
     digitalWrite(irLedPin, LOW);
     
     //delay 50ms between bursts
     delay(50);
     
     //reset time0 to current micros()
     time0 = micros();
     
     TCNT1 = 0; //reset timer
     
     //re-enable compare output mode
     TCCR1 |= (1 << COM1A0);

  }
    
}
