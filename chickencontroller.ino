#define startIR digitalWrite(txPinIRFront, HIGH);
#define stopIR digitalWrite(txPinIRFront, LOW);
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

#include <avr/eeprom.h>
#include <LiquidCrystal.h>

boolean testrun = true;

const int rxPinIRFront = A7;
const int txPinIRFront = 3;
const int rxPinIRBack = A6;
const int txPinIRBack = 2;
const int VAL_DELAY = 25;
const int VAL_TRIGGER_FRONT = -500;
const int VAL_TRIGGER_BACK = 500;
int clockArray[24][60];
int hours = 23;
int minutes = 30;
boolean timeSet = false;
int setHours = 0;
int setMinutes = 0;

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
//LCD Variables
int lcd_key     = 0;
int adc_key_in  = 0;
int cursorposition = 0;

//IR Light Values
int VAL_RECV_OFFSET_FRONT = 0;
int VAL_RECV_OFFSET_BACK = 0;
int VAL_RECV_HIGH_FRONT = 0;
int VAL_RECV_HIGH_BACK = 0;
int VAL_DIFF_FRONT = 0;
int VAL_DIFF_BACK = 0;
int trigCountFront = 0;
int trigCountBack = 0;
int permittedFault = 2;
boolean frontTriggered = false;
boolean backTriggered = false;

// Data output mode:
// R - raw data
// T - trigger events
char dataOutput = 'T';

void setup()
{
  Serial.begin(9600);
  lcd.begin(16, 2);              // start the library
  lcd.setCursor(0,0);
  initClockArray();
  correctingInitialClockValues();
  setClockValues();
  Serial.println("Starting Setup");
  pinMode(rxPinIRFront, INPUT);
  pinMode(txPinIRFront, OUTPUT);
  pinMode(rxPinIRBack, INPUT);
  pinMode(txPinIRBack, OUTPUT);
  Serial.println("Setup complete");
}

void loop() {

  if(timeSet){
    //FILL WHEN CLOCK MODULE IS IMPLEMENTED
    //IF TIME IS SET CHECK IF TIME HAS REACHED TARGET TIME
  }
  /* FRONT/BACK BARRIER RX/TX START*/
  //Creating Offset Value
  digitalWrite(txPinIRFront, LOW);
  digitalWrite(txPinIRBack, LOW);
  delay(VAL_DELAY);
  VAL_RECV_OFFSET_FRONT = analogRead(rxPinIRFront);
  VAL_RECV_OFFSET_FRONT = analogRead(rxPinIRBack);
  if( dataOutput == 'R'){  Serial.println(VAL_RECV_OFFSET_FRONT); Serial.println(VAL_RECV_OFFSET_BACK);}


  //Reading High Value
  digitalWrite(txPinIRFront, HIGH);
  digitalWrite(txPinIRBack, HIGH);
  delay(VAL_DELAY);
  VAL_RECV_HIGH_FRONT = analogRead(rxPinIRFront);
  VAL_RECV_HIGH_BACK = analogRead(rxPinIRBack);
  if( dataOutput == 'R'){  Serial.println(VAL_RECV_HIGH_FRONT); Serial.println(VAL_RECV_HIGH_BACK);}

  //Calculate Value Difference
  VAL_DIFF_FRONT = VAL_RECV_HIGH_FRONT - VAL_RECV_OFFSET_FRONT;
  VAL_DIFF_BACK = VAL_RECV_HIGH_BACK - VAL_RECV_OFFSET_BACK;
  if( dataOutput == 'R'){  Serial.println("Diff: "); }
  //Serial.println(VAL_DIFF_FRONT);
  //Serial.println(VAL_DIFF_BACK);
  /* FRONT BARRIER RX/TX END */

  /* TRIGGER ACTION TREE */
  if( dataOutput == 'T')
  { 
    /* FRONT BARRIER START*/
    //Front Barrier broken
    if( VAL_DIFF_FRONT >= VAL_TRIGGER_FRONT)
    { 
      //Increment amount of loops Barrier is broken
      trigCountFront++;
      //Stop trigCountFront from overflowing max int val
      if(trigCountFront > 50000){
        trigCountFront = permittedFault + 1;
      }
      //Set frontTriggered if Barrier broken too long
      if(trigCountFront > permittedFault)
      {
        if(frontTriggered == false)
        {
          Serial.println("Object in Front Barrier");
        }
        //Serial.println(trigCountFront);
        frontTriggered = true;
      }
    }
    //Front Barrier not broken
    else if( VAL_DIFF_FRONT < VAL_TRIGGER_FRONT)
    {
      //Reset all triggerrelated Variables if needed
      trigCountFront = 0;
      if( frontTriggered == true)
      {
        Serial.println("Object left Front Barrier");
        frontTriggered = false;
      }
    }
    /* FRONT BARRIER END*/

    /* BACK BARRIER START*/
    //Back Barrier broken
    if( VAL_DIFF_BACK >= VAL_TRIGGER_BACK)
    { 
      //Increment amount of loops Barrier is broken
      trigCountBack++;
      //Stop trigCountBack from overflowing max int val
      if(trigCountBack > 50000){
        trigCountBack = permittedFault + 1;
      }
      //Set backTriggered if Barrier broken too long
      if(trigCountBack > permittedFault)
      {
        if(backTriggered == false)
        {
          Serial.println("Object in Back Barrier");
        }
        //Serial.println(trigCountBack);
        backTriggered = true;
      }
    }
    //Back Barrier not broken
    else if( VAL_DIFF_BACK < VAL_TRIGGER_BACK)
    {
      //Reset all triggerrelated Variables if needed
      trigCountBack = 0;
      if( backTriggered == true)
      {
        Serial.println("Object left Back Barrier");
        backTriggered = false;
      }
    }
    /* BACK BARRIER END*/
    
  lcd.setCursor(0,0);
  //lcd.print(millis()/1000);      // display seconds elapsed since power-up
  //lcd.print(trigCountFront);
  if(testrun){
    if (frontTriggered) {lcd.print("Barrier Broken");}
    if (!frontTriggered) {lcd.print("Barrier Up      ");}
  }
  else{
    lcd.print("Set Closing Time:");
  }
  lcd.setCursor(7,1);            // move to the begining of the second line
  lcd_key = read_LCD_buttons();  // read the buttons
  keytrigger();
  }
}

// read the buttons
int read_LCD_buttons()
{
 adc_key_in = analogRead(0);      // read the value from the sensor 
 // add approx 50 to those values
 //Serial.println(adc_key_in);
 if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
 //V1.1 threshold
 /*
 if (adc_key_in < 50)   return btnRIGHT;  
 if (adc_key_in < 250)  return btnUP; 
 if (adc_key_in < 450)  return btnDOWN; 
 if (adc_key_in < 650)  return btnLEFT;  
 if (adc_key_in < 850)  return btnSELECT;  
  */

 //Manually read triggervalues for buttons, then added 50
 if (adc_key_in < 75)   return btnRIGHT;  
 if (adc_key_in < 160)  return btnUP; 
 if (adc_key_in < 310)  return btnDOWN; 
 if (adc_key_in < 450)  return btnLEFT; 
 if (adc_key_in < 670)  return btnSELECT;   
 
 return btnNONE;  // default value
}

void keytrigger()
{
 lcd.setCursor(0,1);
 if(cursorposition < 0) {cursorposition = 0;}
 if(cursorposition >= 4) {cursorposition = 4;}
 switch (lcd_key)               // depending on which button was pushed, we perform an action
 {
   case btnRIGHT:
     {
     //TODO change cursor to right position unless already to the rightmost position
     lcd.print("Cursor to RIGHT");
     lcd.setCursor(cursorposition,1);
     cursorposition++; 
     if(cursorposition >= 4) {cursorposition = 4;}
     lcd.setCursor(cursorposition,1);
     break;
     }
   case btnLEFT:
     {
      //TODO change cursor to left position unless already to the leftmost position
     lcd.print("Cursor to LEFT");
     lcd.setCursor(cursorposition,1);
     cursorposition--; 
     if(cursorposition < 0) {cursorposition = 0;}
     lcd.setCursor(cursorposition,1);
     break;
     }
   case btnUP:
     {
      //TODO Increment clock value at selected position
     lcd.setCursor(cursorposition,1);
     if (cursorposition == 0 || cursorposition == 1)
     {
      hours++;
     }
     if (cursorposition == 3 || cursorposition == 4)
     {
      minutes++;
     }
     setClockValues();
     break;
     }
   case btnDOWN:
     {
      //TODO Decrement clock value at selected position
     lcd.setCursor(cursorposition,1);
     if (cursorposition == 0 || cursorposition == 1)
     {
      hours--;
     }
     if (cursorposition == 3 || cursorposition == 4)
     {
      minutes--;
     }
     setClockValues();
     break;
     }
   case btnSELECT:
     {
      //TODO Set clock value for all positions
     lcd.print("SELECT");
     //
     if(timeSet){
       setHours = 0;
       setMinutes = 0;
       timeSet = false;
     }
     else
     {
       setHours = clockArray[hours][0];
       setMinutes = clockArray[0][minutes];
       timeSet = true;
     }
     break;
     }
     case btnNONE:
     {
     //Set second row when no button is pressed
     setClockValues();
     break;
     }
 }
}

void setClockValues(){
     //Correcting faulty values
     if(minutes > 59){ hours++; minutes = 0;}
     if(minutes < 0) { hours--; minutes =59; }
     if(hours > 23){ hours = 0;}
     if(hours < 0) { hours = 23; }

     //Setting Values
     lcd.setCursor(0,1);
     if(hours < 10 && hours > -1){lcd.print("0"); lcd.setCursor(1,1);}
     lcd.print(clockArray[hours][0]);
     lcd.setCursor(2,1);
     lcd.print(":");
     lcd.setCursor(3,1);
     if(minutes < 10){lcd.print("0"); lcd.setCursor(4,1);}
     lcd.print(clockArray[0][minutes]);

     //CLEAR DISPLAY AFTER TIME
     lcd.setCursor(5,1);
     lcd.print("           ");
     
     //Display Cursorposition
     lcd.setCursor(8,1);
     lcd.print(cursorposition);
}

void correctingInitialClockValues(){
   if(minutes == 60) { minutes = 0;}
   if(hours == 60) { hours = 0;}
}

void initClockArray(){
  for(int h = 0; h < 24; h++){
      clockArray[h][0] = h;
  }
  for(int m = 0; m < 60; m++){
      clockArray[0][m] = m;
  }
}
