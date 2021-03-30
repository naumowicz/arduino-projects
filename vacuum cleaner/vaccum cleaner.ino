/*
Project basing on:
https://create.arduino.cc/projecthub/SurtrTech/measure-any-ac-current-with-acs712-70aa85
*/
#include <Filters.h> //Library for calculating AC current https://github.com/hideakitai/Filters

#define ACS712 A6 //ACS712 data pin
#define CLEANER A2 //relay for turning on and off filter cleaner
#define MASTER A4 //master - detecting if other device connected to vacuum cleaner draws current, yes means turning vacuum on
#define ENGINE_RELAY A0 //relay for turning vacuum cleaner engine

float testFrequency = 50; //test signal frequency (Hz)
float windowLength = 40.0 / testFrequency; //for how long wait between checking signal (for average)

float intercept = 0;  // to be adjusted based on calibration testing
float slope = 0.0752; // to be adjusted based on calibration testing

class Vaccum
{
private:
	unsigned long nextCleaningTime; //time for next cleaning
	bool cleanerState; //position of filter cleaner
	unsigned int cleanerCounter; //how many times filter cleaner cleaned the filter
	unsigned int resetCleanerCounterNumber; //after this number filter cleaner assumes the filter was cleaned
	unsigned int cleaningInterval; //time between cleaning filter procedure
	unsigned int timeBetweenCleaningProcedure; //cleaning procedure needs hitting filter with bat and moving bat again at ready position, it requires time between those two events
	unsigned int shortTimeBetweenCleaningProcedure; //short delay waiting for bat hitting filter, without this time bat won't hit filter properly
	bool delayAfterMaster; //time after detecting master to start vacuum cleaner
	float ampsFromACSToStartVaccum; //how many amps needed to start vaccum when master is connected
	unsigned int timeAfterMaster; //for how long vaccum has to run after master stopped working

	RunningStatistics inputStats; //var for counting AC current

	float amps; //real current
	float acsValue; //value detected from ACS712 meter

public:
	Vaccum()
	{
		nextCleaningTime = 0;
		cleanerState = false;
		cleanerCounter = 0;
		resetCleanerCounterNumber = 6;
		cleaningInterval = 16000;
		timeBetweenCleaningProcedure = 2000;
		shortTimeBetweenCleaningProcedure = 100;
		delayAfterMaster = false;
		ampsFromACSToStartVaccum = 0.5f;
		timeAfterMaster = 5000;
		

		// create statistics to look at the raw test signal
		inputStats.setWindowSecs(windowLength);

		// Current sensor
		amps = 0.0f;
		acsValue = 0.0f;
	}

	bool checkPowerStatus()
	{
		// return digitalRead(ACS712);
		// return true;
	}

	void mainLoop()
	{
		while (true) //lazy way to have endless loop
		{
			while (checkMaster() == false and checkCurrent() == false) //if vacuum has no master connected, vaccum works normally (in case checking current as well)
			{
				turnOnEngine();
				if (millis() >= nextCleaningTime)
				{
					cleaningFilter();
				}
			}
			while (checkMaster() == true and checkCurrent() == true) //if vacuum has master connected, vaccum waits for current from master to start the engine
			{
				turnOnEngine();
				if (millis() >= nextCleaningTime)
				{
					cleaningFilter();
				}
				delayAfterMaster = true;
			}
			cleanUpAfterMasterProcedure();
			turnOffEngine();
			resetCleanerPosition();
		}
	}

	bool checkMaster() //works in reverse, when electrical circuit is closed, master is not connected
	{
		return !digitalRead(MASTER);
	}

	void cleaningFilter()
	{
		if (cleanerCounter == resetCleanerCounterNumber) //when cleaning was done
		{
			cleanerCounter = 0;
			nextCleaningTime = millis() + cleaningInterval; //set next time for cleaning
		}
		else //cleaning was not finished
		{
			cleanerState = !cleanerState;
			digitalWrite(CLEANER, cleanerState); //changing state of cleaner
			cleanerCounter++;

			if (cleanerCounter % 2 == 0) //time between idle position of bat, after it makes move back and forth
			{
				nextCleaningTime = millis() + timeBetweenCleaningProcedure;
			}
			else //time between bat on idle position and hitting filter, waiting till bat hits
			{
				nextCleaningTime = millis() + shortTimeBetweenCleaningProcedure;
			}
		}
	}

	void resetCleanerPosition()
	{
		digitalWrite(CLEANER, LOW);
		cleanerState = false;
		cleanerCounter = 0;
	}

	bool checkCurrent()
	{
		acsValue = analogRead(ACS712); // read the analog in value
		inputStats.input(acsValue);	   // pass value to Stats function

		amps = intercept + slope * inputStats.sigma(); //Calibrate the values
		// Serial.println(amps);
		if (amps >= ampsFromACSToStartVaccum)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	void turnOnEngine()
	{
		// Serial.println("engine on");
		digitalWrite(ENGINE_RELAY, HIGH);
	}

	void turnOffEngine()
	{
		// Serial.println("engine off");
		digitalWrite(ENGINE_RELAY, LOW);
	}

	void calibrateASC() // calibrating output values from ACS712, done by inputStats object
	{
		for (int i = 0; i < 25; i++)
		{
			delay(50);
			checkCurrent();
		}
		delay(1500);
	}

	void cleanUpAfterMasterProcedure() //when master no longer works, vacuum has to work further for some time
	{
		if (delayAfterMaster)
		{
			// Serial.println("delaying");
			delay(timeAfterMaster);
			delayAfterMaster = false;
		}
	}
};

void setup()
{
	//setting pin mode for each of Arduino pins
	pinMode(CLEANER, OUTPUT);
	pinMode(MASTER, INPUT_PULLUP);
	pinMode(ENGINE_RELAY, OUTPUT);
	pinMode(ACS712, INPUT);

	//forcing LOW state because it can be random
	digitalWrite(CLEANER, LOW);
	digitalWrite(ENGINE_RELAY, LOW);
	// Serial.begin(9600);
}

void loop()
{
	Vaccum *vaccum;
	vaccum = new Vaccum();
	//making sure cleaner has right position
	vaccum->resetCleanerPosition();
	vaccum->calibrateASC();
	vaccum->mainLoop();
}
