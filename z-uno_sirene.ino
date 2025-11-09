/*
 * Battery operated ZWave outdoor alarm siren
 * Z-UNO Sketch
 */

// PIN NUMBERS
#define LED_PIN       13          // Info: remember, this is PWM1 PIN
#define NEUN_VOLT_PIN 4           // ADC1
#define ALARM_PIN     PWM2

// ZWAVE CHANNELS
#define Switch_SirenOnOff_Channel           1
#define Sensor_9V_Channel                   2
#define Sensor_3V_Channel                   3
#define Sensor_Tamper_Channel               4
#define Switch_RequestBatteryUpdate_Channel 5

// Global variables

//... sensors
byte TAMPER_SensorBinaryState=LOW;
byte _tAMPER_SensorBinaryState=LOW;

//... switches
byte AlarmState; // switch external sirene on/off
byte SwitchState; // workaround to request an sensor update (battery status) send from the sleeping FLIRS device

boolean send9VReport=false;
boolean send3VReport=false;
boolean sendTamperReport=false;

boolean tamperStayAwake=false;

boolean firstRun;

// FLIRS Device
ZUNO_SETUP_SLEEPING_MODE(ZUNO_SLEEPING_MODE_FREQUENTLY_AWAKE);

// Z-Wave channels
ZUNO_SETUP_CHANNELS(
  ZUNO_SWITCH_BINARY(AlarmGetter, AlarmSetter),
  ZUNO_SENSOR_MULTILEVEL(ZUNO_SENSOR_MULTILEVEL_TYPE_VOLTAGE, SENSOR_MULTILEVEL_SCALE_VOLT, SENSOR_MULTILEVEL_SIZE_TWO_BYTES, SENSOR_MULTILEVEL_PRECISION_ONE_DECIMAL, NeunVoltGetter),
  ZUNO_SENSOR_MULTILEVEL(ZUNO_SENSOR_MULTILEVEL_TYPE_VOLTAGE, SENSOR_MULTILEVEL_SCALE_VOLT, SENSOR_MULTILEVEL_SIZE_TWO_BYTES, SENSOR_MULTILEVEL_PRECISION_ONE_DECIMAL, DreiVoltGetter),
  ZUNO_SENSOR_BINARY_TAMPER(TAMPER_SensorBinaryGetter),
  ZUNO_SWITCH_BINARY(SwitchGetter, SwitchSetter)
);

//ZUNO_SENSOR_BINARY(ZUNO_SENSOR_BINARY_TAMPER, TAMPER_SensorBinaryGetter),

void setup() {
  analogReference(INTERNAL); // use the internal ~1.2volt reference
  analogRead(A0);  // force voltage reference to be turned on
  
  pinMode(ALARM_PIN, OUTPUT); 
  pinMode(NEUN_VOLT_PIN, INPUT);  // 9V measure

  firstRun=true;
 
  _tAMPER_SensorBinaryState = LOW;
  TAMPER_SensorBinaryState = LOW;

  //... check if a INT1 was detected to check if TAMPER sensor report should be send to ZWave controller
  byte wakeUpReason = zunoGetWakeReason();
  switch (wakeUpReason) {

    case ZUNO_WAKEUP_REASON_INT1:
      // Wakeup on INT1 state change or Key Scanner
      //..set the sensor to HIGH to prepare ZWave report
      _tAMPER_SensorBinaryState=HIGH;
      tamperStayAwake=true;
      break;
    
    case ZUNO_WAKEUP_REASON_RADIO:
      // FLiRS received a packet
      break;

  }
   
}

void loop() {

  //... Tamper alarm from wake-up via INT1?
  if (_tAMPER_SensorBinaryState != TAMPER_SensorBinaryState) { // if state changes
    TAMPER_SensorBinaryState = _tAMPER_SensorBinaryState; // save new state
    zunoSendReport(Sensor_Tamper_Channel); // send report over the Z-Wave to the controller  
  }

  //... ensure all signals can be handled after a wake-up
  if (firstRun)
  {
    delay(10000); //...wait on initial wakeup that device and zwave controller can receive and process all pending signals that might be pending in queue
  }
  
  firstRun=false;

  //...9V
  if (send9VReport)
  {
    zunoSendReport(Sensor_9V_Channel); // Send 9v alarm battery report on request of ZWave Switch change   
  }

  //...3V
  if (send3VReport)
  {
    zunoSendReport(Sensor_3V_Channel); // Send 3v operating battery report on request of ZWave Switch change   
  }

  //...Tamper INT1
  if (sendTamperReport)
  {
    zunoSendReport(Sensor_Tamper_Channel); // Send updated tamper sensor status   
  }

  
  // ... go to sleep to save battery
  if (!send9VReport && !send3VReport && !sendTamperReport && !tamperStayAwake)
  {
    delay(450);
    zunoSetBeamCountWU(0); // ensure deep sleep on FLIRS
    zunoSendDeviceToSleep(); 
  } else 
  {
    delay(50);
  }

}

word get9VStatus()
{
  
  /*
    uses the stable internal 1.2volt reference
    10k resistor from A0 to ground, and 100k resistor from A0 to +batt
    used in formula for R1 and R2
  */
  
  float R1 = 100000.0; // resistance of R1 (~100K)
  float R2 = 10000.0; // resistance of R2 (~10K)
  float Aref = 1.23; // ***calibrate here*** | change this to the actual ref ~1.21 voltage of ---YOUR--- Z-Uno
  
  unsigned int total; 
  float voltage; // converted to volt

  total=0;
  for (int x = 0; x < 5; x++) { // multiple analogue readings for averaging
    total = total + analogRead(NEUN_VOLT_PIN); // add each value to a total
  }
  voltage = ((float)total / 5.0) * ((R1+R2)/R2) * Aref / 1024.0 ; // convert readings to volt
  return (word)(voltage*10);
 
}

word get3VStatus()
{
  float vout = 0.0;
  float vin = 0.0;
  word value = 0;

  value = (word)analogRead(BATTERY);

  vin = (1.23 * 1024.0) / value; 
  if (vin<0.09) {
   vin=0.0;//statement to quash undesired reading !
  }
  
  return (word)(vin*10);
  
}


void ResetTamperSensor()
{
    //... reset tamper status
   _tAMPER_SensorBinaryState=LOW;
   TAMPER_SensorBinaryState = _tAMPER_SensorBinaryState;
   sendTamperReport=true;
   
   tamperStayAwake=false;
   
}

// Getters and setters
void AlarmSetter(byte value) {
  
  AlarmState = value;
 
  //.. activate or deactivate siren and flash light connected to ALARM_PIN
  digitalWrite(ALARM_PIN, AlarmState ? HIGH : LOW);

  ResetTamperSensor();

}

byte AlarmGetter() {
  return AlarmState;
}

// request switch update on battery reports and reset tamper
void SwitchSetter(byte value) {
  
  SwitchState = value;
  
  //... get 9V battery level and send report
  send9VReport=true;
  
  //... get 3V battery level and send report
  send3VReport=true;

  ResetTamperSensor();
  
}

// 9V request switch state get
byte SwitchGetter() {
  return SwitchState;
}

// 9V battery status get
word NeunVoltGetter() {
  word value=get9VStatus();
  send9VReport=false;
  return value;
}

// 3V battery status get
word DreiVoltGetter() {
  word value=get3VStatus();
  send3VReport=false;
  return value;
}

// tamper sensor status get
byte TAMPER_SensorBinaryGetter() {
  sendTamperReport=false;
  return TAMPER_SensorBinaryState;
}