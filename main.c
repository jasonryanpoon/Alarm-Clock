//This program uses the RTC to display time on a 4 digit 7 segment display
//When the alarm triggers, it plays mp3 files through a USB connected on the micro USB port

#include "stm32f4xx_rtc.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_pwr.h"
#include "stm32f4xx_dac.h"
#include "stm32f4xx_tim.h"
#include "misc.h"
#include "stm32f4xx_exti.h"
#include "audioMP3.h"
#include "timeKeeping.h"
#include "main.h"

//structures
RCC_ClocksTypeDef RCC_Clocks;
GPIO_InitTypeDef	GPIOInitStruct;
TIM_TimeBaseInitTypeDef TIM_InitStruct;
NVIC_InitTypeDef NVIC_InitStructure;
EXTI_InitTypeDef EXTI_InitStructure;
I2C_InitTypeDef I2C_InitStruct;


//function prototypes
void configuration(void);
void Output_Segment(int number);
void display7Seg(uint8_t h, uint8_t m);
void setTime(uint8_t h, uint8_t m, uint8_t AMPM);
void setAlarm(uint8_t h, uint8_t m, uint8_t AMPMs);
void snooze(void);
void getCurrentTime(void);
void set24Hour(void);
void buttonControls(void);


//global variables
int interruptOccurred = 0;
int hour24Flag = 0;
unsigned int debouncing = 0;
int buttonState = 0;
int buttonFlag = 0;
int currentmode = 0;
int modeon = 0;
int setmin = 0;
int displayalarm = 0;
int prevtime = 0;
int newtime = 0;
int prevhour = 0;
int newhour = 0;
int snoozeflag = 0;
int snoozedelay = 0;

int main(void)
{

	configuration();

  // This flag should be set to zero AFTER you have debugged your MP3 player code
  // In your final code, this will be set to 1 when the alarm time is reached in the
  // RTC_ALARM_IRQHandler function.
  // It is set to one here for testing purposes only.
  interruptOccurred = 0;
  hour24Flag = 0;
  //Continuously loops checking for the alarm interrupt
  while(1)
  {
	//	 mp3PlayingFlag = 1;
	//	 audioToMp3();

	  //Checks for alarm A interrupt and calls the mp3 function
	  if(1 == interruptOccurred && mp3PlayingFlag == 0)
	  {

		 interruptOccurred = 0;
		 mp3PlayingFlag = 1;
		 audioToMp3();
	  }

	  //Ensures the alarm is not already playing
	  else if(1 == interruptOccurred && mp3PlayingFlag == 1)
	  {
		 interruptOccurred = 0;
	  }

  }

}

//Timer interrupt handler that is called at a rate of 500Hz
//This function gets the time and displays it on the 7 segment display
//It also checks for button presses, debounces, and handles each case
void TIM5_IRQHandler(void)
{
	int previousState = 0;

	//Checks that interrupt has occurred
	if( TIM_GetITStatus( TIM5, TIM_IT_Update ) != RESET )
	{
		getCurrentTime();

		if((snoozeflag == 1)) //If the snooze button was pressed, wait then use the snooze function
			snoozedelay++;	//Increment the delay counter
		if((snoozeflag == 1) && (snoozedelay > 500)){ //If increment counter complete
			snooze();
			snoozeflag = 0; //Reset
			snoozedelay = 0; //Reset
		}

		if(1 == hour24Flag) set24Hour();//Displays as either 24 or 12 hour

		newhour = myclockTimeStruct.RTC_Hours; //Holds the current hour
		if((prevhour != newhour) && (currentmode != 2)) timeHourCheck(); //Checks the hour if it changes
		prevhour = newhour; //Holds the previous hour

		if((displayalarm == 1))//If in AlarmSet mode, display the alarm
			display7Seg(AlarmStruct.RTC_AlarmTime.RTC_Hours, AlarmStruct.RTC_AlarmTime.RTC_Minutes);
		else	// Otherwise, display the time
			display7Seg(myclockTimeStruct.RTC_Hours, myclockTimeStruct.RTC_Minutes);

		buttonControls();


		/*MODES: The three modes of the alarm clock.
		 *
		 *12 OR 24 HOUR MODE: Switches between 12 and 24 hour clock for both current time
		 *and alarm time.
		 *
		 *SET TIME MODE: Sets the hour and the minutes using the UP and DOWN buttons.
		 *
		 *SET ALARM MODE: Set the hour and minutes of the alarm using the UP and DOWN buttons.
		 */
		if((modeon == 1)) // If the mode menu is toggled on
		{
			//12 OR 24 HOUR MODE
			if((buttonFlag == 1) && (buttonState == SELECT) && (currentmode == 1)) //Select is pressed
			{
				if(0 == hour24Flag)
					hour24Flag = 1; //Dets a flag for the display function to look for
				else
					hour24Flag = 0;

				buttonFlag = 0; //Resets flag
			}// 24 or 12 HOUR MODE


			//SET TIME MODE
			if(currentmode == 2){
				//SELECT Min or Hour change
				if((buttonFlag == 1) && (buttonState == SELECT)) //If the select button is pressed
				{
					if(setmin == 0) //setmin flag to indicate min change or hour change
						setmin = 1;
					else
						setmin = 0;

					buttonFlag = 0;
				}
				else {

					//Increments minute
					if((buttonFlag == 1) && (buttonState == UP) && (setmin == 1))
					{
						if((myclockTimeStruct.RTC_Minutes & 0x0F) == 0x0F) //Accounts for Hex conversion
							setTime(myclockTimeStruct.RTC_Hours, myclockTimeStruct.RTC_Minutes + 0x07, myclockTimeStruct.RTC_H12);
						else if(myclockTimeStruct.RTC_Minutes >= 89) //If greater than 59mins, goto 0mins
							setTime(myclockTimeStruct.RTC_Hours, 0x00, myclockTimeStruct.RTC_H12);
						else //Otherwise, add a minute
							setTime(myclockTimeStruct.RTC_Hours , myclockTimeStruct.RTC_Minutes + 0x01, myclockTimeStruct.RTC_H12);
						buttonFlag = 0;
					}

					//Decrements minute
					if((buttonFlag == 1) && (buttonState == DOWN) && (setmin == 1))
					{
						if(((myclockTimeStruct.RTC_Minutes & 0x0F) == 0x06) && myclockTimeStruct.RTC_Minutes > 0x10) //Accounts for Hex conversion
							setTime(myclockTimeStruct.RTC_Hours, myclockTimeStruct.RTC_Minutes - 0x07, myclockTimeStruct.RTC_H12);
						else if((myclockTimeStruct.RTC_Minutes & 0x0F) == 0x00) //If less than 0mins, goto 59mins
							setTime(myclockTimeStruct.RTC_Hours, 0x59, myclockTimeStruct.RTC_H12);
						else //Otherwise, add a minute
							setTime(myclockTimeStruct.RTC_Hours, myclockTimeStruct.RTC_Minutes - 0x01, myclockTimeStruct.RTC_H12);
						buttonFlag = 0;
					}

					//Increments hour
					if((buttonFlag == 1) && (buttonState == UP))
					{
						if((myclockTimeStruct.RTC_Hours & 0x0F) == 0x0F)
							setTime(myclockTimeStruct.RTC_Hours + 0x07, myclockTimeStruct.RTC_Minutes, myclockTimeStruct.RTC_H12);
						//If up from 24 hr, (24:00 -> 00:00)
						else if((myclockTimeStruct.RTC_Hours == 0x23) && (myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_24))
							setTime(0x00, myclockTimeStruct.RTC_Minutes, myclockTimeStruct.RTC_H12);
						//If up from 12 hr, (12:00 -> 1:00)
						else if((myclockTimeStruct.RTC_Hours >= 0x0C) && (myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_12))
							setTime(0x01, myclockTimeStruct.RTC_Minutes, myclockTimeStruct.RTC_H12);
						//If up from 12 hr, (11:00PM -> 12:00AM)
						else if(((myclockTimeStruct.RTC_Hours & 0x0F) == 0x0B) && (myclockTimeStruct.RTC_H12 == RTC_H12_PM))
							setTime(myclockTimeStruct.RTC_Hours + 0x01, myclockTimeStruct.RTC_Minutes, RTC_H12_AM);
						//If up from 12 hr, (11:00AM -> 12:00PM)
						else if(((myclockTimeStruct.RTC_Hours & 0x0F) == 0x0B) && (myclockTimeStruct.RTC_H12 == RTC_H12_AM))
							setTime(myclockTimeStruct.RTC_Hours + 0x01, myclockTimeStruct.RTC_Minutes, RTC_H12_PM);
						else //Otherwise, add a minute
							setTime(myclockTimeStruct.RTC_Hours + 0x01, myclockTimeStruct.RTC_Minutes, myclockTimeStruct.RTC_H12);
						buttonFlag = 0;
					}

					//Decrements hour
					if((buttonFlag == 1) && (buttonState == DOWN))
					{
						if(((myclockTimeStruct.RTC_Hours & 0x0F) == 0x06) && myclockTimeStruct.RTC_Hours > 0x10)
							setTime(myclockTimeStruct.RTC_Hours - 0x07, myclockTimeStruct.RTC_Minutes, myclockTimeStruct.RTC_H12);
						//If down from 24 hr, (24:00 <- 00:00)
						else if(((myclockTimeStruct.RTC_Hours & 0x0F) == 0x00) && (myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_24))
							setTime(0x1D, myclockTimeStruct.RTC_Minutes, myclockTimeStruct.RTC_H12);
						//If down from 12 hr, (12:00 <- 1:00)
						else if(((myclockTimeStruct.RTC_Hours & 0x0F) == 0x01) && (myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_12))
							setTime(0x0C, myclockTimeStruct.RTC_Minutes, myclockTimeStruct.RTC_H12);
						//If up from 12 hr, (11:00PM <- 12:00AM)
						else if(((myclockTimeStruct.RTC_Hours & 0x0F) == 0x0C) && (myclockTimeStruct.RTC_H12 == RTC_H12_AM))
							setTime(myclockTimeStruct.RTC_Hours - 0x01, myclockTimeStruct.RTC_Minutes, RTC_H12_PM);
						//If up from 12 hr, (11:00AM <- 12:00PM)
						else if(((myclockTimeStruct.RTC_Hours & 0x0F) == 0x0C) && (myclockTimeStruct.RTC_H12 == RTC_H12_PM))
							setTime(myclockTimeStruct.RTC_Hours - 0x01, myclockTimeStruct.RTC_Minutes, RTC_H12_AM);
						else //Otherwise, add a minute
							setTime(myclockTimeStruct.RTC_Hours - 0x01, myclockTimeStruct.RTC_Minutes, myclockTimeStruct.RTC_H12);
						buttonFlag = 0;
					}//Down hour
				}//Increment/decrement min and hour
			}//Set Time

			//SET Alarm
			if(currentmode == 3)
			{
				displayalarm = 1; //Set flag to display the alarm time
				if((buttonFlag == 1) && (buttonState == SELECT)) //If the select button is pushed
				{
					if(setmin == 0) //Toggle setting the minute or setting the hour
						setmin = 1;
					else
						setmin = 0;

					buttonFlag = 0;
					}
				else {

					if(0 != (0x100 & RTC->CR)) //Checks if alarm was on or off
					{
						previousState = 1;
					}

					//Increment min alarm
					if((buttonFlag == 1) && (buttonState == UP) && (setmin == 1))
					{
						if((AlarmStruct.RTC_AlarmTime.RTC_Minutes & 0x0F) == 0x0F) //Accounts for Hex conversion
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours, AlarmStruct.RTC_AlarmTime.RTC_Minutes + 0x07, AlarmStruct.RTC_AlarmTime.RTC_H12);
						else if(AlarmStruct.RTC_AlarmTime.RTC_Minutes >= 89) //If greater than 59, goto 00
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours, 0x00, AlarmStruct.RTC_AlarmTime.RTC_H12);
						else //Otherwise, add one to the minutes
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours, AlarmStruct.RTC_AlarmTime.RTC_Minutes + 0x01, AlarmStruct.RTC_AlarmTime.RTC_H12);

						buttonFlag = 0;

					}

					//Decrement min alarm
					if((buttonFlag == 1) && (buttonState == DOWN) && (setmin == 1))
					{
						if(((AlarmStruct.RTC_AlarmTime.RTC_Minutes & 0x0F) == 0x06) && AlarmStruct.RTC_AlarmTime.RTC_Minutes > 0x10)
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours, AlarmStruct.RTC_AlarmTime.RTC_Minutes - 0x07, AlarmStruct.RTC_AlarmTime.RTC_H12);
						else if((AlarmStruct.RTC_AlarmTime.RTC_Minutes & 0x0F) == 0x00) //If less than 0mins, goto 59mins
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours, 0x59, AlarmStruct.RTC_AlarmTime.RTC_H12);
						else
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours, AlarmStruct.RTC_AlarmTime.RTC_Minutes - 0x01, AlarmStruct.RTC_AlarmTime.RTC_H12);
						buttonFlag = 0;
					}

					//Increment hour alarm
					if((buttonFlag == 1) && (buttonState == UP))
					{
						if((AlarmStruct.RTC_AlarmTime.RTC_Hours & 0x0F) == 0x0F)
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours + 0x07, AlarmStruct.RTC_AlarmTime.RTC_Minutes, AlarmStruct.RTC_AlarmTime.RTC_H12);
						//If up from 24 hr, (24:00 -> 00:00)
						else if((AlarmStruct.RTC_AlarmTime.RTC_Hours == 0x1D) && (myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_24))
							setAlarm(0x00, AlarmStruct.RTC_AlarmTime.RTC_Minutes, AlarmStruct.RTC_AlarmTime.RTC_H12);
						//If up from 12 hr, (12:00 -> 1:00)
						else if(((AlarmStruct.RTC_AlarmTime.RTC_Hours & 0x0F) == 0x0C) && (myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_12))
							setAlarm(0x01, AlarmStruct.RTC_AlarmTime.RTC_Minutes, AlarmStruct.RTC_AlarmTime.RTC_H12);
						//If up from 12 hr, (11:00PM -> 12:00AM)
						else if((AlarmStruct.RTC_AlarmTime.RTC_Hours == 0x0B) && (AlarmStruct.RTC_AlarmTime.RTC_H12 == RTC_H12_PM))
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours + 0x01, AlarmStruct.RTC_AlarmTime.RTC_Minutes, RTC_H12_AM);
						//If up from 12 hr, (11:00AM -> 12:00PM)
						else if((AlarmStruct.RTC_AlarmTime.RTC_Hours == 0x0B) && (AlarmStruct.RTC_AlarmTime.RTC_H12 == RTC_H12_AM))
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours + 0x01, AlarmStruct.RTC_AlarmTime.RTC_Minutes, RTC_H12_PM);
						else //Otherwise, increase the hour by one
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours + 0x01, AlarmStruct.RTC_AlarmTime.RTC_Minutes, AlarmStruct.RTC_AlarmTime.RTC_H12);

						buttonFlag = 0;

					}

					//Decrement hour alarm
					if((buttonFlag == 1) && (buttonState == DOWN))
					{
						if(((AlarmStruct.RTC_AlarmTime.RTC_Hours & 0x0F) == 0x06) && AlarmStruct.RTC_AlarmTime.RTC_Hours > 0x10)//Accounts for Hex conversion
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours - 0x07, AlarmStruct.RTC_AlarmTime.RTC_Minutes, AlarmStruct.RTC_AlarmTime.RTC_H12);
						//If down from 24 hr, (24:00 <- 00:00)
						else if(((AlarmStruct.RTC_AlarmTime.RTC_Hours & 0x0F) == 0x00) && (myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_24))
							setAlarm(0x1D, AlarmStruct.RTC_AlarmTime.RTC_Minutes, AlarmStruct.RTC_AlarmTime.RTC_H12);
						//If down from 12 hr, (12:00 <- 1:00)
						else if((AlarmStruct.RTC_AlarmTime.RTC_Hours == 0x01) && (myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_12))
							setAlarm(0x0C, AlarmStruct.RTC_AlarmTime.RTC_Minutes, AlarmStruct.RTC_AlarmTime.RTC_H12);
						//If up from 12 hr, (11:00PM <- 12:00AM)
						else if((AlarmStruct.RTC_AlarmTime.RTC_Hours == 0x0C) && (AlarmStruct.RTC_AlarmTime.RTC_H12 == RTC_H12_AM))
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours - 0x01, AlarmStruct.RTC_AlarmTime.RTC_Minutes, RTC_H12_PM);
						//If up from 12 hr, (11:00AM <- 12:00PM)
						else if((AlarmStruct.RTC_AlarmTime.RTC_Hours == 0x0C) && (AlarmStruct.RTC_AlarmTime.RTC_H12 == RTC_H12_PM))
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours - 0x01, AlarmStruct.RTC_AlarmTime.RTC_Minutes, RTC_H12_AM);
						else // Otherwise, decrement the hour by one
							setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours - 0x01, AlarmStruct.RTC_AlarmTime.RTC_Minutes, AlarmStruct.RTC_AlarmTime.RTC_H12);

						buttonFlag = 0;

					}

					if(1==previousState) //If alarm was on, re-enable it
					{
						RTC_AlarmCmd(RTC_Alarm_A,ENABLE);
						RTC_ClearFlag(RTC_FLAG_ALRAF);
						previousState = 0;
					}
				}//Increment/Decrement min hour alarm
			}//Set Alarm
		}//Mode on

		//If snooze button is pressed cancels the mp3 and sets alarm 10 minutes later
		if((buttonFlag == 1) && (buttonState == SNOOZE))
		{
			if(mp3PlayingFlag == 1)
			{
			  exitMp3 = 1;
			  snoozeflag = 1;
			}
			buttonFlag = 0;
		}

		//If the reset button is pressed without mp3 playing, it turns off and on the alarm
		//Otherwise it cancels the mp3
		if((buttonFlag == 1) && (buttonState == SELECT))
		{
			if(mp3PlayingFlag == 0)  //If not playing
			{

				//toggle alarm
				if(0 != (0x100 & RTC->CR))  //If alarm is on
				{
					RTC_AlarmCmd(RTC_Alarm_A,DISABLE);

				}

				//If alarm is off
				else
				{
					RTC_AlarmCmd(RTC_Alarm_A,ENABLE);
					RTC_ClearFlag(RTC_FLAG_ALRAF);
				}
			 }else
			 {
				 exitMp3 = 1;
			 }

			 //Checks if snooze has been used before, resets the alarm to original time
			 if(1 == snoozeMemory)
			 {
				 AlarmStruct.RTC_AlarmTime = alarmMemory.RTC_AlarmTime;
				 RTC_SetAlarm(RTC_Format_BCD,RTC_Alarm_A,&AlarmStruct);
				 snoozeMemory = 0;
			 }
			 buttonFlag = 0;
		 }
	     //Clears interrupt flag
	     TIM5->SR = (uint16_t)~TIM_IT_Update;
    }
}

//Alarm A interrupt handler
//When alarm occurs, clear all the interrupt bits and flags
//Then set the flag to play mp3
void RTC_Alarm_IRQHandler(void)
{

	//Resets alarm flags and sets flag to play mp3
	  if(RTC_GetITStatus(RTC_IT_ALRA) != RESET)
	  {
    	RTC_ClearFlag(RTC_FLAG_ALRAF);
	    RTC_ClearITPendingBit(RTC_IT_ALRA);
	    EXTI_ClearITPendingBit(EXTI_Line17);
		interruptOccurred = 1;

	  }


}
void Output_Segment(int number){
	switch(number)
	{
		case 1: //When the digit is one
		GPIOE->BSRRL = 0b00000110000000; //Set pins 7, 8
		GPIOE->BSRRH = 0b11100000000011; //Reset all other segments
			break;
		case 2: //When the digit is two
			GPIOE->BSRRL = 0b11000010000011; //Set pins 0,1,7,12,13
			GPIOE->BSRRH = 0b00100100000000; //Reset all other segments
				break;
		case 3: //When the digit is three
			GPIOE->BSRRL = 0b11000110000001; //Set pins 0,7,8,12,13
			GPIOE->BSRRH = 0b00100000000010; //Reset all other segments
				break;
		case 4: //When the digit is four
			GPIOE->BSRRL = 0b01100110000000; //Set pins 7, 8, 11, 12
			GPIOE->BSRRH = 0b10000000000011; //Reset all other segments
				break;
		case 5: //When the digit is five
			GPIOE->BSRRL = 0b11100100000001; //Set pins 0, 8, 11, 12, 13
			GPIOE->BSRRH = 0b00000010000010; //Reset all other segments
				break;
		case 6: //When the digit is six
			GPIOE->BSRRL = 0b11100100000011; //Set pins 0, 1, 8, 11, 12, 13
			GPIOE->BSRRH = 0b00000010000000; //Reset all other segments
				break;
		case 7: //When the digit is seven
			GPIOE->BSRRL = 0b10000110000000; //Set pins 7, 8, 13
			GPIOE->BSRRH = 0b01100000000011; //Reset all other segments
				break;
		case 8: //When the digit is eight
			GPIOE->BSRRL = 0b11100110000011; //Set pins 0, 1, 7, 8, 11, 12, 13
				break;
		case 9: //When the digit is nine
			GPIOE->BSRRL = 0b11100110000001; //Set pins 0, 7, 8, 11, 12, 13
			GPIOE->BSRRH = 0b00000000000010; //Reset all other segments
				break;
		case 0: //When the digit is zero
			GPIOE->BSRRL = 0b10100110000011; //Set pins 0, 1, 7, 8, 11, 13
			GPIOE->BSRRH = 0b01000000000000; //Reset all other segments
				break;
		}//switch
}//Output_Segment

//displays the current clock or alarm time
void display7Seg(uint8_t h, uint8_t m)
{
	static int pos = 0;	//Contains digit position information

	//Converts into a 4 digit decimal number
	int dech = ((h & 0xF0) >> 4) * 10 + (h & 0x0F);
	int decm = ((m & 0xF0) >> 4) * 10 + (m & 0x0F);
	int t = (dech*100) + decm;

	int num[] = {1000, 100, 10, 1}; //Array of decimal places (Digit Position)
	int dgt[] = {0, 0, 0, 0}; //Array of digit values

	//Loop that distributes single decimal into 4 numbers and places them in dgt array
	for (int position = 0; position < 4; position++) {
	  int cnt = 0;
	  while (t >= num[position]) {
	    cnt++;
	    t -= num[position];
	  }//while
	  dgt[position]=cnt;
	}//for

	//AMPM LEDs
	if(myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_12){ //If in 12 hr format
		if(displayalarm == 1){ //If displaying the alarm
			if(AlarmStruct.RTC_AlarmTime.RTC_H12 == RTC_H12_AM){ //If AM, set green LED
				GPIOD->BSRRH = 0b1000000000;
				GPIOD->BSRRL = 0b10000000000;
			}
			else{
				if(AlarmStruct.RTC_AlarmTime.RTC_H12 == RTC_H12_PM){ //If PM, set red LED
					GPIOD->BSRRH = 0b10000000000;
					GPIOD->BSRRL = 0b1000000000;
				}
			}
		}
		else{ //If displaying the time
			if(myclockTimeStruct.RTC_H12 == RTC_H12_AM){ //If AM, set green LED
				GPIOD->BSRRH = 0b1000000000;
				GPIOD->BSRRL = 0b10000000000;
			}
			else{
				if(myclockTimeStruct.RTC_H12 == RTC_H12_PM){ //If PM, set red LED
					GPIOD->BSRRH = 0b10000000000;
					GPIOD->BSRRL = 0b1000000000;
				}
			}
		}
	}

	//Enable Digits
	switch(pos)
		{
			case 0: // First Digit
				GPIOD->BSRRH = 0b110000110; // set pins 1,2,8,7 = 0
				Output_Segment(dgt[pos]); //Output digit value
				GPIOD->BSRRL = 0b1000;		// Use set reset register low to set pin 3 = 1
				pos++; //Increment to the next digit
					break;
			case 1: // Second Digit
				GPIOD->BSRRH = 0b110001010; // Set pins 1,3,8,7 = 0
				Output_Segment(dgt[pos]);
				GPIOD->BSRRL = 0b100;		// Use set reset register low to set pins 2 = 1
				pos++;
					break;
			case 2: // Third Digit
				GPIOD->BSRRH = 0b010001110; // Set pins 1,2,3,7 = 0
				Output_Segment(dgt[pos]);
				GPIOD->BSRRL = 0b100000000;		// Use set reset register low to set pins 8 = 1
				pos++;
					break;
			case 3: // Fourth Digit
				GPIOD->BSRRH = 0b100001110; // Set pins 1,2,3,8 = 0
				Output_Segment(dgt[pos]);
				GPIOD->BSRRL = 0b010000000;		// Use set reset register low to set pins 7 = 1
				pos++; //Reset to the first digit
					break;
			case 4: // Colon
				pos = 0;	//Reset to first digit
				GPIOD->BSRRH = 0b110001100; // Set other digits off
				GPIOE->BSRRL = 0b10000010000000; // Set colon segments on
				GPIOE->BSRRH = 0b01100100000011; // Set other segments off
				GPIOD->BSRRL = 0b000000010; // Set colon digit on
				break;

		}//switch
}//Display7Seg


//Used to set time values
void setTime(uint8_t h, uint8_t m, uint8_t AMPM)
{
	myclockTimeStruct.RTC_H12 = AMPM; //AM or PM
	myclockTimeStruct.RTC_Hours = h; //Hours
	myclockTimeStruct.RTC_Minutes = m; //Minutes
	myclockTimeStruct.RTC_Seconds = 0x00; //Seconds set to 0s
	RTC_SetTime(RTC_Format_BCD, &myclockTimeStruct); //Set the time with new settings
}

//used to set alarm values
void setAlarm(uint8_t h, uint8_t m, uint8_t AMPM)
{
	AlarmStruct.RTC_AlarmTime.RTC_Hours = h; //Alarm Hour
	AlarmStruct.RTC_AlarmTime.RTC_Minutes = m; //Alarm Minutes
	AlarmStruct.RTC_AlarmTime.RTC_H12 = AMPM; //Alarm Am or PM
	AlarmStruct.RTC_AlarmTime.RTC_Seconds = 0x00; //Seconds set to 0s

	RTC_AlarmCmd(RTC_Alarm_A,DISABLE);
	RTC_SetAlarm(RTC_Format_BCD, RTC_Alarm_A, &AlarmStruct); //Where AlarmStruct hold your new alarm time.
	RTC_AlarmCmd(RTC_Alarm_A,ENABLE); //If you wish to reactivate the alarm.
	RTC_ClearFlag(RTC_FLAG_ALRAF);
}

//Adds 10 minutes to the current alarm time and checks to make sure that time is
//acceptable for a 12 hour or 24 hour clock.  It also stores the time when the button is
//first pressed so when the alarm is cancelled the alarm time goes back to
//the original
void snooze(void)
{
	alarmMemory.RTC_AlarmTime = AlarmStruct.RTC_AlarmTime;
	//IF adding 10 mins goes over 59
	if((AlarmStruct.RTC_AlarmTime.RTC_Minutes + 0x10) > 89){
		// In 12 hr, 12:54 -> 1:04
		if((AlarmStruct.RTC_AlarmTime.RTC_Hours >= 0x12) && (myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_12))
			setAlarm(0x01, AlarmStruct.RTC_AlarmTime.RTC_Minutes - 0x50, AlarmStruct.RTC_AlarmTime.RTC_H12);
		// In 24 hr, 23:54 -> 00:04
 		else if((AlarmStruct.RTC_AlarmTime.RTC_Hours == 0x23) && (myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_24))
 			setAlarm(0x00, AlarmStruct.RTC_AlarmTime.RTC_Minutes - 0x50, RTC_H12_AM);
 		// 11:54am -> 12:04pm
 		else if((AlarmStruct.RTC_AlarmTime.RTC_Hours == 0x11) && (AlarmStruct.RTC_AlarmTime.RTC_H12 == RTC_H12_AM))
			setAlarm(0x12, AlarmStruct.RTC_AlarmTime.RTC_Minutes - 0x50, RTC_H12_PM);
 		// 11:54pm -> 12:54am
 		else if((AlarmStruct.RTC_AlarmTime.RTC_Hours == 0x11) && (AlarmStruct.RTC_AlarmTime.RTC_H12 == RTC_H12_PM))
			setAlarm(0x12, AlarmStruct.RTC_AlarmTime.RTC_Minutes - 0x50, RTC_H12_AM);
 		else
 		// EX: 1:04 -> 2:04
 		setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours + 0x01, AlarmStruct.RTC_AlarmTime.RTC_Minutes - 0x50, AlarmStruct.RTC_AlarmTime.RTC_H12);
	}
	else // Otherwise, add 10 minutes to the current time
	setAlarm(AlarmStruct.RTC_AlarmTime.RTC_Hours, AlarmStruct.RTC_AlarmTime.RTC_Minutes + 0x10, AlarmStruct.RTC_AlarmTime.RTC_H12);
	snoozeMemory = 1;

}


//Called by the timer interrupt to separate the BCD time values
void getCurrentTime(void)
{
	// Obtain current time.
	RTC_GetTime(RTC_Format_BCD, &myclockTimeStruct);
}



//Converts the 12 hour time to 24 hour when displaying the time
//or alarm values
void set24Hour(void)
{
	//uint8_t hour = myclockTimeStruct.RTC_Hours;	//Show hours in variables tab
	//uint8_t H12 = myclockTimeStruct.RTC_H12;		//Show AM/PM in variables tab
	if(myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_12)
		{			//Check if the clock is in the 12 hour mode else 24
			//TIME
			if(myclockTimeStruct.RTC_H12 == RTC_H12_AM){//If the current time is in AM
				myclockInitTypeStruct.RTC_HourFormat = RTC_HourFormat_24;//Change structure to 24 hour mode
				myclockTimeStruct.RTC_H12 = RTC_H12_AM;	//Set struct to PM
				RTC_Init(&myclockInitTypeStruct);	//Write struct to RTC register
			}else{
				myclockTimeStruct.RTC_Hours = myclockTimeStruct.RTC_Hours + 0x12;	//Else If the time is PM subract 12 hours from timestruct
				myclockInitTypeStruct.RTC_HourFormat = RTC_HourFormat_24;	//Change structure to 24 hour mode
				myclockTimeStruct.RTC_H12 = RTC_H12_PM;					//Set struct to PM
				RTC_Init(&myclockInitTypeStruct);						//Write initStruct to RTC register
				RTC_SetTime(RTC_Format_BCD, &myclockTimeStruct);		//Write new time struct
			}
			//ALARM
			if(AlarmStruct.RTC_AlarmTime.RTC_H12 == RTC_H12_PM){//If the current alarm is in PM
				AlarmStruct.RTC_AlarmTime.RTC_Hours = AlarmStruct.RTC_AlarmTime.RTC_Hours + 0x12;//Subract 12 hours from timestruct
				AlarmStruct.RTC_AlarmTime.RTC_H12 = RTC_H12_PM;									//Set struct to PM
				RTC_AlarmCmd(RTC_Alarm_A,DISABLE);
				RTC_SetAlarm(RTC_Format_BCD, RTC_Alarm_A, &AlarmStruct); //Where AlarmStruct holds your new alarm time.
				RTC_AlarmCmd(RTC_Alarm_A,ENABLE); 						 //Reactivates the alarm.
				RTC_ClearFlag(RTC_FLAG_ALRAF);
			}
	}else{
	if(myclockInitTypeStruct.RTC_HourFormat == RTC_HourFormat_24)
		{
			//TIME
			if(myclockTimeStruct.RTC_Hours > 0x12){							//If greater than 12hours must be in 24hour mode
				myclockTimeStruct.RTC_Hours = myclockTimeStruct.RTC_Hours - 0x12;	//Subtract 12 hours from struct to convert to 12hour format
				myclockInitTypeStruct.RTC_HourFormat = RTC_HourFormat_12;	//Change initstruct to 12 hour
				myclockTimeStruct.RTC_H12 = RTC_H12_PM;					//Set struct to PM
				RTC_Init(&myclockInitTypeStruct);							//Write init
				RTC_SetTime(RTC_Format_BCD, &myclockTimeStruct);		//Write to RTC
			}else{
				myclockInitTypeStruct.RTC_HourFormat = RTC_HourFormat_12;	//Must be 24 <12 so change to 12
				myclockTimeStruct.RTC_H12 = RTC_H12_AM;						//Make sure AM is set
				RTC_Init(&myclockInitTypeStruct);							//Write init
				RTC_SetTime(RTC_Format_BCD, &myclockTimeStruct);			//Write timestruct to RTC
			}
			//ALARM
			if(AlarmStruct.RTC_AlarmTime.RTC_Hours > 0x12){
				AlarmStruct.RTC_AlarmTime.RTC_Hours = AlarmStruct.RTC_AlarmTime.RTC_Hours - 0x12;	//Else If the time is PM subract 12 hours from timestruct
				AlarmStruct.RTC_AlarmTime.RTC_H12 = RTC_H12_PM;					//Set struct to PM
				RTC_AlarmCmd(RTC_Alarm_A,DISABLE);
				RTC_SetAlarm(RTC_Format_BCD, RTC_Alarm_A, &AlarmStruct); //Where AlarmStruct hold your new alarm time.
				RTC_AlarmCmd(RTC_Alarm_A,ENABLE); //Reactivates the alarm.
				RTC_ClearFlag(RTC_FLAG_ALRAF);
			}
		}
	}
	hour24Flag = 0;			//Reset flag after 12-24 change is done
}


//Called every timer interrupt.  checks for changes on the
//input data register and then debounces and sets which button is pressed
void buttonControls(void)
{
	static int new = 0; //New IDR state
	static int old = 0; //Previous IDR state

	//Gets the current register value of pins 6, 7, 8, 9, 11 and stores the values in new
	uint16_t newState = GPIOC->IDR;
	new = (newState & 0b0000000001000000) >> 6;
	new = (new*10) + ((newState & 0b0000000010000000) >> 7);
	new = (new*10) + ((newState & 0b0000000100000000) >> 8);
	new = (new*10) + ((newState & 0b0000001000000000) >> 9);
	new = (new*10) + ((newState & 0b0000100000000000) >> 11);

	int check = 0; //Checks to make sure the button is not held down
	debouncing++; //Increment the debouncing counter
	if(old != new) debouncing = 0; //If the previous IDR does not equal the new, reset debounce
	if((old == new) && (debouncing >= 50)){ check = 1;} //Enables buttons if debounced correctly

	//SNOOZE placed outside debounce to stop the Mp3
	int pin11 = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_11); //Snooze
		if(pin11 == 0){ //If SNOOZE is pressed
			buttonState = SNOOZE; //Change the pressed state to SNOOZE
			buttonFlag = 1; //Flag to indicate button press
			debouncing = 0; //Reset the debounce counter
		}
	//SELECT used as reset outside debounce when alarm is playing
	int pin9 = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_9); //Reset
		if((modeon == 0) && (pin9 == 0)){ //If not in the mode menu and SELECT is pressed
			buttonState = SELECT;
			buttonFlag = 1;
			debouncing = 0;
		}

	if(check == 1){ //If the IDR passes the debounce check
		int pin6 = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_6); //MODE
		int pin7 = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_7); //UP
		int pin8 = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_8); //DOWN

		if(pin6 == 0){ //If MODE was pressed
				buttonState = MODE;
				modeon = 1; //Enables the mode menu
				buttonFlag = 1;
				debouncing = 0;
			switch(currentmode){ //Mode menu
				case 0: // Set 12/24 hr mode
					currentmode = 1;
					break;
				case 1: // Set time mode
					currentmode = 2;
					setmin = 0;
					break;
				case 2: // Set alarm mode
					currentmode = 3;
					setmin = 0;
					break;
				case 3: // Exit Mode menu
					displayalarm = 0;
					currentmode = 0;
					modeon = 0;
					break;
			}
		}//If button is pressed, set button state to match
		else{
			if(pin7 == 0){ //If UP button is pressed
				buttonState = UP;
				buttonFlag = 1;
				debouncing = 0;
			}
			else{
				if(pin8 == 0){ //If DOWN button is pressed
					buttonState = DOWN;
					buttonFlag = 1;
					debouncing = 0;
				}
				else{
					if(pin9 == 0){ //If SELECT button is pressed
					buttonState = SELECT;
					buttonFlag = 1;
					debouncing = 0;
					}
					else{
						if(pin11 == 0){ //If SNOOZE button is pressed
						buttonState = SNOOZE;
						buttonFlag = 1;
						debouncing = 0;
						}
					}
				}
			}
		}
	}
	old = new; //Debouncing
}


//Configures the clocks, gpio, alarm, interrupts etc.
void configuration(void)
{
	//Lets the system clocks be viewed
	RCC_GetClocksFreq(&RCC_Clocks);

	//Enable peripheral clocks
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);	// Needed for Audio chip

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	//enable the RTC
	PWR_BackupAccessCmd(DISABLE);
	PWR_BackupAccessCmd(ENABLE);
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
	RCC_RTCCLKCmd(ENABLE);
	RTC_AlarmCmd(RTC_Alarm_A,DISABLE);

	//Enable the LSI OSC
	RCC_LSICmd(ENABLE);

	//Wait till LSI is ready
	while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);

	//Enable the external interrupt for the RTC to use the Alarm
	// EXTI configuration
	EXTI_ClearITPendingBit(EXTI_Line17);
	EXTI_InitStructure.EXTI_Line = EXTI_Line17;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);

	//Set timer 5 to interrupt at a rate of 500Hz
	TIM_TimeBaseStructInit(&TIM_InitStruct);
	TIM_InitStruct.TIM_Period	=  8000;	// 500Hz
	TIM_InitStruct.TIM_Prescaler = 20;
	TIM_TimeBaseInit(TIM5, &TIM_InitStruct);

	// Enable the TIM5 global Interrupt
	NVIC_Init( &NVIC_InitStructure );
	NVIC_InitStructure.NVIC_IRQChannel = TIM5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init( &NVIC_InitStructure );

	//Setup the RTC for 12 hour format
	myclockInitTypeStruct.RTC_HourFormat = RTC_HourFormat_12;
	myclockInitTypeStruct.RTC_AsynchPrediv = 127;
	myclockInitTypeStruct.RTC_SynchPrediv = 0x00FF;
	RTC_Init(&myclockInitTypeStruct);

	//Set the time displayed on power up to 7:00AM
	myclockTimeStruct.RTC_H12 = RTC_H12_AM;
	myclockTimeStruct.RTC_Hours = 0x07;
	myclockTimeStruct.RTC_Minutes = 0x59;
	myclockTimeStruct.RTC_Seconds = 0x00;
	RTC_SetTime(RTC_Format_BCD, &myclockTimeStruct);


	//Sets alarmA for 8:00AM, date doesn't matter
	AlarmStruct.RTC_AlarmTime.RTC_H12 = RTC_H12_AM;
	AlarmStruct.RTC_AlarmTime.RTC_Hours = 0x08;
	AlarmStruct.RTC_AlarmTime.RTC_Minutes = 0x00;
	AlarmStruct.RTC_AlarmTime.RTC_Seconds = 0x00;
	AlarmStruct.RTC_AlarmMask = RTC_AlarmMask_DateWeekDay;
	RTC_SetAlarm(RTC_Format_BCD,RTC_Alarm_A,&AlarmStruct);

	// Enable the Alarm global Interrupt
	NVIC_Init( &NVIC_InitStructure );
	NVIC_InitStructure.NVIC_IRQChannel = RTC_Alarm_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init( &NVIC_InitStructure );

	// Pins B6 and B9 are used for I2C communication, configure as alternate function.
	// We use them to communicate with the CS43L22 chip via I2C
	GPIO_StructInit( &GPIOInitStruct );
	GPIOInitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_9; // we are going to use PB6 and PB9
	GPIOInitStruct.GPIO_Mode = GPIO_Mode_AF;			// set pins to alternate function
	GPIOInitStruct.GPIO_Speed = GPIO_Speed_50MHz;		// set GPIO speed
	GPIOInitStruct.GPIO_OType = GPIO_OType_OD;			// set output to open drain --> the line has to be only pulled low, not driven high
	GPIOInitStruct.GPIO_PuPd = GPIO_PuPd_UP;			// enable pull up resistors
	GPIO_Init(GPIOB, &GPIOInitStruct);

	// The I2C1_SCL and I2C1_SDA pins are now connected to their AF
	// so that the I2C1 can take over control of the
	//  pins
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_I2C1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_I2C1);

	// Set the I2C structure parameters
	// I2C is used to configure CS43L22 chip for analog processing on the headphone jack.
	I2C_InitStruct.I2C_ClockSpeed = 100000; 			// 100kHz
	I2C_InitStruct.I2C_Mode = I2C_Mode_I2C;			// I2C mode
	I2C_InitStruct.I2C_DutyCycle = I2C_DutyCycle_2;	// 50% duty cycle --> standard
	I2C_InitStruct.I2C_OwnAddress1 = 0x00;			// own address, not relevant in master mode
	I2C_InitStruct.I2C_Ack = I2C_Ack_Disable;			// disable acknowledge when reading (can be changed later on)
	I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit; // set address length to 7 bit addresses

	// Initialize the I2C peripheral w/ selected parameters
	I2C_Init(I2C1,&I2C_InitStruct);

	// Enable I2C1
	I2C_Cmd(I2C1, ENABLE);

	//IO for push buttons using internal pull-up resistors
	GPIO_StructInit( &GPIOInitStruct );
	GPIOInitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_11;
	GPIOInitStruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIOInitStruct.GPIO_Mode = GPIO_Mode_IN;
	GPIOInitStruct.GPIO_OType = GPIO_OType_PP;
	GPIOInitStruct.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOC, &GPIOInitStruct);

	//Configure GPIO for segments
	GPIO_StructInit( &GPIOInitStruct );
	GPIOInitStruct.GPIO_Pin = GPIO_Pin_13| GPIO_Pin_7| GPIO_Pin_8| GPIO_Pin_0| GPIO_Pin_1| GPIO_Pin_11| GPIO_Pin_12;
	GPIOInitStruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIOInitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIOInitStruct.GPIO_OType = GPIO_OType_PP;
	GPIOInitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOE, &GPIOInitStruct);

	//Configure GPIO for multiplexing
	// Note: Pin D4 is used to reset the CS43L22 chip
	//       Pins D7, D8, D9, D10, D11 are recommended for multiplexing the LED display.
	GPIO_StructInit( &GPIOInitStruct );
	GPIOInitStruct.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_8 | GPIO_Pin_7| GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11;
	GPIOInitStruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIOInitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIOInitStruct.GPIO_OType = GPIO_OType_PP;
	GPIOInitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOD, &GPIOInitStruct);

	//Enables RTC alarm A interrupt
	RTC_ITConfig(RTC_IT_ALRA, ENABLE);

	//Enables timer interrupt
	TIM5->DIER |= TIM_IT_Update;

	//Enables timer
	TIM5->CR1 |= TIM_CR1_CEN;

}
