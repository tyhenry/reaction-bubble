/*
* Master RF24Network Serial Printer for LoVid Reaction Bubble Project
* by Tyler Henry
* 
* Designed to work with Atmega328 (Uno)
*
* uses (TMRh20) RF24Network + RF24 libraries to receive messages over RF24Network
* prints payload (unsigned long) and transmitter node to serial
* 
*/

#include <RF24.h>
#include <RF24Network.h>
#include <SPI.h>

/* RF24Network setup */
/*--------------------------------------------------------*/
// node address for RF24Network:
const uint16_t rfNode = 00; // Master receiver Arduino Uno is base node: 00
// IR beams are addressed: 01, 02, 03, 04, 05

RF24 radio(9,10); // RF module on pins 9,10
RF24Network network(radio); // create the RF24Network object

struct payload_t { // Structure of our payload
  unsigned long rfCode;
};
/*--------------------------------------------------------*/


void setup() {
  SPI.begin();
  radio.begin(); // start the RF24 module
  network.begin(90, rfNode); // start the RF24 network on channel 90 with rfNode address

  Serial.begin(9600);
  Serial.println("ready to receive and print network messages\n");
}

void loop() {

  network.update(); // check for packets on network
  
  while ( network.available() ) { // anything on the network for us?
    
    RF24NetworkHeader header;   // if so, grab it and print to serial
    payload_t payload;
    network.read(header,&payload,sizeof(payload));
    Serial.print(payload.rfCode);
    Serial.print(" from node ");
    Serial.print(header.from_node);
    Serial.print('\n');
  }
}


