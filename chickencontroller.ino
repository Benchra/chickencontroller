/*
   Automated IR-based Chickencoopdoor with LCD Display to set closing and opening times
   2017_12_09  : Added Statemachine
   2017_12_11  : Added LCD Menu
   2017_12_14  : Added Clock Module
   2017_12_15  : Added Motor logic, motor activation and limit switch function
   2017_12_20  : Changed Motor Controls, added interrupt for lowering door (3s) !NOT TESTED!
   2017_12_22  : Bugfixed Motor Controls
   -Benedict Hadi
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
#define MAX_MENU 3
#define DOOR_CLOSING_DURATION 4

boolean testrun = false;

//Pins
const int rxPinIRFront = A6;
const int txPinIRFront = 2;
const int rxPinIRBack = A7;
const int txPinIRBack = 3;
const int relaisA = 11; //up
const int relaisB = 10; //down
const int limitSwitchTop = 12;
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
boolean openingTimeReached = false;
boolean closingTimeReached = false;
int currentDoorClosingDuration = 0;
int targetDoorClosingDuration = 0;
int prevseconds = 61;
int seconds = 61;
int layoverseconds = 0;

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
int permittedFault = 4;
boolean frontTriggered = false;
boolean backTriggered = false;
boolean frontEntry = false;
boolean backEntry = false;
const int VAL_DELAY = 25;
int VAL_TRIGGER_FRONT = -500;
int VAL_TRIGGER_BACK = -500;

//Motor/door variables
boolean doorLowering = false;
boolean doorRaising = false;
boolean doorUp = false;
boolean doorDown = false;
int interruptOverflowCounter = 0;
int interruptOverflowCounterPrev = 0;
boolean startLoweringCountdown = false;
boolean manualMovement = false;
unsigned int limitSwitchTopValue = 0;

//LCD Variables
int lcd_key     = 0;
int adc_key_in  = 0;
int cursorposition = 0;
int lcdmenu = 0;				// 0: time open/close set; 1: time current set(clock module); 2: maxChicken set; 3:Control Motor Manually
boolean changeMenu = false;		// change behaviour of right/left
boolean chickenSet = false;		// amount of chicken
boolean clockSet = false;		// lcd clock time
boolean timeSet = false;		// open close times
int tempcursor = 0;				// used to navigate to valid position on the row

//IR Light Values
int VAL_RECV_OFFSET_FRONT = 0;
int VAL_RECV_OFFSET_BACK = 0;
int VAL_RECV_HIGH_FRONT = 0;
int VAL_RECV_HIGH_BACK = 0;
int VAL_DIFF_FRONT = 0;
int VAL_DIFF_BACK = 0;
boolean analogIR = false;

// Data output mode:
// R - raw data
// T - trigger events
// S - Sensor Values
char dataOutput = 'S';

void setup()
{
  Serial.begin(9600);
  Serial.println("Starting Setup");

  //LCD setup
  lcd.begin(16, 2);              // start the library
  lcd.setCursor(0, 0);
  initClockArray();
  correctingInitialClockValues();
  setClockValues();

  //Clock Module and I^2C setup
  Wire.begin();
  setClockModule();
  setPinModes();
  //setupInterrupts();
  
  Serial.println("Setup complete");
}

void setupInterrupts()
{
  noInterrupts();
  //cli();//stop interrupts
  //set timer1 interrupt to 1Hz
  TCCR1A = 0;// Timmerregister TCCR1A set to 0
  TCCR1B = 0;// Timmerregister TCCR1B set to 0
  TCNT1  = 0;// set register to 0
  // Setzen compare match register for 1Hz
  OCR1A = 15624;// = (16*10^6) / (1024*1) - 1 (muss < 65536 sein)
  // Timer mode: CTC mode
  TCCR1B |= (1 << WGM12);
  // set CS10 bits for prescaler 1
  TCCR1B |= (1 << CS10);
  // activate timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  //sei();//allow interrupts
}

// Timer2 creates interrupt every 3s
ISR(TIMER1_COMPA_vect) {
  TCNT5 = 0;        // Set Countregister to 0
  interruptOverflowCounter++;
  if(interruptOverflowCounter >= 3)
  {
    interruptOverflowCounter = 0;
  }
}
 

void setClockModule()
{
  Clock.setClockMode(false);
  Clock.setHour(0);
  Clock.setMinute(0);
  Clock.setSecond(0);
  minutesModule = Clock.getMinute();
  hoursModule = Clock.getHour(hourformat , PM);
  secondsModule = Clock.getSecond();
  Serial.print("Time : "); Serial.print(hoursModule); Serial.print(":"); Serial.print(minutesModule); Serial.print(":"); Serial.println(secondsModule);
}

void setPinModes()
{

  if(analogIR)
  {
    TCCR3B = TCCR3B & B11111000 | B00000001; // set timer 3 divisor to 1
  }
  pinMode(rxPinIRFront, INPUT);
  pinMode(txPinIRFront, OUTPUT);
  pinMode(rxPinIRBack, INPUT);
  pinMode(txPinIRBack, OUTPUT);
  pinMode(relaisA, OUTPUT);
  pinMode(relaisB, OUTPUT);
  pinMode(limitSwitchTop, INPUT_PULLUP);
}

void getClockValues()
{
  minutesModule = Clock.getMinute();
  hoursModule = Clock.getHour(hourformat , PM);
  secondsModule = Clock.getSecond();
  //Serial.print("Time : "); Serial.print(hoursModule); Serial.print(":"); Serial.print(minutesModule); Serial.print(":"); Serial.println(secondsModule);
}

void loop() {
  //Function polling clock Module values
  getClockValues();

  //Function controlling the motor
  motorControl();

  //Getting raw IR Data
  if(!manualMovement)
  {
    updateIRValues();
  }

  //IR Logic
  createStates();
  statemachine();
  logicTrigger();

  //Display Functions
  setInfoRow();
  lcd_key = read_LCD_buttons();  // read the buttons
  keytrigger();
}


void updateTimeReached()
{
	if (timeSet && clockSet) 
	{
		if (hoursOpen == hoursModule && minutesOpen == minutesModule)
		{
      if(doorUp)
      {
        openingTimeReached = false;
        closingTimeReached = false;
      }
      else
      {
        //Serial.println("Opening Time Reached");
  			openingTimeReached = true;
  			closingTimeReached = false;
      }
		}
		else if (hoursClose == hoursModule && minutesClose == minutesModule)
		{
			openingTimeReached = false;
			closingTimeReached = true;
		}
		else
		{
			openingTimeReached = false;
			closingTimeReached = false;
		}
	}
}

void motorControl()
{
  doorUp = limitSwitchTriggeredTop(); //read limitSwitch
  
  if (!manualMovement)
  {
    //Stopping Conditions
    if (doorLowering)
    {
      if (state != 0)
      {
        moveMotor('s');
        Serial.println("´MOTOR CONTROL: WARNING Time Reached but Object in barrier");
      }
      if (loweringDurationReached())
      {
        moveMotor('s');
        Serial.println("´MOTOR CONTROL: door stopping downwards(endtime reached)");
      }
    }
    if (doorRaising)
    {
      if (doorUp)
      {
        moveMotor('s');
        Serial.println("´MOTOR CONTROL: stopping motor because door already at the top position");
      }
    }

	  if (timeSet && clockSet) 
	  {
		  updateTimeReached();
		  if (openingTimeReached)
		  {
        if(!doorUp)
        {
  			  moveMotor('u');
  			  Serial.println("MOTOR CONTROL: raising door");
        }
		  }
		  else if (closingTimeReached)
		  {
        if(chickencounter >= maxChicken)
        {
          if(state == 0)
          {
    		    moveMotor('d');
    		    Serial.println("´MOTOR CONTROL: lowering motor because time was reached and all Chicken in coop");
          }
        }
        else
        {
          Serial.println("´MOTOR CONTROL: WARNING Time Reached but not all Chicken in Coop");
        }
		  }
	  }
  }
  else
  {
	  if (doorLowering)
	  {
		  moveMotor('d');
		  Serial.println("´MOTOR CONTROL: lowering motor");
	  }
	  else if (doorRaising)
	  {
      if(limitSwitchTriggeredTop())
      {
        moveMotor('s');
      }
      else
      {
		    moveMotor('u');
		    Serial.println("´MOTOR CONTROL: raising motor");
      }
	  }
	  else
	  {
		  moveMotor('s');
	  }
  }
}

void updateIRValues()
{
  /* FRONT/BACK BARRIER RX/TX START*/
  // Creating Offset Value
  if(analogIR)
  {
    analogWrite(txPinIRFront, 0);
    analogWrite(txPinIRBack, 0);
  }
  else
  {
    digitalWrite(txPinIRFront, LOW);
    digitalWrite(txPinIRBack, LOW);
  }
  delay(VAL_DELAY);
  VAL_RECV_OFFSET_FRONT = analogRead(rxPinIRFront);
  VAL_RECV_OFFSET_BACK = analogRead(rxPinIRBack);

  //Reading High Value
  if(analogIR)
  {
    analogWrite(txPinIRFront, 255);
    analogWrite(txPinIRBack, 255);
  }
  else
  {
    digitalWrite(txPinIRFront, HIGH);
    digitalWrite(txPinIRBack, HIGH);
  }
  delay(VAL_DELAY);
  VAL_RECV_HIGH_FRONT = analogRead(rxPinIRFront);
  VAL_RECV_HIGH_BACK = analogRead(rxPinIRBack);

  //Calculate Value Difference
  VAL_DIFF_FRONT = VAL_RECV_HIGH_FRONT - VAL_RECV_OFFSET_FRONT;
  VAL_DIFF_BACK = VAL_RECV_HIGH_BACK - VAL_RECV_OFFSET_BACK;
  
  if ( dataOutput == 'S') {
    Serial.print("Front IR Value: ");
    Serial.print(VAL_DIFF_FRONT);
    Serial.print(" Back IR Value: ");
    Serial.println(VAL_DIFF_BACK);
  }
  /* FRONT/BACK BARRIER RX/TX END */
}

void createStates()
{
  /* FRONT BARRIER START*/
  //Front Barrier broken
  if ( VAL_DIFF_FRONT >= VAL_TRIGGER_FRONT)
  {
    //Increment amount of loops Barrier is broken
    trigCountFront++;
    //Stop trigCountFront from overflowing max int val
    if (trigCountFront > 50000) {
      trigCountFront = permittedFault + 1;
    }
    //Set frontTriggered if Barrier broken too long
    if (trigCountFront > permittedFault)
    {
      if (frontTriggered == false)
      {
        Serial.println("Object in Front Barrier");
      }
      //Serial.println(trigCountFront);
      frontTriggered = true;
    }
  }
  //Front Barrier not broken
  else if ( VAL_DIFF_FRONT < VAL_TRIGGER_FRONT)
  {
    //Reset all triggerrelated Variables if needed
    trigCountFront = 0;
    if ( frontTriggered == true)
    {
      Serial.println("Object left Front Barrier");
      frontTriggered = false;
    }
  }
  /* FRONT BARRIER END*/

  /* BACK BARRIER START*/
  //Back Barrier broken
  if ( VAL_DIFF_BACK >= VAL_TRIGGER_BACK)
  {
    //Increment amount of loops Barrier is broken
    trigCountBack++;
    //Stop trigCountBack from overflowing max int val
    if (trigCountBack > 50000) {
      trigCountBack = permittedFault + 1;
    }
    //Set backTriggered if Barrier broken too long
    if (trigCountBack > permittedFault)
    {
      if (backTriggered == false)
      {
        Serial.println("Object in Back Barrier");
      }
      //Serial.println(trigCountBack);
      backTriggered = true;
    }
  }
  //Back Barrier not broken
  else if ( VAL_DIFF_BACK < VAL_TRIGGER_BACK)
  {
    //Reset all triggerrelated Variables if needed
    trigCountBack = 0;
    if ( backTriggered == true)
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
  if (!frontTriggered && !backTriggered)
  {
    state = 0;
  }
  else if (frontTriggered && backTriggered)
  {
    state = 1;
  }
  else if (frontTriggered && !backTriggered)
  {
    state = 2;
  }
  else if (!frontTriggered && backTriggered)
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
  if (prevstate != state)
  {
    //Nothing prev
    if (prevstate == 0)
    {
      logiccount = 0;  //if previous state implies barrier up, reset the logiccount (i.e. UP-UP doesnt count towards logic tree, only the following decision does.}
      frontEntry = false;
      backEntry = false;
      if (state == 1)
      {
        Serial.println("WARNING: Object entered both Barriers too fast or simultaneously");
      }
      else if (state == 2)
      {
        frontEntry = true;
        //Serial.println("Object entered Front Barrier");
      }
      else if (state == 3)
      {
        backEntry = true;
        //Serial.println("Object entered Back Barrier");
      }
    }
    //front + back trig prev
    else if (prevstate == 1)
    {
      if (state == 0)
      {
        Serial.println("WARNING: Object left Barriers too fast or didn't leave through the barriers");
      }
      else if (state == 2)
      {
        if (logiccount == 1) {
          if (backEntry)
          {
            logiccount++;
          }
        }
      }
      else if (state == 3)
      {
        if (logiccount == 1) {
          if (frontEntry)
          {
            logiccount++;
          }
        }
      }
    }
    //frontTrig prev
    else if (prevstate == 2)
    {
      if (state == 0)
      {
        if (logiccount != 0) {
          logiccount++;
        }
      }
      else if (state == 1)
      {
        if (logiccount == 0) {
          logiccount++;
        }
      }
      else if (state == 3)
      {
        Serial.println("WARNING: (SKIPPED STATE 1) OBJECT TOO FAST OR 2 OBJECTS");
      }
    }
    //backtrig prev
    else if (prevstate == 3)
    {
      if (state == 0)
      {
        if (logiccount == 2) {
          logiccount++;
        }
      }
      else if (state == 1)
      {
        if (logiccount == 0) {
          logiccount++;
        }
      }
      else if (state == 2)
      {
        Serial.println("WARNING: (SKIPPED STATE 1) Object TOO FAST OR 2 OBJECTS");
      }
    }
    //Serial.println(logiccount);
  }
}

void logicTrigger()
{
  if (logiccount == 3)
  {
    logiccount = 0;
    if (frontEntry)
    {
      //CHICKEN PASSED THROUGH THE FRONT (I.E. +1 CHICKEN IN COOP)
      chickencounter++;
      Serial.println("Amount of Chickens in Coop:");
      Serial.println(chickencounter);
    }
    else if (backEntry)
    {
      //CHICKEN PASSED THROUGH THE BACK (I.E. -1 CHICKEN IN COOP)
      chickencounter--;
      Serial.println("Amount of Chickens in Coop:");
      Serial.println(chickencounter);
    }
    else if (!backEntry || !frontEntry)
    {
      Serial.println("WARNING: LOGIC TREE FAULTY");
    }
  }
}

void setInfoRow()
{
  lcd.setCursor(0, 0);

  if (lcdmenu == 0)
  {
    if (!timeSet)
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
    else {
      lcd.print("Close/Open Time:  ");
    }
  }
  else if (lcdmenu == 1)
  {
    if (!clockSet)
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
    else {
      lcd.print("CLOCK IS SET :          ");
    }
  }
  else if (lcdmenu == 2)
  {
    if (!chickenSet)
    {
      lcd.print("Chicken Amount?");
    }
    else {
      lcd.print("Chicken Set    ");
    }
  }
  if (lcdmenu == 3)
  {
    if(!manualMovement)
    {
      lcd.print("Press Select       ");
    }
    else
    {
      lcd.print("Manual MotorMov  ");
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
  if (adc_key_in < 70)   return btnRIGHT;
  if (adc_key_in < 180)  return btnUP;
  if (adc_key_in < 350)  return btnDOWN;
  if (adc_key_in < 500)  return btnLEFT;
  if (adc_key_in < 750)  return btnSELECT;

  return btnNONE;  // default value
}

void keytrigger()
{
  lcd.setCursor(0, 1);
  if (cursorposition < MIN_DISPLAY_LENGTH) {
    cursorposition = MIN_DISPLAY_LENGTH;
  }
  if (cursorposition >= MAX_DISPLAY_LENGTH) {
    cursorposition = MAX_DISPLAY_LENGTH;
  }
  switch (lcd_key)               // depending on which button was pushed, perform an action
  {
    case btnRIGHT:
      {
        if (changeMenu)
        {
          lcdmenu++;
          if (lcdmenu >= MAX_MENU) {
            lcdmenu = MAX_MENU;
          }
          menustate();
          delay(500);
        }
        else
        {
          //change cursor to right position unless already to the rightmost position
          //lcd.print("Cursor to RIGHT");
          lcd.setCursor(cursorposition, 1);
          cursorposition++;
          if (cursorposition >= MAX_DISPLAY_LENGTH) {
            cursorposition = MAX_DISPLAY_LENGTH;
          }
          lcd.setCursor(cursorposition, 1);
        }
        break;
      }
    case btnLEFT:
      {
        if (changeMenu)
        {
          lcdmenu--;
          if (lcdmenu <= 0) {
            lcdmenu = 0;
          }
          menustate();
          delay(500);
          break;
        }
        else
        {
          //change cursor to left position unless already to the leftmost position
          lcd.setCursor(cursorposition, 1);
          cursorposition--;
          if (cursorposition < MIN_DISPLAY_LENGTH) {
            cursorposition = MIN_DISPLAY_LENGTH;
          }
          lcd.setCursor(cursorposition, 1);
          break;
        }
      }
    case btnUP:
      {
        switch(lcdmenu)
        {
          case 0:
          {
            if (!timeSet)
            {
              //Increment clock value at selected position
              lcd.setCursor(cursorposition, 1);
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
            break;
          }
          case 1:
          {
            //Decrement clock value at selected position
            lcd.setCursor(cursorposition, 1);
            if (cursorposition >= 0 && cursorposition <= 7)
            {
              hoursClock++;
            }
            else if (cursorposition > 7 && cursorposition <= 15)
            {
              minutesClock++;
            }
            break;
          }
          case 2:
          {
            maxChicken++;
            if (maxChicken >= 99) {
              maxChicken = 99;
            }
            delay(500);
            break;
          }
          case 3:
          {
            if(manualMovement)
            {
              if(!doorRaising)
              {
                doorRaising = true;
                doorLowering = false;
                Serial.println("SET doorRaising true");
              }
            }
            break;
          }
        }
        setClockValues();
        break;
      }
    case btnDOWN:
      {
        switch(lcdmenu)
        {
          case 0:
          {
            //Decrement clock value at selected position
            lcd.setCursor(cursorposition, 1);
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
            break;
          }
          case 1:
          {
            //Decrement clock value at selected position
            lcd.setCursor(cursorposition, 1);
            if (cursorposition >= 0 && cursorposition <= 7)
            {
              hoursClock--;
            }
            else if (cursorposition > 7 && cursorposition <= 15)
            {
              minutesClock--;
            }
            break;
          }
		  //Decrement max chicken value
          case 2:
          {
            maxChicken--;
            if (maxChicken <= 0) {
              maxChicken = 0;
            }
            delay(500);
            break;
          }
          case 3:
          {
			//CREATe DOOR STATES MANUALLY
            if(manualMovement)
            {
              if(!doorLowering)
              {
                doorRaising = false;
                doorLowering = true;
                Serial.println("SET doorLowering true");
              }
            }
            break;
          }
        }
        setClockValues();
        break;
      }
    case btnSELECT:
      {
        switch(lcdmenu)
        {
          case 0:
          {
            if (timeSet) {
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
            break;
          }
          case 1:
          {
             if (clockSet) {
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
            break;
          }
          case 2:
          {
            if (chickenSet) {
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
            break;
          }
          case 3:
          {
            if (manualMovement) 
            {
              changeMenu = true;
              manualMovement = false;
			        doorLowering = false;
			        doorRaising = false;
              delay(1000);
            }
            else
            {
              manualMovement = true;
              changeMenu = false;
              delay(1000);
            }
            break;
          }
        }
        break;
      }
    case btnNONE:
      {
        setClockValues();

        if(lcdmenu == 3)
        {
          doorLowering = false;
          doorRaising = false;
        }
        
        break;
      }
  }
}

void setClockValues() {
  //Correcting faulty values
  if (minutesClose > 59) {
    hoursClose++;
    minutesClose = 0;
  }
  if (minutesClose < 0) {
    hoursClose--;
    minutesClose = 59;
  }
  if (hoursClose > 23) {
    hoursClose = 0;
  }
  if (hoursClose < 0) {
    hoursClose = 23;
  }

  if (minutesOpen > 59) {
    hoursOpen++;
    minutesOpen = 0;
  }
  if (minutesOpen < 0) {
    hoursOpen--;
    minutesOpen = 59;
  }
  if (hoursOpen > 23) {
    hoursOpen = 0;
  }
  if (hoursOpen < 0) {
    hoursOpen = 23;
  }

  if (minutesClock > 59) {
    hoursClock++;
    minutesClock = 0;
  }
  if (minutesClock < 0) {
    hoursClock--;
    minutesClock = 59;
  }
  if (hoursClock > 23) {
    hoursClock = 0;
  }
  if (hoursClock < 0) {
    hoursClock = 23;
  }

  tempcursor = 0;
  switch (lcdmenu)
  {
	  case 0:
	  {
		  /*SETTING VALUES CLOSING TIME */
		  lcd.setCursor(tempcursor, 1);
		  if (hoursClose < 10 && hoursClose > -1)
		  {
			  lcd.print("0");
			  tempcursor++;
			  lcd.setCursor(tempcursor, 1);
			  lcd.print(clockArray[hoursClose][0]);
			  tempcursor++;
		  }
		  else
		  {
			  lcd.print(clockArray[hoursClose][0]);
			  tempcursor += 2;
		  }

		  lcd.setCursor(tempcursor, 1);
		  lcd.print(":");
		  tempcursor++;
		  lcd.setCursor(tempcursor, 1);
		  if (minutesClose < 10)
		  {
			  lcd.print("0");
			  tempcursor++;
			  lcd.setCursor(tempcursor, 1);
			  lcd.print(clockArray[0][minutesClose]);
			  tempcursor++;
		  }
		  else
		  {
			  lcd.print(clockArray[0][minutesClose]);
			  tempcursor += 2;
		  }
		  lcd.print("cl");
		  tempcursor += 2;
		  lcd.print("  ");
		  tempcursor += 2;

		  /*SETTING VALUES OPENING TIME */
		  lcd.setCursor(tempcursor, 1);
		  if (hoursOpen < 10 && hoursOpen > -1)
		  {
			  lcd.print("0");
			  tempcursor++;
			  lcd.setCursor(tempcursor, 1);
			  lcd.print(clockArray[hoursOpen][0]);
			  tempcursor++;
		  }
		  else
		  {
			  lcd.print(clockArray[hoursOpen][0]);
			  tempcursor += 2;
		  }
		  lcd.setCursor(tempcursor, 1);
		  lcd.print(":");
		  tempcursor++;
		  lcd.setCursor(tempcursor, 1);
		  if (minutesOpen < 10)
		  {
			  lcd.print("0");
			  tempcursor++;
			  lcd.setCursor(tempcursor, 1);
			  lcd.print(clockArray[0][minutesOpen]);
			  tempcursor++;
		  }
		  else
		  {
			  lcd.print(clockArray[0][minutesOpen]);
			  tempcursor += 2;
		  }
		  lcd.print("op");
		  tempcursor += 2;
		  break;
	  }
	  case 1:
	  {
		  lcd.setCursor(tempcursor, 1);
		  if (hoursClock < 10 && hoursClock > -1)
		  {
			  lcd.print("0");
			  tempcursor++;
			  lcd.setCursor(tempcursor, 1);
			  lcd.print(clockArray[hoursClock][0]);
			  tempcursor++;
		  }
		  else
		  {
			  lcd.print(clockArray[hoursClock][0]);
			  tempcursor += 2;
		  }
		  lcd.setCursor(tempcursor, 1);
		  lcd.print(":");
		  tempcursor++;
		  lcd.setCursor(tempcursor, 1);
		  if (minutesClock < 10)
		  {
			  lcd.print("0");
			  tempcursor++;
			  lcd.setCursor(tempcursor, 1);
			  lcd.print(clockArray[0][minutesClock]);
			  tempcursor++;
		  }
		  else
		  {
			  lcd.print(clockArray[0][minutesClock]);
			  tempcursor += 2;
		  }
		  lcd.print("   ");
		  tempcursor += 3;
		  if (hoursModule < 10 && hoursModule > -1)
		  {
			  lcd.print("0");
			  tempcursor++;
			  lcd.setCursor(tempcursor, 1);
			  lcd.print(hoursModule);
			  tempcursor++;
		  }
		  else
		  {
			  lcd.print(clockArray[hoursModule][0]);
			  tempcursor += 2;
		  }
		  lcd.setCursor(tempcursor, 1);
		  lcd.print(":");
		  tempcursor++;
		  lcd.setCursor(tempcursor, 1);

		  if (minutesModule < 10)
		  {
			  lcd.print("0");
			  tempcursor++;
			  lcd.setCursor(tempcursor, 1);
			  lcd.print(clockArray[0][minutesModule]);
			  tempcursor++;
		  }
		  else
		  {
			  lcd.print(clockArray[0][minutesModule]);
			  tempcursor += 2;
		  }

		  lcd.setCursor(tempcursor, 1);
		  lcd.print(":");
		  tempcursor++;
		  lcd.setCursor(tempcursor, 1);

		  if (secondsModule < 10)
		  {
			  lcd.print("0");
			  tempcursor++;
			  lcd.setCursor(tempcursor, 1);
			  lcd.print(clockArray[0][secondsModule]);
			  tempcursor++;
		  }
		  else
		  {
			  lcd.print(clockArray[0][secondsModule]);
			  tempcursor += 2;
		  }
		  break;
	  }
	  case 2:
	  {
		  lcd.setCursor(tempcursor, 1);
		  if (maxChicken < 10 && maxChicken > -1)
		  {
			  lcd.print("0");
			  tempcursor++;
			  lcd.setCursor(tempcursor, 1);
			  lcd.print(maxChicken);
			  tempcursor++;
		  }
		  else
		  {
			  lcd.print(maxChicken);
			  tempcursor += 2;
		  }
		  lcd.print("                ");
		  break;
	  }
	  case 3:
	  {
		  lcd.setCursor(tempcursor, 1);
		  if(doorLowering)
      {
        lcd.print("Door is lowering      ");
      }
      else if(doorRaising)
      {
        lcd.print("Door is raising       ");
      }
      else
      {
        lcd.print("Door is stopped          ");
      }
		  break;
	  }
  }
}

void moveMotor(char command) {
  if (command == 'u') {
    //Set relais to go up
    digitalWrite(relaisA, HIGH);
    digitalWrite(relaisB, LOW);
    doorLowering = false;
    doorRaising = true;
  }
  else if (command == 'd') {
    //set relais to go down
    digitalWrite(relaisA, LOW);
    digitalWrite(relaisB, HIGH);
    doorLowering = true;
    doorRaising = false;
  }
  else if (command == 's') {
    //Set relais to stop
    digitalWrite(relaisA, LOW);
    digitalWrite(relaisB, LOW);
    doorLowering = false;
    doorRaising = false;
  }
}

boolean limitSwitchTriggeredTop() {
  boolean triggered = false;
  limitSwitchTopValue = digitalRead(limitSwitchTop);
  if (limitSwitchTopValue == 0)
  {
    limitSwitchTopValue = 1;
  }
  else if(limitSwitchTopValue ==1)
  {
    limitSwitchTopValue =0;
  }
  //Serial.println(limitSwitchTopValue);
  
  if (limitSwitchTopValue >= LIMIT_SWITCH_TRIGGER_VALUE)
  {
	  triggered = true;
  }
  else
  {
	  triggered = false;
  }
  return triggered;
}

void correctingInitialClockValues() {
  if (minutesClose == 60) {
    minutesClose = 0;
  }
  if (hoursClose == 24) {
    hoursClose = 0;
  }
  if (minutesOpen == 60) {
    minutesOpen = 0;
  }
  if (hoursOpen == 24) {
    hoursOpen = 0;
  }
  if (minutesClock == 60) {
    minutesOpen = 0;
  }
  if (hoursClock == 24) {
    hoursOpen = 0;
  }
}

void initClockArray() {
  for (int h = 0; h < 24; h++) {
    clockArray[h][0] = h;
  }
  for (int m = 0; m < 60; m++) {
    clockArray[0][m] = m;
  }
}

void menustate() {
  if (lcdmenu == 0 && timeSet == false) {
    changeMenu = false;
  } else if (lcdmenu == 0 && timeSet == true) {
    changeMenu = true;
  }
  if (lcdmenu == 1 && clockSet == false) {
    changeMenu = false;
  } else if (lcdmenu == 0 && clockSet == true) {
    changeMenu = true;
  }
  if (lcdmenu == 2 && chickenSet == false) {
    changeMenu = false;
  } else if (lcdmenu == 0 && chickenSet == true) {
    changeMenu = true;
  }
  if (lcdmenu == 3 && manualMovement == false) {
    changeMenu = true;
  } else if (lcdmenu == 3 && manualMovement == true) {
    changeMenu = false;
  }
}

boolean loweringDurationReached()
{
  boolean reached = false;
  
  if (startLoweringCountdown == false) //set stuff only at the first call until the duration is reached
  {
    startLoweringCountdown = true;
  }

  if(startLoweringCountdown)
  {
    if(getCurrentDoorClosingDuration()  >= DOOR_CLOSING_DURATION)
    {
      reached = true;
      seconds = 61;
      prevseconds = 61;
      layoverseconds = 0;
      startLoweringCountdown = false;
    }
  }
  return reached;
}

int getCurrentDoorClosingDuration()
{
  seconds = Clock.getSecond();
  if(seconds != prevseconds)
  {
    layoverseconds++;
  }
  prevseconds = seconds;
  return layoverseconds;
}

