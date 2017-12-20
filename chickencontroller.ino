/*
 * Automated IR-based Chickencoopdoor with LCD Display to set closing and opening times
 * 2017_12_09  : Added Statemachine
 * 2017_12_11  : Added LCD Menu
 * 2017_12_14  : Added Clock Module
 * 2017_12_15  : Added Motor logic, motor activation and limit switch function
 * 2017_12_20  : Changed Motor Controls, added interrupt for lowering door (3s) !NOT TESTED!
 * -Benedict Hadi
 */
#include <avr/eeprom.h>
#include <LiquidCrystal.h>
#include <DS3231.h>

#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5
#define MAX_DISPLAY_LENGTH 15
#define MIN_DISPLAY_LENGTH 0
#define LIMIT_SWITCH_TRIGGER_VALUE 1 //need to check

boolean testrun = false;

//Pins
const int rxPinIRFront = A6;
const int txPinIRFront = 2;
const int rxPinIRBack = A7;
const int txPinIRBack = 3;
const int relaisA = 4;
const int relaisB = 5;
const int limitSwitchTop = A2;
const int limitSwitchBottom = A3;
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

//Time Variables
int clockArray[24][60];
int hoursClose = 0;
int minutesClose = 0;
int hoursOpen = 0;
int minutesOpen = 0;
int hoursClock = 0;
int minutesClock = 0;
int hoursModule = 0;
int minutesModule = 0;
int secondsModule = 0;
int setHoursClose = 0;
int setMinutesClose = 0;
int setHoursOpen = 0;
int setMinutesOpen = 0;
DS3231 Clock;
boolean hourformat = false;
boolean PM = false;

//Chicken Variables
int setChicken = 0;
int maxChicken = 5;


//Statemachine variables
int state = 0;
int prevstate = 0;
int logiccount = 0;
int chickencounter = 0;
int trigCountFront = 0;
int trigCountBack = 0;
int permittedFault = 2;
boolean frontTriggered = false;
boolean backTriggered = false;
boolean frontEntry = false;
boolean backEntry = false;
const int VAL_DELAY = 25;
const int VAL_TRIGGER_FRONT = -500;
const int VAL_TRIGGER_BACK = 500;

//Motor/door variables
boolean doorLowering = false;
boolean doorUp = false;
boolean doorDown = false;
int interruptOverflowCounter = 0;
int doorClosingDuration = 3;

//LCD Variables
int lcd_key     = 0;
int adc_key_in  = 0;
int cursorposition = 0;
boolean clockChanged = false;
int lcdmenu = 0; //0: time open/close set; 1: time current set(clock module); 2: maxChicken set
boolean changeMenu = false;    // change behaviour of right/left
boolean chickenSet = false;   // amount of chicken
boolean clockSet = false;    // lcd clock time
boolean timeSet = false;    // open close times
int tempcursor = 0;        // navigate to valid position on the row

//IR Light Values
int VAL_RECV_OFFSET_FRONT = 0;
int VAL_RECV_OFFSET_BACK = 0;
int VAL_RECV_HIGH_FRONT = 0;
int VAL_RECV_HIGH_BACK = 0;
int VAL_DIFF_FRONT = 0;
int VAL_DIFF_BACK = 0;


// Data output mode:
// R - raw data
// T - trigger events
char dataOutput = 'T';

void setup()
{
  Serial.begin(9600);
  Serial.println("Starting Setup");

  //LCD setup
  lcd.begin(16, 2);              // start the library
  lcd.setCursor(0,0);
  initClockArray();
  correctingInitialClockValues();
  setClockValues();
  
  //Clock Module and I^2C setup
  Wire.begin();
  setClockModule();
  setPinModes();

  cli();//stop interrupts

//setzen des timer1 interrupt auf 1Hz
  TCCR1A = 0;// Timmerregister TCCR1A auf 0 setzen
  TCCR1B = 0;// Timmerregister TCCR1B auf 0 setzen
  TCNT1  = 0;// Zählregister auf 0 setzen
  // Setzen des compare match register für 1Hz
  OCR1A = 15624;// = (16*10^6) / (1024*1) - 1 (muss < 65536 sein)
  // Timer modus: CTC mode
  TCCR1B |= (1 << WGM12);
  // Setzen CS12 und CS10 bits für Vorteiler 64
  TCCR1B |= (1 << CS11) | (1 << CS10);  
  // Aktivieren timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  // Setzen der Bits für externen Takt an DigitalenPort 47
  TCCR5B |= (1 << CS52) | (1 << CS51) | (1 << CS50);  


//sei();//allow interrupts

  Serial.println("Setup complete");
}

// Timer2 erzeugt alle 3s
ISR(TIMER1_COMPA_vect){
  TCNT5 = 0;        // Setzen des Zählregisters auf 0
  interruptOverflowCounter++;
  if(interruptOverflowCounter >= doorClosingDuration)
  {
    moveMotor('s');
    interruptOverflowCounter = 0;
    Serial.println("timer triggerd 3 times");
    cli();
  }
}

void setClockModule()
{
  Clock.setClockMode(false);
  minutesModule = Clock.getMinute();
  hoursModule = Clock.getHour(hourformat , PM);
  secondsModule = Clock.getSecond();
}

void setPinModes()
{
  pinMode(rxPinIRFront, INPUT);
  pinMode(txPinIRFront, OUTPUT);
  pinMode(rxPinIRBack, INPUT);
  pinMode(txPinIRBack, OUTPUT);
  pinMode(relaisA, OUTPUT);
  pinMode(relaisB, OUTPUT);
  pinMode(limitSwitchTop, INPUT);
  pinMode(limitSwitchBottom, INPUT);
}

void getClockValues()
{
  minutesModule = Clock.getMinute();
  hoursModule = Clock.getHour(hourformat , PM);
  secondsModule = Clock.getSecond();
}

void loop() {
  //Function polling clock Module values
  getClockValues();
  
  //Function controlling the motor
  motorControl();

  //Getting raw IR Data
  getIRValues();
  /*DISPLAY SENSOR VALUES FOR DEBUGGING */
  //Serial.println(VAL_DIFF_FRONT);
  //Serial.println(VAL_DIFF_BACK);

  //IR Logic
  createStates();
  statemachine();
  logicTrigger();

  //Display Functions
  setInfoRow();
  lcd_key = read_LCD_buttons();  // read the buttons
  keytrigger();
}

void motorControl()
{
  doorUp = limitSwitchTriggeredTop();
  boolean openingTime = false;
  boolean closingTime = false;
  
  if(doorLowering) //if motor is lowering
  {
    if(state != 0)
    {
      // stop motor
      moveMotor('s');
    }
  }
  else if(limitSwitchTriggeredTop)
  {
    moveMotor('s');
  }
  
  if(timeSet && clockSet){
    if(hoursOpen == hoursModule && minutesOpen == minutesModule)
    {
      openingTime = true;
    }
    if(hoursClose == hoursModule && minutesClose == minutesModule)
    {
      closingTime = true;
    }
    
    if(openingTime)
    {
        if(doorUp)
        {
          //if up stay up
          moveMotor('s');
        }
        else if(!doorUp)
        {
          //start motor up
          moveMotor('u');
          doorLowering = false;
        }
    }
    else if(closingTime && chickencounter == maxChicken)
    {
      if(state == 0) //only lower if nothing in door
      {
        if(doorUp)
        {
          //start motor down
          //start interrupt
          sei();
          doorLowering = true;
        }
        else if(doorDown)
        {
          moveMotor('s');
          doorLowering = false;
        }
      }
      else
      {
        moveMotor('s');
      }
    }
  }
}

void getIRValues()
{
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
//  if( dataOutput == 'R'){  
//    Serial.print("HIGH Front IR Value: ");
//    Serial.print(VAL_RECV_HIGH_FRONT); 
//    Serial.print(" HIGH Back IR Value: ");
//    Serial.println(VAL_RECV_HIGH_BACK);
//    }

  //Calculate Value Difference
  VAL_DIFF_FRONT = VAL_RECV_HIGH_FRONT - VAL_RECV_OFFSET_FRONT;
  VAL_DIFF_BACK = VAL_RECV_HIGH_BACK - VAL_RECV_OFFSET_BACK;

  if( dataOutput == 'R'){  
    Serial.print("Front IR Value: ");
    Serial.print(VAL_RECV_HIGH_FRONT); 
    Serial.print(" Back IR Value: ");
    Serial.println(VAL_RECV_HIGH_BACK);
    }
  
  /* FRONT/BACK BARRIER RX/TX END */
}  

void createStates()
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
    /* BARRIER TRAVERSING (ACTION NEEDED)*/
    //   x    ,     0     ,     1      ,  2     ,    3         logiccounts
    //   0    ,     2     ,     1      ,  3     ,    0         states
    //UP - UP, DOWN - UP, DOWN - DOWN, UP - DOWN, UP - UP
    //UP - UP, UP - DOWN, DOWN - DOWN, DOWN - UP, ""
    //UP - UP, UP - DOWN, DOWN - DOWN, UP - DOWN, ""
    //UP - UP, DOWN - UP, DOWN - DOWN, DOWN - UP, ""

    /*BARRIER TOUCHING (NO ACTION NEEDED)*/
    //UP - UP, DOWN - UP, UP - UP
    //UP - UP, UP - DOWN, UP - UP
   /* END BARRIER BREAKING LOGIC TREE */
}

void statemachine()
{
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
}

void logicTrigger()
{
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
}

void setInfoRow()
{
  lcd.setCursor(0,0);

  if(lcdmenu == 0)
  {
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
      else{
       lcd.print("Close/Open Time:  ");
      }
   }
   else if(lcdmenu == 1)
   {
    if(!clockSet)
    {
     if (cursorposition >= 0 && cursorposition <= 7)
     {
        lcd.print("Set Current H  ");
     }
     else if (cursorposition > 7 && cursorposition <= 15)
     {
        lcd.print("Set Current Min:  ");
     }
    }
    else{
     lcd.print("CLOCK IS SET :          "); 
    }
   }
   else if(lcdmenu == 2)
   {
      if(!chickenSet)
      {
        lcd.print("Chicken Amount?");
      }
      else{
       lcd.print("Chicken Set    ");
      }
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
      if(changeMenu)
      {
        lcdmenu++;
        if(lcdmenu >= 2){lcdmenu = 2;}
        menustate();
        delay(500);
        break;
      }
      else
      {
        //change cursor to right position unless already to the rightmost position
       //lcd.print("Cursor to RIGHT");
       lcd.setCursor(cursorposition,1);
       cursorposition++; 
       if(cursorposition >= MAX_DISPLAY_LENGTH) {cursorposition = MAX_DISPLAY_LENGTH;}
       lcd.setCursor(cursorposition,1);
       break;
      }
     }
   case btnLEFT:
     {
      clockChanged = true;
      if(changeMenu)
      {
        lcdmenu--;
        if(lcdmenu <= 0){lcdmenu =0;}
        menustate();
        delay(500);
        break;
      }
      else
      {
       //change cursor to left position unless already to the leftmost position
       lcd.setCursor(cursorposition,1);
       cursorposition--; 
       if(cursorposition < MIN_DISPLAY_LENGTH) {cursorposition = MIN_DISPLAY_LENGTH;}
       lcd.setCursor(cursorposition,1);
       break;
      }
     }
   case btnUP:
     {
      clockChanged = true;
      if(lcdmenu == 0)
      {
         if(!timeSet)
         {
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
         }
      }
      else if(lcdmenu == 1)
      {
       //Decrement clock value at selected position
       lcd.setCursor(cursorposition,1);
       if (cursorposition >= 0 && cursorposition <= 7)
       {
        hoursClock++;
       }
       else if (cursorposition > 7 && cursorposition <= 15)
       {
        minutesClock++;
       }
      }
      else if(lcdmenu == 2)
      {
        maxChicken++;
        if(maxChicken >= 99){maxChicken = 99;}
        delay(500);
      }
     setClockValues();
     break;
   }
   case btnDOWN:
     {
       clockChanged = true;
        if(lcdmenu == 0)
        {
         if(!timeSet)
          {
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
          }
        }
        else if(lcdmenu == 1)
        {
          //Decrement clock value at selected position
           lcd.setCursor(cursorposition,1);
           if (cursorposition >= 0 && cursorposition <= 7)
           {
            hoursClock--;
           }
           else if (cursorposition > 7 && cursorposition <= 15)
           {
            minutesClock--;
           }
        }
        else if(lcdmenu == 2)
        {
          maxChicken--;
          if(maxChicken <= 0){maxChicken = 0;}
          delay(500);
        }
       setClockValues();
       break;
     }
   case btnSELECT:
     {
        //TODO Set clock value for all positions
       clockChanged = true;
       if(lcdmenu == 0)
       {
         if(timeSet){
           setHoursClose = 0;
           setMinutesClose = 0;
           setHoursOpen = 0;
           setMinutesOpen = 0;
           timeSet = false;
           changeMenu = false;
           delay(1000);
         }
         else
         {
           setHoursClose = clockArray[hoursClose][0];
           setMinutesClose = clockArray[0][minutesClose];
           setHoursOpen = clockArray[hoursOpen][0];
           setMinutesOpen = clockArray[0][minutesOpen];
           timeSet = true;
           changeMenu = true;
           delay(1000);
         }
       }
       else if(lcdmenu == 1)
       {
        if(clockSet){
           hoursClock = hoursModule;
           minutesClock = minutesModule;
           clockSet = false;
           changeMenu = false;
           delay(1000);
         }
         else
         {
           Clock.setHour(hoursClock);
           Clock.setMinute(minutesClock);
           clockSet = true;
           changeMenu = true;
           delay(1000);
         }
       }
       else if(lcdmenu == 2)
       {
         if(chickenSet){
           maxChicken = 0;
           chickenSet = false;
           changeMenu = false;
           delay(1000);
         }
         else
         {
           setChicken = maxChicken;
           chickenSet = true;
           changeMenu = true;
           delay(1000);
         }
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
       if(lcdmenu == 1)
       {
        setClockValues(); 
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

     if(minutesClock > 59){ hoursClock++; minutesClock =0;}
     if(minutesClock < 0) { hoursClock--; minutesClock = 59; }
     if(hoursClock > 23){ hoursClock = 0;}
     if(hoursClock < 0) { hoursClock = 23; }
     
     tempcursor = 0;
     if(lcdmenu == 0)
     {
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
   else if(lcdmenu == 1)
   {
       lcd.setCursor(tempcursor,1);
       if(hoursClock < 10 && hoursClock > -1)
       {
        lcd.print("0"); 
        tempcursor++; 
        lcd.setCursor(tempcursor,1);
        lcd.print(clockArray[hoursClock][0]);
        tempcursor++;
       }
       else
       {
        lcd.print(clockArray[hoursClock][0]);
        tempcursor += 2;
       }
       lcd.setCursor(tempcursor,1);
       lcd.print(":");
       tempcursor++;
       lcd.setCursor(tempcursor,1);
       if(minutesClock < 10)
       {
        lcd.print("0"); 
        tempcursor++; 
        lcd.setCursor(tempcursor,1);
        lcd.print(clockArray[0][minutesClock]);
        tempcursor++;
       }
       else
       {
        lcd.print(clockArray[0][minutesClock]); 
        tempcursor += 2;
       }
       //tempcursor at +5 higher than starting value now

      lcd.print("   "); 
      tempcursor += 3;

     if(hoursModule < 10 && hoursModule > -1)
     {
        lcd.print("0"); 
        tempcursor++; 
        lcd.setCursor(tempcursor,1);
        lcd.print(hoursModule);
        tempcursor++;
     }
     else
     {
      lcd.print(clockArray[hoursModule][0]);
      tempcursor += 2;
     }
     lcd.setCursor(tempcursor,1);
     lcd.print(":");
     tempcursor++;
     lcd.setCursor(tempcursor,1);
     
     if(minutesModule < 10)
     {
      lcd.print("0"); 
      tempcursor++; 
      lcd.setCursor(tempcursor,1);
      lcd.print(clockArray[0][minutesModule]);
      tempcursor++;
     }
     else
     {
      lcd.print(clockArray[0][minutesModule]); 
      tempcursor += 2;
     }
     
     lcd.setCursor(tempcursor,1);
     lcd.print(":");
     tempcursor++;
     lcd.setCursor(tempcursor,1);
     
     if(secondsModule < 10)
     {
      lcd.print("0"); 
      tempcursor++; 
      lcd.setCursor(tempcursor,1);
      lcd.print(clockArray[0][secondsModule]);
      tempcursor++;
     }
     else
     {
      lcd.print(clockArray[0][secondsModule]); 
      tempcursor += 2;
     }
     
   }

   else if(lcdmenu == 2)
   {
      lcd.setCursor(tempcursor,1);
      if(maxChicken < 10 && maxChicken > -1)
       {
        lcd.print("0"); 
        tempcursor++; 
        lcd.setCursor(tempcursor,1);
        lcd.print(maxChicken);
        tempcursor++;
       }
       else
       {
        lcd.print(maxChicken);
        tempcursor += 2;
       }
      lcd.print("                   ");
   }
}

void moveMotor(char command){
  if(command == 'u'){
   //Set relais to go up
   digitalWrite(relaisA, HIGH);
   digitalWrite(relaisB, LOW);
   doorLowering = false;
  }
  else if(command == 'd'){
    //set relais to go down
    digitalWrite(relaisA, LOW);
    digitalWrite(relaisB, HIGH);
    doorLowering = true;
  }
  else if(command == 's'){
    //Set relais to stop
    digitalWrite(relaisA, LOW);
    digitalWrite(relaisB, LOW);
    doorLowering = false;
  }
}

boolean limitSwitchTriggeredTop(){
  boolean triggered = false; 
  analogRead(limitSwitchTop);

  if(limitSwitchTop >= LIMIT_SWITCH_TRIGGER_VALUE)
  {
    triggered = true;
  }
  return triggered;
}

void correctingInitialClockValues(){
   if(minutesClose == 60) { minutesClose = 0;}
   if(hoursClose == 24) { hoursClose = 0;}
   if(minutesOpen == 60) { minutesOpen = 0;}
   if(hoursOpen == 24) { hoursOpen = 0;}
   if(minutesClock == 60) { minutesOpen = 0;}
   if(hoursClock == 24) { hoursOpen = 0;}
}

void initClockArray(){
  for(int h = 0; h < 24; h++){
      clockArray[h][0] = h;
  }
  for(int m = 0; m < 60; m++){
      clockArray[0][m] = m;
  }
}

void menustate(){
        if(lcdmenu == 0 && timeSet == false){changeMenu = false;}else if(lcdmenu == 0 && timeSet == true){changeMenu = true;}
        if(lcdmenu == 1 && clockSet == false){changeMenu = false;}else if(lcdmenu == 0 && clockSet == true){changeMenu = true;}
        if(lcdmenu == 2 && chickenSet == false){changeMenu = false;}else if(lcdmenu == 0 && chickenSet == true){changeMenu = true;}
}
