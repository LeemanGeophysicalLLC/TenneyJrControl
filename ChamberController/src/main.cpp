#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MAX31865.h>
#include "Cmd.h"
#include "pins.h"
#include <Adafruit_SleepyDog.h>
#include <PID_v1.h>

/*
 * COMMANDS
 * 
 * FANSON - Turns on Fans
 * FANSOFF - Turns off Fans
 * COOLMODE - Turns chamber into cooling mode
 * WARMMODE - Turns chamber into heating mode
 * STDBYMODE - Turns chamber into standby mode
 * SETTEMP XXX - Sets temperature in floating point degC
 * POWERON - Turns on the master contactor
 * POWEROFF - Turns off the master contactor
 */

// use hardware SPI, just pass in the CS pin
Adafruit_MAX31865 thermo = Adafruit_MAX31865(10);

// Global states
enum chamber_modes {MODE_COOLING, MODE_WARMING, MODE_STANDBY};
uint8_t fan_status = 0;
uint8_t master_contactor_status = 0;
uint8_t compressor_status = 0;
uint8_t cold_bypass_status = 0;
uint8_t heater_status = 0;
uint8_t rtd_status = 0;
uint8_t control_pin = 0;
chamber_modes chamber_mode = MODE_STANDBY;
double setpoint = 20.0;
double process_variable = 0.;
char dummychar[2];

// PID Setup
double PID_output=0;
double Kp=10000, Ki=500, Kd=0;
PID myPID(&process_variable, &PID_output, &setpoint, Kp, Ki, Kd, DIRECT);

int WindowSize = 5000;
unsigned long windowStartTime;


void read_rtd()
{
  rtd_status = thermo.readFault();
  uint16_t rtd = thermo.readRTD();
  float ratio = rtd;
  ratio /= 32768;
  // first is resistance at freezing, second is reference resistance
  process_variable = thermo.temperature(100, 430.0);
}

void FansOn(int arg_cnt, char **args)
{
  /*
   * Turn the chamber and radiator fans on
   */
  digitalWrite(PIN_FANS, HIGH);
  fan_status = 1;
}

void FansOff(int arg_cnt, char **args)
{
  /*
   * Turn the chamber and radiator fans off
   */
  digitalWrite(PIN_FANS, LOW);
  fan_status = 0;
}

void CompressorOn()
{
  /*
   * Turn the compressor on
   */
  digitalWrite(PIN_COMPRESSOR, HIGH);
  compressor_status = 1;
}

void CompressorOff()
{
  /*
   * Turn the compressor off
   */
  digitalWrite(PIN_COMPRESSOR, LOW);
  compressor_status = 0;
}

void ColdBypassOn()
{
  /*
   * Turn the cold bypass on (warm the chamber)
   */
  digitalWrite(PIN_COLD_BYPASS, HIGH);
  cold_bypass_status = 1;
}

void ColdBypassOff()
{
  /*
   * Turn the cold bypass off (cool the chamber)
   */
  digitalWrite(PIN_COLD_BYPASS, LOW);
  cold_bypass_status = 0;
}

void HeaterOn()
{
  /*
   * Turn the heater on (warm the chamber)
   */
  digitalWrite(PIN_HEATER, HIGH);
  heater_status = 1;
}

void HeaterOff()
{
  /*
   * Turn the heater off (cool the chamber)
   */
  digitalWrite(PIN_HEATER, LOW);
  heater_status = 0;
}

void MasterOn(int arg_cnt, char **args)
{
  /*
   * Turn the master on
   */
  digitalWrite(PIN_MASTER_POWER, HIGH);
  master_contactor_status = 1;
}

void MasterOff(int arg_cnt, char **args)
{
  /*
   * Turn the master off
   */
  digitalWrite(PIN_MASTER_POWER, LOW);
  master_contactor_status = 0;
}

void SetCoolMode(int arg_cnt, char **args)
{
  /*
   * Set the chamber into cooling mode. turn on fans, compressor, make sure bypass valve is
   * on so we don't start cooling yet.
   */
  chamber_mode = MODE_COOLING;
  control_pin = PIN_COLD_BYPASS;
  MasterOn(1, *dummychar);
  FansOn(1, *dummychar);
  HeaterOff();
  CompressorOn();
  ColdBypassOn();
}

void SetWarmMode(int arg_cnt, char **args)
{
  /*
   * Set the chamber into warming mode.
   */
  chamber_mode = MODE_WARMING;
  MasterOn(1, *dummychar);
  FansOn(1, *dummychar);
  HeaterOff();
  CompressorOff();
  ColdBypassOff();
  windowStartTime = millis();
  myPID.SetOutputLimits(0, WindowSize);
  myPID.SetMode(AUTOMATIC);
}

void SetTemperature(int arg_cnt, char **args)
{
  /*
   * Set the temperature as a float in degC.
   */
  if (arg_cnt > 0 )
  {
    setpoint = cmdStr2Num(args[1], 10);
    setpoint += cmdStr2Num(args[2], 10) / 10;
  }
}

void SetKP(int arg_cnt, char **args)
{
  /*
   * Set the KP Gain (temporary until unit restarts).
   */
  if (arg_cnt > 0 )
  {
    Kp = cmdStr2Num(args[1], 10);
    Kp += cmdStr2Num(args[2], 10) / 10;
  }
}

void SetKI(int arg_cnt, char **args)
{
  /*
   * Set the KI Gain (temporary until unit restarts).
   */
  if (arg_cnt > 0 )
  {
    Ki = cmdStr2Num(args[1], 10);
    Ki += cmdStr2Num(args[2], 10) / 10;
  }
}


void SetKD(int arg_cnt, char **args)
{
  /*
   * Set the KP Gain (temporary until unit restarts).
   */
  if (arg_cnt > 0 )
  {
    Kd = cmdStr2Num(args[1], 10);
    Kd += cmdStr2Num(args[2], 10) / 10;
  }
}


void SetStandbyMode(int arg_cnt, char **args)
{
  /*
   * Put the chamber in standby mode.
   */
  chamber_mode = MODE_STANDBY;
  MasterOn(1, *dummychar);
  FansOff(1, *dummychar);
  HeaterOff();
  CompressorOff();
  ColdBypassOff();
}

void SendStatus()
{
  /*
   * Send the status to the computer.
   */
  switch (chamber_mode)
  {
    case MODE_STANDBY:
      Serial.print("STANDBY");
      break;
    case MODE_COOLING:
      Serial.print("COOLING");
      break;
    case MODE_WARMING:
      Serial.print("WARMING");
      break;
    default:
      Serial.print("ERROR");
      break;
  }
  Serial.print("\t");
  Serial.print(rtd_status);
  Serial.print("\t");
  Serial.print(process_variable);
  Serial.print("\t");
  Serial.print(setpoint);
  Serial.print("\t");
  Serial.print(master_contactor_status);
  Serial.print("\t");
  Serial.print(compressor_status);
  Serial.print("\t");
  Serial.print(cold_bypass_status);
  Serial.print("\t");
  Serial.print(heater_status);
  Serial.print("\t");
  Serial.print(fan_status);
  Serial.print("\t");
  Serial.print(Kp);
  Serial.print("\t");
  Serial.print(Ki);
  Serial.print("\t");
  Serial.print(Kd);
  Serial.print("\t");
  Serial.println(PID_output);
}

void setup()
{

  Serial.begin(9600);

  Watchdog.enable(4000);

  pinMode(PIN_MASTER_POWER, OUTPUT);
  pinMode(PIN_COMPRESSOR, OUTPUT);
  pinMode(PIN_COLD_BYPASS, OUTPUT);
  pinMode(PIN_HEATER, OUTPUT);
  pinMode(PIN_FANS, OUTPUT);
  
  digitalWrite(PIN_MASTER_POWER, LOW);
  digitalWrite(PIN_COMPRESSOR, LOW);
  digitalWrite(PIN_COLD_BYPASS, LOW);
  digitalWrite(PIN_HEATER, LOW);
  digitalWrite(PIN_FANS, LOW);

  thermo.begin(MAX31865_3WIRE);

  // Setup the command structure
  cmdInit(&Serial);
  cmdAdd("FANSON", FansOn);
  cmdAdd("FANSOFF", FansOff);
  cmdAdd("COOLMODE", SetCoolMode);
  cmdAdd("WARMMODE", SetWarmMode);
  cmdAdd("STDBYMODE", SetStandbyMode);
  cmdAdd("SETTEMP", SetTemperature);
  cmdAdd("POWERON", MasterOn);
  cmdAdd("POWEROFF", MasterOff);
  cmdAdd("SETKP", SetKP);
  cmdAdd("SETKI", SetKI);
  cmdAdd("SETKD", SetKD);
}

void RunCoolingControl()
{
  /*
   * Cooling mode control logic.
   */
  float control_setpoint = setpoint;

  /*
  if (cold_bypass_status)
  {
    control_setpoint += 0.05;
  }
  else
  {
    control_setpoint -= 0.05;
  }
  */

  if (process_variable > control_setpoint)
  {
    ColdBypassOff();
  }

  else
  {
    ColdBypassOn();
  }
}

void RunWarmingControl()
{
  /*
   * Control logic in the warming mode
   */
  /*
  float control_setpoint = setpoint;

  if (heater_status)
  {
    control_setpoint -= 0.2;
  }
  else
  {
    control_setpoint -= 0.0;
  }

  if (process_variable < control_setpoint)
  {
    HeaterOn();
  }

  else
  {
    HeaterOff();
  }
  */
  myPID.Compute();
  if (millis() - windowStartTime > WindowSize)
  { //time to shift the Relay Window
    windowStartTime += WindowSize;
  }
  /*
  Serial.print("debug,");
  Serial.print(millis());
  Serial.print(",");
  Serial.print(windowStartTime);
  Serial.print(",");
  Serial.print(WindowSize);
  Serial.print(",");
  Serial.println(PID_output);
  */
  if (PID_output < 500) HeaterOff();
  if (PID_output > millis() - windowStartTime) HeaterOn();
  else HeaterOff();
}

void loop()
{
  /*
   * Main loop
   * 
   * Pet the dog
   * Send the status
   * Update process variable and RTD status
   * Run the appropriate control loop
   * Process any serial commands
   */

  static uint8_t status_counter = 0;
  Watchdog.reset();

  if (status_counter%10 == 0)
  {
    SendStatus();
  }
  
  read_rtd();
  
  switch (chamber_mode)
  {
  case MODE_COOLING:
    RunCoolingControl();
    delay(500);
    break;
  case MODE_WARMING:
    RunWarmingControl();
    //delay(10);
    break;
  default:
    break;
  }

  cmdPoll();
}