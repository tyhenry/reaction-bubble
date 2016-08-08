/*
* BeamBreak and IR Code detector for LoVid Reaction Bubble Project
* by Tyler Henry
* 
* jump from pin 3 to +v sets to IR code detector mode
* otherwise, will only report beam breaks over radio
* 
* Designed to work with Atmega328 (Adafruit Trinket Pro or Uno)
*
* Input Capture function
* uses timer hardware to measure pulses on pin 8 on 168/328
* 
* Input capture timer code based on Input Capture code found in Arduino Cookbook
*/

#include <RF24.h>
#include <RF24Network.h>
#include <SPI.h>


/* IR CODES GO HERE */
/*--------------------------------------------------------*/

//value of IR CODEs to be checked for (these are length of IR burst in microseconds)
const int codeArraySize = 5;
const int codes[codeArraySize] = {1000, 1200, 1400, 1600, 1800};

/*--------------------------------------------------------*/

/* RF24 NODE ADDRESS GOES HERE */
/*--------------------------------------------------------*/

// node address of this IR beam for RF24Network:
const uint16_t rfNode = 01;

// IR beams are addressed: 01, 02, 03, 04, 05
// Master receiver Arduino Uno is base node: 00
/*--------------------------------------------------------*/

const int ledBBPin = 3; // LED Beam Break Pin: ON when beam break detected (OFF when receiving IR signal)

const int inputCapturePin = 8; // Input Capture pin fixed to internal Timer - MUST BE 8 on ATMega168/328
const int prescale = 64; // prescale factor 64 needed to count to 62 ms for beam break
const byte prescaleBits = B011; // see Table 18-1 or data sheet

// calculate time per counter tick in ns
const long precision = (1000000/(F_CPU/1000)) * prescale ;
const long compareMatch = (F_CPU/(16 * prescale)) - 1; //compareMatch interrupt value for beam break detection (16Hz)

volatile unsigned long burstCount; // CPU tick counter for current burst
volatile unsigned long pauseCount; // CPU tick counter for current pause

unsigned long burstCounts[3]; // storage array of burst samples for checking code matches
unsigned int burstIndex = 0; // index to the bursCounts storage array
unsigned long burstTimes[3]; // storage array for conversion of burstCounts to time in microseconds

volatile boolean bursted = false;
volatile boolean paused = false;

volatile boolean beamBroken = false;
volatile boolean beamBrokenNew = true;

boolean codeDetectMode = false;

/* RF24Network setup */
/*--------------------------------------------------------*/
RF24 radio(9,10); // RF module on pins 9,10
RF24Network network(radio); // create the RF24Network object
/*--------------------------------------------------------*/


/* ICR interrupt capture vector */
/* runs when pin 8 detects an input change (rising or falling trigger depends on ICES1 value) */
ISR(TIMER1_CAPT_vect){
  TCNT1 = 0; // reset counter
  
  if(bitRead(TCCR1B, ICES1)){
      // rising edge was detected on pin 8 - meaning IR signal ended (NO IR signal now)
      burstCount = ICR1; // save ICR timer value as burst length
      beamBrokenNew = true;
      bursted = true; // burst complete
  } else {
      // falling edge was detected on pin 8 - meaning IR signal now present
      beamBroken = false;
      pauseCount = ICR1; // save ICR timer value as pause length
      paused = true; // pause complete
  }
   
  TCCR1B ^= _BV(ICES1); // toggle bit to trigger on the other edge
}

//beam break interrupt vector
ISR(TIMER1_COMPA_vect){ //timer1 interrupt at 16Hz (62.5 milliseconds)
    beamBroken = true;
}


void setup() {
  
  SPI.begin();
  radio.begin(); // start the RF24 module
  network.begin(90, rfNode); // start the RF24 network on channel 90 with rfNode address
  
  pinMode(inputCapturePin, INPUT); // ICP pin (digital pin 8 on Arduino) as input
  pinMode(ledBBPin, OUTPUT); // LED on when beam break detected
  
//  pinMode(3, INPUT); // jumper on 3 to +v to set to code detector mode
//  if (digitalRead(3) == 1){
//    codeDetectMode = true;
//  }
//  
  cli(); //stop interrupts temporarily
  
  //input capture timer:
    TCCR1A = 0; // Normal counting mode
    TCCR1B = 0;
    TCCR1B = prescaleBits ; // set prescale bits
    TCNT1 = 0; //reset counter
    TCCR1B |= _BV(ICES1); // enable input capture: counter value written to ICR1 on rising edge
    bitSet(TIMSK1,ICIE1); // enable input capture interrupt for timer 1: runs ISR
    //TIMSK1 |= _BV(TOIE1); // Add this line to enable overflow interrupt
  
  //beam break interrupt:
    TCCR1B |= (1 << WGM12); //Timer 1 CTC mode: TCNT1 clears when OCR1A reached
    OCR1A = compareMatch; //set compare match interrupt for beam break to 62ms (~16Hz)
    TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
    
  sei(); //turn interrupts back on
  
}


void loop(){

  network.update();
  
  if (bursted && codeDetectMode){ // run if a burst was detected
  
    burstCounts[burstIndex] = burstCount; // save the current burst count in the array 
    
    if(burstIndex == 2){ // if array is full
            
      // calculate time (microseconds) values of burstCounts array
      for(int i = 0 ; i < 3 ; i++){
        burstTimes[i] = calcTime(burstCounts[i]);
      }
      
      // bubble sort (i.e. from small to large) the time value array for easy comparisons
      bubbleSort(burstTimes, 3);
      
      //check for close matches in array
      for(int i=0; i<2; i++){
        
        unsigned long dif = burstTimes[i+1] - burstTimes[i];
        if (dif < 100) { //check to see if the two times are close (100 is fairly arbitrary, depends on sensor accuracy)
                  
          unsigned long avg = (burstTimes[i+1] + burstTimes[i]) / 2; //use average to check for matches to CODE values

          for (int j=0; j<3; j++){ // check the CODE value array for matches
            if (avg > codes[j]-50 && avg < codes[j]+50){ //if the avg of the two values is within 100 micros of a CODE

              // SEND MATCHED CODE VALUE TO RF
              unsigned long code = codes[j];
              sendRF(code); // CODE value received
              break;
            }
          }
        }
      }
      burstIndex = 0;
    }
    else {
      burstIndex++;
    }
    
    bursted = false; // reset bursted boolean
  }

  
  if (beamBroken){    
    if (beamBrokenNew){
      sendRF(62500); // send beam break value of 62500
      beamBrokenNew = false;
    }
    digitalWrite(ledBBPin, HIGH);
    burstIndex = 0;
  }
  else {
    digitalWrite(ledBBPin, LOW);
  }
    
  
}

// converts a timer count to microseconds
long calcTime(unsigned long count){

  long durationNanos = count * precision; // pulse duration in nanoseconds
  
  if(durationNanos > 0){
    long durationMicros = durationNanos / 1000;
    return (durationMicros); // duration in microseconds
  } else {
    return 0;
  }
}


/* lifted from http://www.hackshed.co.uk/arduino-sorting-array-integers-with-a-bubble-sort-algorithm/ */
void bubbleSort(unsigned long a[], int aSize) {
    for(int i=0; i<(aSize-1); i++) {
        for(int j=0; j<(aSize-(i+1)); j++) {
                if(a[j] > a[j+1]) {
                    int t = a[j];
                    a[j] = a[j+1];
                    a[j+1] = t;
                }
        }
    }
}

/* RF24 Network */
struct payload_t { // structure of message payload
  unsigned long rfCode;
};

// sends RF packet to base node (address: 00)
void sendRF(unsigned long _rfCode){

  payload_t payload = {_rfCode};
  RF24NetworkHeader header(00); // node to send to (base node 00)
  network.write(header,&payload,sizeof(payload));
  
}

