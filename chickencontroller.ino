/*
 * Automated IR-based Chickencoopdoor with LCD Display to set closing and opening times
 * 2017_12_09  : Added Statemachine
 * -Benedict Hadi
 */

#define startIR digitalWrite(txPinIRFront, HIGH);
#define stopIR digitalWrite(txPinIRFront, LOW);
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5
#define MAX_DISPLAY_LENGTH 15
#define MIN_DISPLAY_LENGTH 0

#include <avr/eeprom.h>
#include <LiquidCrystal.h>

boolean testrun = false;

const int rxPinIRFront = A7;
const int txPinIRFront = 3;
const int rxPinIRBack = A6;
const int txPinIRBack = 2;
const int VAL_DELAY = 25;
const int VAL_TRIGGER_FRONT = -500;
const int VAL_TRIGGER_BACK = 500;
int clockArray[24][60];
int hoursClose = 23;
int minutesClose = 30;
int hoursOpen = 23;
int minutesOpen = 30;
boolean timeSet = false;
int setHoursClose = 0;
int setMinutesClose = 0;
int setHoursOpen = 0;
int setMinutesOpen = 0;
int tempcursor = 0;

//Statemachine variables
int state = 0;
int prevstate = 0;
int logiccount = 0;
boolean frontEntry = false;
boolean backEntry = false;
int chickencounter = 0;

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
//LCD Variables
int lcd_key     = 0;
int adc_key_in  = 0;
int cursorposition = 0;
boolean clockChanged = false;

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
  // Creating Offset Value
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

  /*DISPLAY SENSOR VALUES IF NEEDED */
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

    /* START BARRIER BREAKING LOGIC TREE */
    /* BARRIER TRAVERSING (ACTION NEEDED)*/
    // UP-UP :        state = 0
    // DOWN - DOWN :  state = 1
    // DOWN - UP :    state = 2
    // UP - DOWN :    state = 3

    // Saving previous state
    prevstate = state;
    
    // Determining loop State
    if(!frontTriggered && !backTriggered)
    {
      state = 0;
    }
    else if(frontTriggered && backTriggered)
    {
      state = 1;
    }
    else if(frontTriggered && !backTriggered)
    {
      state = 2;
    }
    else if(!frontTriggered && backTriggered)
    {
      state = 3;
    }

    /* EXAMPLE */
    //   x    ,     0     ,     1      ,  2     ,    3         logiccounts
    //   0    ,     2     ,     1      ,  3     ,    0         states
    //UP - UP, DOWN - UP, DOWN - DOWN, UP - DOWN, UP - UP
    
    //UP - UP, UP - DOWN, DOWN - DOWN, DOWN - UP, ""
    
    //UP - UP, UP - DOWN, DOWN - DOWN, UP - DOWN, ""
    //UP - UP, DOWN - UP, DOWN - DOWN, DOWN - UP, ""

    /*BARRIER TOUCHING (NO ACTION NEEDED)*/
    //UP - UP, DOWN - UP, UP - UP
    //UP - UP, UP - DOWN, UP - UP
    
    /* STATEMACHINE START*/
    if(prevstate != state)
    {
      //Nothing prev
      if(prevstate == 0)
      {
        logiccount = 0;  //if previous state implies barrier up, reset the logiccount (i.e. UP-UP doesnt count towards logic tree, only the following decision does.}
        frontEntry = false;
        backEntry = false;
        if(state == 1)
        {
          Serial.println("WARNING: Object entered both Barriers too fast or simultaneously");
        }
        else if(state == 2)
        {
          frontEntry = true;
          //Serial.println("Object entered Front Barrier");
        }
        else if(state == 3)
        {
          backEntry = true;
          //Serial.println("Object entered Back Barrier");
        }
      }
      //front + back trig prev
      else if(prevstate == 1)
      {
        if(state == 0)
        {
          Serial.println("WARNING: Object left Barriers too fast or simultaneously");
        }
        else if(state == 2)
        {
          if(logiccount == 1){
            if(backEntry)
            {
              logiccount++;
            }
          }
        }
        else if(state == 3)
        {
          if(logiccount == 1){
            if(frontEntry)
            {
              logiccount++;
            }
          }
        }
      }
      //frontTrig prev
      else if(prevstate == 2)
      {
        if(state == 0)
        {
          if(logiccount != 0){
            logiccount++;
          }
        }
        else if(state == 1)
        {
          if(logiccount == 0){
            logiccount++;
          }
        }
        else if(state == 3)
        {
          Serial.println("WARNING: (SKIPPED STATE 1) PROGRAM TIMING TOO SLOW OR BAD LUCK");
        }
      }
      //backtrig prev
      else if(prevstate == 3)
      {
        if(state == 0)
        {          
          if(logiccount == 2){
            logiccount++;
          }
        }
        else if(state == 1)
        {
          if(logiccount == 0){
            logiccount++;
          }
        }
        else if(state == 2)
        {
          Serial.println("WARNING: PROGRAM TIMING TOO SLOW OR BAD LUCK");
        }
      }
      Serial.println(logiccount);
    }
    /* STATEMACHINE END*/
    /* END BARRIER BREAKING LOGIC TREE */

    
    if(logiccount == 3)
    {
      logiccount = 0;
      if(frontEntry)
      {
        //CHICKEN PASSED THROUGH THE FRONT (I.E. +1 CHICKEN IN COOP)
        chickencounter++;
        Serial.println("Amount of Chickens in Coop:");
        Serial.println(chickencounter);
      }
      else if(backEntry)
      {
        //CHICKEN PASSED THROUGH THE BACK (I.E. -1 CHICKEN IN COOP)
        chickencounter--;
        Serial.println("Amount of Chickens in Coop:");
        Serial.println(chickencounter);
      }
      else if(!backEntry || !frontEntry)
      {
        Serial.println("WARNING: LOGIC TREE FAULTY");
      }
      else
      {Serial.println("what the fuck");}
    }
    
  lcd.setCursor(0,0);
  //lcd.print(millis()/1000);      // display seconds elapsed since power-up
  //lcd.print(trigCountFront);
  
  if(testrun){
    /*
    if (frontTriggered) {lcd.print("Barrier Broken");}
    if (!frontTriggered) {lcd.print("Barrier Up      ");}
    */
    lcd.print(chickencounter);
    lcd.setCursor(2,0);
    lcd.print("Chicken inside");
  }
  else{
    if(!timeSet)
    {
     if (cursorposition >= 0 && cursorposition <= 3)
     {
      lcd.print("Set Closing H  ");
     }
     else if (cursorposition > 3 && cursorposition <= 7)
     {
      lcd.print("Set Closing Min  ");
     }
     else if (cursorposition > 7 && cursorposition <= 11)
     {
      lcd.print("Set Opening H  ");
     }
     else if (cursorposition > 11 && cursorposition <= 15)
     {
      lcd.print("Set Opening Min  ");
     }
    }
    else
    {
      lcd.print("Close/Open Time:  ");
    }
  }
  
  //lcd.setCursor(7,1);            // move to the begining of the second line
  lcd_key = read_LCD_buttons();  // read the buttons
  keytrigger();
  }
}

// read the buttons
int read_LCD_buttons()
{
 adc_key_in = analogRead(0);      // read the value from the sensor 
 // add approx 50 to those values
 //Serial.println("key value"); Serial.println(adc_key_in);
 if (adc_key_in > 1000) return btnNONE; // 1st option for speed reasons since it will be the most likely result
 
 /*BUTTON THRESHOLD*/
 //Manually read triggervalues for buttons, then added 50
 if (adc_key_in < 120)   return btnRIGHT;  
 if (adc_key_in < 230)  return btnUP; 
 if (adc_key_in < 350)  return btnDOWN; 
 if (adc_key_in < 500)  return btnLEFT; 
 if (adc_key_in < 700)  return btnSELECT;   
 
 return btnNONE;  // default value
}

void keytrigger()
{
 lcd.setCursor(0,1);
 if(cursorposition < MIN_DISPLAY_LENGTH) {cursorposition = MIN_DISPLAY_LENGTH;}
 if(cursorposition >= MAX_DISPLAY_LENGTH) {cursorposition = MAX_DISPLAY_LENGTH;}
 switch (lcd_key)               // depending on which button was pushed, perform an action
 {
   case btnRIGHT:
     {
      clockChanged = true;
     //change cursor to right position unless already to the rightmost position
     //lcd.print("Cursor to RIGHT");
     lcd.setCursor(cursorposition,1);
     cursorposition++; 
     if(cursorposition >= MAX_DISPLAY_LENGTH) {cursorposition = MAX_DISPLAY_LENGTH;}
     lcd.setCursor(cursorposition,1);
     break;
     }
   case btnLEFT:
     {
      clockChanged = true;
     //change cursor to left position unless already to the leftmost position
     //lcd.print("Cursor to LEFT");
     lcd.setCursor(cursorposition,1);
     cursorposition--; 
     if(cursorposition < MIN_DISPLAY_LENGTH) {cursorposition = MIN_DISPLAY_LENGTH;}
     lcd.setCursor(cursorposition,1);
     break;
     }
   case btnUP:
     {
       if(!timeSet)
       {
         clockChanged = true;
         //Increment clock value at selected position
         lcd.setCursor(cursorposition,1);
         if (cursorposition >= 0 && cursorposition <= 3)
         {
          hoursClose++;
         }
         else if (cursorposition > 3 && cursorposition <= 7)
         {
          minutesClose++;
         }
         else if (cursorposition > 7 && cursorposition <= 11)
         {
          hoursOpen++;
         }
         else if (cursorposition > 11 && cursorposition <= 15)
         {
          minutesOpen++;
         }
         setClockValues();
       }
       break;
     }
   case btnDOWN:
     {
       if(!timeSet)
       {
         clockChanged = true;
         //Decrement clock value at selected position
         lcd.setCursor(cursorposition,1);
         if (cursorposition >= 0 && cursorposition <= 3)
         {
          hoursClose--;
         }
         else if (cursorposition > 3 && cursorposition <= 7)
         {
          minutesClose--;
         }
         else if (cursorposition > 7 && cursorposition <= 11)
         {
          hoursOpen--;
         }
         else if (cursorposition > 11 && cursorposition <= 15)
         {
          minutesOpen--;
         }
         setClockValues();
       }
       break;
     }
   case btnSELECT:
     {
        //TODO Set clock value for all positions
       clockChanged = true;
       //lcd.print("SELECT");
       //
       if(timeSet){
         setHoursClose = 0;
         setMinutesClose = 0;
         setHoursOpen = 0;
         setMinutesOpen = 0;
         timeSet = false;
         delay(1000);
       }
       else
       {
         setHoursClose = clockArray[hoursClose][0];
         setMinutesClose = clockArray[0][minutesClose];
         setHoursOpen = clockArray[hoursClose][1];
         setMinutesOpen = clockArray[1][minutesClose];
         timeSet = true;
         delay(1000);
       }
       break;
     }
     case btnNONE:
     {
       //Set second row when no button is pressed
       if(clockChanged)
       {
        setClockValues();
        clockChanged = false;
       }
       break;
     }
 }
}

void setClockValues(){
     //Correcting faulty values
     if(minutesClose > 59){ hoursClose++; minutesClose = 0;}
     if(minutesClose < 0) { hoursClose--; minutesClose = 59; }
     if(hoursClose > 23){ hoursClose = 0;}
     if(hoursClose < 0) { hoursClose = 23; }
     
     if(minutesOpen > 59){ hoursOpen++; minutesOpen = 0;}
     if(minutesOpen < 0) { hoursOpen--; minutesOpen = 59; }
     if(hoursOpen > 23){ hoursOpen = 0;}
     if(hoursOpen < 0) { hoursOpen = 23; }

     tempcursor = 0;
     
     /*SETTING VALUES CLOSING TIME */
     lcd.setCursor(tempcursor,1);
     if(hoursClose < 10 && hoursClose > -1)
     {
      lcd.print("0"); 
      tempcursor++; 
      lcd.setCursor(tempcursor,1);
      lcd.print(clockArray[hoursClose][0]);
      tempcursor++;
     }
     else
     {
      lcd.print(clockArray[hoursClose][0]);
      tempcursor += 2;
     }
     lcd.setCursor(tempcursor,1);
     lcd.print(":");
     tempcursor++;
     lcd.setCursor(tempcursor,1);
     if(minutesClose < 10)
     {
      lcd.print("0"); 
      tempcursor++; 
      lcd.setCursor(tempcursor,1);
      lcd.print(clockArray[0][minutesClose]);
      tempcursor++;
     }
     else
     {
      lcd.print(clockArray[0][minutesClose]); 
      tempcursor += 2;
     }
     //tempcursor at +5 higher than starting value now

    
    lcd.print("cl"); 
    tempcursor += 2;

    
    lcd.print("  "); 
    tempcursor += 2;

    /*SETTING VALUES OPENING TIME */
     lcd.setCursor(tempcursor,1);
     if(hoursOpen < 10 && hoursOpen > -1)
     {
      lcd.print("0"); 
      tempcursor++; 
      lcd.setCursor(tempcursor,1);
      lcd.print(clockArray[hoursOpen][0]);
      tempcursor++;
     }
     else
     {
      lcd.print(clockArray[hoursOpen][0]);
      tempcursor += 2;
     }
     lcd.setCursor(tempcursor,1);
     lcd.print(":");
     tempcursor++;
     lcd.setCursor(tempcursor,1);
     if(minutesOpen < 10)
     {
      lcd.print("0"); 
      tempcursor++; 
      lcd.setCursor(tempcursor,1);
      lcd.print(clockArray[0][minutesOpen]);
      tempcursor++;
     }
     else
     {
      lcd.print(clockArray[0][minutesOpen]); 
      tempcursor += 2;
     }
     //tempcursor at +5 higher than starting value now
     
     lcd.print("op"); 
     tempcursor += 2;
}

void correctingInitialClockValues(){
   if(minutesClose == 60) { minutesClose = 0;}
   if(hoursClose == 60) { hoursClose = 0;}
   if(minutesOpen == 60) { minutesOpen = 0;}
   if(hoursOpen == 60) { hoursOpen = 0;}
}

void initClockArray(){
  for(int h = 0; h < 24; h++){
      clockArray[h][0] = h;
  }
  for(int m = 0; m < 60; m++){
      clockArray[0][m] = m;
  }
}
