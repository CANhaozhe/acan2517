//——————————————————————————————————————————————————————————————————————————————
//  ACAN2517 / ACAN Demo 
//  ACAN2517 uses hardware SPI1 and an external interrupt pin
//  This sketch runs only on a Teensy 3.5 or 3.6 (or 3.1 / 3.2 see below)
//  It uses the Teensy 3.x builtin CAN0 interface for testing intensive
//  communication with a MCP2517 CAN controller.
//  The builtin CAN0 interface and the MCP2517 controller should be connected
//  throught transceivers.
//  Note that the Tx and Rx alternate pins are used for the Teensy builtin CAN0.
//  On a teensy 3.1 / 3.2, do not use alternate pins, they are not supported
//——————————————————————————————————————————————————————————————————————————————

#include <ACAN.h>      // For the Teensy 3.x builtin CAN
#include <ACAN2517.h>  // For the external MCP2517

//——————————————————————————————————————————————————————————————————————————————
// Select CAN baud rate.
// Select a baud rate common to the builtin CAN interface and the MCP2517

static const uint32_t CAN_BIT_RATE = 1000 * 1000 ;

//——————————————————————————————————————————————————————————————————————————————
//  MCP2517 connections: adapt theses settings to your design
//  As hardware SPI is used, you should select pins that support SPI functions.
//  This sketch is designed for a Teensy 3.5/3.6, using SPI1
//  But standard Teensy 3.5/3.6 SPI1 pins are not used
//    SCK input of MCP2517 is connected to pin #32
//    SDI input of MCP2517 is connected to pin #0
//    SDO output of MCP2517 is connected to pin #1
//  CS input of MCP2517 should be connected to a digital output port
//  INT output of MCP2517 should be connected to a digital input port, with interrupt capability
//——————————————————————————————————————————————————————————————————————————————

static const byte MCP2517_SCK = 32 ; // SCK input of MCP2517 
static const byte MCP2517_SDI =  0 ; // SI input of MCP2517  
static const byte MCP2517_SDO =  1 ; // SO output of MCP2517 

static const byte MCP2517_CS  = 31 ; // CS input of MCP2517 
static const byte MCP2517_INT = 38 ; // INT output of MCP2517

//——————————————————————————————————————————————————————————————————————————————
//  MCP2517 Driver object
//——————————————————————————————————————————————————————————————————————————————

ACAN2517 can (MCP2517_CS, SPI1, MCP2517_INT) ;

//——————————————————————————————————————————————————————————————————————————————
//   SETUP
//——————————————————————————————————————————————————————————————————————————————

void setup () {
  pinMode (3, OUTPUT) ;
  pinMode (4, OUTPUT) ;
  pinMode (5, OUTPUT) ;
  pinMode (6, OUTPUT) ;
  pinMode (7, OUTPUT) ;
//--- Switch on builtin led
  pinMode (LED_BUILTIN, OUTPUT) ;
  digitalWrite (LED_BUILTIN, HIGH) ;
//--- Start serial
  Serial.begin (38400) ;
//--- Wait for serial (blink led at 10 Hz during waiting)
  while (!Serial) {
    delay (50) ;
    digitalWrite (LED_BUILTIN, !digitalRead (LED_BUILTIN)) ;
  }
//--- Define alternate pins for SPI1 (see https://www.pjrc.com/teensy/td_libs_SPI.html)
  SPI1.setMOSI (MCP2517_SDI) ;
  SPI1.setMISO (MCP2517_SDO) ;
  SPI1.setSCK (MCP2517_SCK) ;
//--- Begin SPI
  SPI1.begin () ;
//--- Configure ACAN2517
  ACAN2517Settings settings2517 (ACAN2517Settings::OSC_4MHz10xPLL, CAN_BIT_RATE) ;
  Serial.print ("MCP2517FD RAM usage: ") ;
  Serial.print (settings2517.ramUsage ()) ;
  Serial.println (" bytes") ;
  settings2517.mDriverReceiveFIFOSize = 1 ;
  const uint32_t errorCode2517 = can.begin (settings2517, [] { can.isr () ; }) ;
  if (errorCode2517 == 0) {
    Serial.println ("ACAN2517 configuration: ok") ;
  }else{
    Serial.print ("ACAN2517 configuration error 0x") ;
    Serial.println (errorCode2517, HEX) ;
  }
//--- Configure ACAN
  ACANSettings settings (CAN_BIT_RATE) ;
  settings.mUseAlternateTxPin = true ;
  settings.mUseAlternateRxPin = true ;
  const uint32_t errorCode = ACAN::can0.begin (settings) ;
  if (errorCode == 0) {
    Serial.println ("ACAN configuration: ok") ;
    Serial.print ("Actual bit rate: ") ;
    Serial.print (settings.actualBitRate ()) ;
    Serial.println (" bit/s") ;
  }else{
    Serial.print ("ACAN configuration error 0x") ;
    Serial.println (errorCode, HEX) ;
  }
}

//——————————————————————————————————————————————————————————————————————————————

static unsigned gBlinkLedDate = 0 ;
static unsigned gReceivedFrameCount = 0 ;
static unsigned gReceivedFrameCount2517 = 0 ;
static unsigned gSentFrameCount = 0 ;
static unsigned gSentFrameCount2517 = 0 ;

static const unsigned MESSAGE_COUNT = 10 * 1000 ;

//——————————————————————————————————————————————————————————————————————————————
// A CAN network requires that stations do not send frames with the same identifier.
// So: 
//   - MCP2517 sends frame with even identifier values;
//   - builtin CAN0 sends frame with odd identifier values;

void loop () {
//--- Blink led
  if (gBlinkLedDate < millis ()) {
    gBlinkLedDate += 1000 ;
    digitalWrite (LED_BUILTIN, !digitalRead (LED_BUILTIN)) ;
    Serial.print (gSentFrameCount) ;
    Serial.print (" ") ;
    Serial.print (gReceivedFrameCount) ;
    Serial.print (" ") ;
    Serial.print (gSentFrameCount2517) ;
    Serial.print (" ") ;
    Serial.println (gReceivedFrameCount2517) ;
  }
  CANMessage frame ;
//--- Send messages via the MCP2517
  if (gSentFrameCount2517 < MESSAGE_COUNT) {
  //--- Make an even identifier for MCP2517
    frame.id = millis () & 0x7FE ;
    frame.len = 8 ;
    for (uint8_t i=0 ; i<8 ; i++) {
      frame.data [i] = i ;
    }
  //--- Send frame via MCP2517
    const bool ok = can.tryToSend (frame) ;
    if (ok) {
      gSentFrameCount2517 += 1 ;
    }
  }
//--- Send messages via the builtin CAN0
  if (gSentFrameCount < MESSAGE_COUNT) {
  //--- Make an odd identifier for builtin CAN0
    frame.id = (millis () & 0x7FE) | 1 ;
    frame.len = 8 ;
  //--- Send frame via builtin CAN0
    const bool ok = ACAN::can0.tryToSend (frame) ;
    if (ok) {
      gSentFrameCount += 1 ;
    }
  }
//--- Receive frame from MCP2517
  if (can.receive (frame)) {
//    can.receive (frame) ;
    gReceivedFrameCount2517 ++ ;
  }
//--- Receive frame via builtin CAN0
  if (ACAN::can0.available ()) {
    ACAN::can0.receive (frame) ;
    bool ok = frame.len == 8 ;
    for (uint8_t i=0 ; (i<8) && ok ; i++) {
      ok = frame.data [i] == i ;
    }
    if (!ok) {
      Serial.println ("RECEIVED DATA ERROR") ;
    }
    gReceivedFrameCount ++ ;
  }
}

//——————————————————————————————————————————————————————————————————————————————
