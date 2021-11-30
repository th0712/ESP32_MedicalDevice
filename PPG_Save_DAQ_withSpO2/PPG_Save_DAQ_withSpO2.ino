//////////////////////////////////////////////////////////////////////////////////////////
//
//   AFE44xx Arduino Firmware
//
//   Copyright (c) 2016 ProtoCentral
//
//   SpO2 computation based on original code from Maxim Integrated
//
//   This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//   NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//   For information on how to use the HealthyPi, visit https://github.com/Protocentral/afe44xx_Oximeter
/////////////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <SPI.h>
#include <math.h>
#include <stdio.h>

#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <AutoConnect.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ThingsBoard.h>


//wifi and device on Thingsboard
//#define WIFI_AP "NDSU IoT"
//#define WIFI_PASSWORD "bacondotwager"
#define TOKEN "AlfXtRanSlldLQG2tZNz"
char thingsboardServer[] = "134.129.122.213";
WiFiClient wifiClient;
ThingsBoard tb(wifiClient);
int status = WL_IDLE_STATUS;
unsigned long lastSend;

//Autoconnect Wifi
WebServer Server;
AutoConnect Portal(Server);
AutoConnectConfig Config;

//NTP library to get real time from server
#define NTP_OFFSET   0 * 60      // In seconds
#define NTP_INTERVAL 5 * 1000    // In miliseconds
#define NTP_ADDRESS  "pool.ntp.org"
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

//LED Time Variables
unsigned long current_time_LED;
unsigned long previous_time_LED;
unsigned long elapsed_time_LED = 0;
float voltage = 0.0f;
float percentage = 0.0f;
int battStatus = 5; //Start in default mode (not transmitting)
int ledState = LOW;   // ledState used to set the LED

//afe44xx Register definition
#define CONTROL0    0x00
#define LED2STC     0x01
#define LED2ENDC    0x02
#define LED2LEDSTC    0x03
#define LED2LEDENDC   0x04
#define ALED2STC    0x05
#define ALED2ENDC   0x06
#define LED1STC     0x07
#define LED1ENDC    0x08
#define LED1LEDSTC    0x09
#define LED1LEDENDC   0x0a
#define ALED1STC    0x0b
#define ALED1ENDC   0x0c
#define LED2CONVST    0x0d
#define LED2CONVEND   0x0e
#define ALED2CONVST   0x0f
#define ALED2CONVEND  0x10
#define LED1CONVST    0x11
#define LED1CONVEND   0x12
#define ALED1CONVST   0x13
#define ALED1CONVEND  0x14
#define ADCRSTCNT0    0x15
#define ADCRSTENDCT0  0x16
#define ADCRSTCNT1    0x17
#define ADCRSTENDCT1  0x18
#define ADCRSTCNT2    0x19
#define ADCRSTENDCT2  0x1a
#define ADCRSTCNT3    0x1b
#define ADCRSTENDCT3  0x1c
#define PRPCOUNT    0x1d
#define CONTROL1    0x1e
#define SPARE1      0x1f
#define TIAGAIN     0x20
#define TIA_AMB_GAIN  0x21
#define LEDCNTRL    0x22
#define CONTROL2    0x23
#define SPARE2      0x24
#define SPARE3      0x25
#define SPARE4      0x26
#define SPARE4      0x26
#define RESERVED1   0x27
#define RESERVED2   0x28
#define ALARM     0x29
#define LED2VAL     0x2a
#define ALED2VAL    0x2b
#define LED1VAL     0x2c
#define ALED1VAL    0x2d
#define LED2ABSVAL    0x2e
#define LED1ABSVAL    0x2f
#define DIAG      0x30
#define count 60

#define CES_CMDIF_PKT_START_1   0x0A
#define CES_CMDIF_PKT_START_2   0xFA
#define CES_CMDIF_TYPE_DATA   0x02
#define CES_CMDIF_PKT_STOP    0x0B


//int IRheartsignal[count];
//int Redheartsignal[count];
int IRdc[count];
int Reddc[count];
double difIRheartsig_dc;
double difREDheartsig_dc;
double powdifIR;
double powdifRed;
double IRac;
double Redac;
double SpOpercentage;
double Ratio;
unsigned long start_time;
unsigned long end_time;

//Pin declarations
const int SPISTE = 15;  // chip select - IO15
const int SPIDRDY = 4;  // data ready pin - IO4
volatile int drdy_trigger = LOW;
const int RESET = 0; // reset pin - IO0
const int PWDN = 2; // powerdown pin - IO2
#define GRN_LED 27          //TBD after soldering
#define RED_LED 26          //TBD after soldering
#define BATTERY_IN 39

void afe44xxInit (void);
void afe44xxWrite (uint8_t address, uint32_t data);
unsigned long afe44xxRead (uint8_t address);
signed long average_BPM( signed long );
volatile char DataPacketHeader[6];
volatile char DataPacket[10];
volatile char DataPacketFooter[2];
int datalen = 0x09;
//long unsigned int time;

volatile static int SPI_RX_Buff_Count = 0;
volatile char *SPI_RX_Buff_Ptr;
volatile int afe44xx_data_ready = false;
volatile unsigned int pckt = 0, buff = 0, t = 0;
unsigned long ueegtemp = 0, ueegtemp2 = 0;
unsigned long IRtemp, REDtemp;
signed long seegtemp = 0, seegtemp2 = 0;
volatile int i;


uint16_t aun_ir_buffer[100]; //infrared LED sensor data
uint16_t aun_red_buffer[100];  //red LED sensor data

#define FS            25    //sampling frequency
#define BUFFER_SIZE  (FS*4)
#define MA4_SIZE  4 // DONOT CHANGE
#define min(x,y) ((x) < (y) ? (x) : (y))

const uint8_t uch_spo2_table[184] = { 95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99,
                                      99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
                                      100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97,
                                      97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91,
                                      90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81,
                                      80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67,
                                      66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
                                      49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29,
                                      28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5,
                                      3, 2, 1
                                    } ;

static  int32_t an_x[ BUFFER_SIZE];
static  int32_t an_y[ BUFFER_SIZE];

volatile int8_t n_buffer_count; //data length

int32_t n_spo2;  //SPO2 value
int8_t ch_spo2_valid;  //indicator to show if the SPO2 calculation is valid
int32_t n_heart_rate; //heart rate value
int8_t  ch_hr_valid;  //indicator to show if the heart rate calculation is valid

long status_byte = 0;
uint8_t LeadStatus = 0;
boolean leadoff_deteted = true;
uint8_t spo2_probe_open = false;
int dec = 0;

//timestamp variables
unsigned long long timestamp;
unsigned long long real_time;
unsigned long long previous_ts;
int ii = 0;

void rootPage(){
  char content[] = "ESP32 Autoconnect Setup";
  Server.send(200, "text/plain", content);
}

void setup()
{
  Serial.begin(115200);
    // Serial.begin(57600);
  //Serial.begin(9600);
  //InitWiFi();
  
  // Enable saved past credential by autoReconnect option,
  // even once it is disconnected.
  Config.autoReconnect = false;
  Config.retainPortal = true;
  Config.autoRise = true;
  //Config.preserveAPMode = true;
  Config.immediateStart = true;
  Config.hostName = "esp32-01";
  Portal.config(Config);
  Server.on("/", rootPage);
  // Establish a connection with an autoReconnect option.
  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    Serial.println(WiFi.getHostname());
  }
  timeClient.begin();
  timeClient.update();
  timestamp = timeClient.getEpochTime();

  //set up for data saving
  Serial.println("CLEARDATA");
  Serial.println("LABEL,Date,Time,Timestamp,PPG_IR,PPG_Red");
  Serial.println("RESETTIMER");
  
  
  Serial.println("Intilazition AFE44xx.. ");
  delay(2000) ;   // pause for a moment
    //LED and battery read pins
  pinMode(GRN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BATTERY_IN, INPUT);
  digitalWrite(RED_LED, HIGH);
  analogReadResolution(12);
  //SPI.begin();
  SPI.begin(14,12,13,15);

  // set the directions
  pinMode (RESET, OUTPUT); //Slave Select
  pinMode (PWDN, OUTPUT); //Slave Select

  digitalWrite(RESET, LOW);
  delay(500);
  digitalWrite(RESET, HIGH);
  delay(500);
  digitalWrite(PWDN, LOW);
  delay(500);
  digitalWrite(PWDN, HIGH);
  delay(500);
  pinMode (SPISTE, OUTPUT); //Slave Select
  pinMode (SPIDRDY, INPUT); // data ready
  //  Serial.println(SPISTE);
  //  Serial.println(SS);
  //  Serial.println(digitalRead(SPISTE));
  //  Serial.println(digitalRead(SCK));
  //  Serial.println(digitalRead(MOSI));

  attachInterrupt(SPIDRDY, afe44xx_drdy_event, FALLING); // Digital2 is attached to Data ready pin of AFE is interrupt0 in ARduino
  //   attachInterrupt(0, afe44xx_drdy_event, RISING );
  // set SPI transmission
  SPI.setClockDivider (SPI_CLOCK_DIV8); // set Speed as 2MHz , 16MHz/ClockDiv
  //SPI.setDataMode (SPI_MODE0);          //Set SPI mode as 0
  SPI.setDataMode (SPI_MODE1);          //Set SPI mode as 1
  SPI.setBitOrder (MSBFIRST);           //MSB first

  // Packet structure
  DataPacketHeader[0] = CES_CMDIF_PKT_START_1;  //packet header1 0x0A
  DataPacketHeader[1] = CES_CMDIF_PKT_START_2;  //packet header2 0xFA
  DataPacketHeader[2] = datalen;                // data length 9
  DataPacketHeader[3] = (uint8_t)(datalen >> 8);
  DataPacketHeader[4] = CES_CMDIF_TYPE_DATA;

  DataPacketFooter[0] = 0x00;
  DataPacketFooter[1] = CES_CMDIF_PKT_STOP;
  afe44xxInit ();
  Serial.println("initialization is done");
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop()
{
  Portal.handleClient();
  timeClient.update();

    //voltage read
  voltage = ReadVoltage(BATTERY_IN);//ADC to voltage conversion
  percentage = 2808.3808*pow(voltage,4)-43560.9157*pow(voltage,3)+252848.5888*pow(voltage,2)-650767.4615*voltage+626532.5703; //curve fit of LiPo
  if(voltage > 4.19) percentage = 100; //upper limit
  if(voltage < 3.5) percentage = 0; //Lower limit
  
  if(percentage < 10){
    battStatus = 0;
  }else if(percentage < 50){
    battStatus = 1;
  }else{
    battStatus = 2;
  }

  current_time_LED = millis();                              //get current time for LED function
  elapsed_time_LED = current_time_LED - previous_time_LED;  //calculate elapsed time for LED function
  LEDFunction(battStatus);
  
  if ( !tb.connected() ) {
    reconnect();
  }
  if (drdy_trigger == HIGH)
  {
    //Serial.println("111111111xxxxxxx!!!");
    detachInterrupt(SPIDRDY);
    afe44xxWrite(CONTROL0, 0x000001);
    IRtemp = afe44xxRead(LED1VAL);
    afe44xxWrite(CONTROL0, 0x000001);
    REDtemp = afe44xxRead(LED2VAL);
    afe44xx_data_ready = true;
  }

  if (afe44xx_data_ready == true)
  {
    //Serial.println("xxxxxxx!!!");
    IRtemp = (unsigned long) (IRtemp << 10);
    seegtemp = (signed long) (IRtemp);
    seegtemp = (signed long) (seegtemp >> 10);

    REDtemp = (unsigned long) (REDtemp << 10);
    seegtemp2 = (signed long) (REDtemp);
    seegtemp2 = (signed long) (seegtemp2 >> 10);


    if (dec == 20)
    {
      //Serial.println("xoxoxo!!!");
      aun_ir_buffer[n_buffer_count] = (uint16_t) (seegtemp >> 4);
      aun_red_buffer[n_buffer_count] = (uint16_t) (seegtemp2 >> 4);

      //send data to Thingsboard
      real_time = timeClient.getEpochTime();
      getAndSendPPG(n_buffer_count, real_time);
      n_buffer_count++;     
      dec = 0;
      
    }
    dec++;

    if (n_buffer_count > 99)
    {
      //         Serial.println("xasdasdx!!!");
      estimate_spo2(aun_ir_buffer, 100, aun_red_buffer, &n_spo2, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);
            if (n_spo2 == -999)
              Serial.println("Probe error!!!!");
            else
            {

      Serial.print(" Sp02 : ");
      Serial.print(n_spo2);
      Serial.print("% ,");
      Serial.print("Pulse rate :");
      Serial.println(n_heart_rate);

      tb.sendTelemetryFloat("SpO2", n_spo2);
      tb.sendTelemetryFloat("Pulse rate", n_heart_rate);
            }
    n_buffer_count = 0;
    }
    afe44xx_data_ready = false;
    drdy_trigger = LOW; 
    attachInterrupt(SPIDRDY, afe44xx_drdy_event, FALLING );
    tb.loop();
  }
}
void getAndSendPPG(int n_buffer_count, unsigned long long real_time)
{
  // Prepare a JSON payload string
  char ts[13];
  if (real_time * 1000 > previous_ts)
    timestamp = real_time * 1000;
  else timestamp = previous_ts + 40; //add 40 ms for each data point
  previous_ts = timestamp;
  sprintf(ts, "%llu", timestamp);
  String payload = "{";
//  payload += "\"ts\":"; payload += ts; payload += ",";
//  payload += "\"values\":"; payload += "{";
  payload += "\"PPG_IR\":"; payload += aun_ir_buffer[n_buffer_count]; payload += ",";
  payload += "\"PPG_R\":"; payload += aun_red_buffer[n_buffer_count];
  payload += "}";
//  payload += "}";

  char PPG_data[1000];
  payload.toCharArray(PPG_data, 1000 );
  tb.sendTelemetryJson(PPG_data);

  //save data for PLX-DAQ serial monitor
      Serial.print("DATA,DATE,TIME,");
      Serial.print(ts);
      Serial.print(",");
      Serial.print(aun_ir_buffer[n_buffer_count]);
      Serial.print(",");
      Serial.print(aun_red_buffer[n_buffer_count]);
      Serial.println(",");
}


//void InitWiFi()
//{
//  Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network

  //WiFi.begin(WIFI_AP, WIFI_PASSWORD);
  //while (WiFi.status() != WL_CONNECTED) {
    //delay(500);
//    Serial.print(".");
 // }
//  Serial.println("Connected to AP");
//}


void reconnect() {
  // Loop until we're reconnected
  while (!tb.connected()) {
    //status = WiFi.status();
    //if ( status != WL_CONNECTED) {
      //WiFi.begin(WIFI_AP, WIFI_PASSWORD);
      //while (WiFi.status() != WL_CONNECTED) {
        //delay(500);
//        Serial.print(".");
      //}
      //Serial.println("Connected to AP");
    //}
    Serial.print("Connecting to ThingsBoard node ...");
    if ( tb.connect(thingsboardServer, TOKEN) ) {
      Serial.println( "[DONE]" );
    } else {
      Serial.print( "[FAILED]" );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}
///////// Gets Fired on DRDY event/////////////////////////////
ICACHE_RAM_ATTR void afe44xx_drdy_event()
{
  drdy_trigger = HIGH;
}

////////////////AFE44xx initialization//////////////////////////////////////////
void afe44xxInit (void)
{
  //  Serial.println("afe44xx Initialization Starts");
  afe44xxWrite(CONTROL0, 0x000000);

  afe44xxWrite(CONTROL0, 0x000008);

  afe44xxWrite(TIAGAIN, 0x000000); // CF = 5pF, RF = 500kR
  afe44xxWrite(TIA_AMB_GAIN, 0x000001);

  afe44xxWrite(LEDCNTRL, 0x001414);
  afe44xxWrite(CONTROL2, 0x000000); // LED_RANGE=100mA, LED=50mA
  afe44xxWrite(CONTROL1, 0x010707); // Timers ON, average 3 samples

  afe44xxWrite(PRPCOUNT, 0X001F3F);

  afe44xxWrite(LED2STC, 0X001770);
  afe44xxWrite(LED2ENDC, 0X001F3E);
  afe44xxWrite(LED2LEDSTC, 0X001770);
  afe44xxWrite(LED2LEDENDC, 0X001F3F);
  afe44xxWrite(ALED2STC, 0X000000);
  afe44xxWrite(ALED2ENDC, 0X0007CE);
  afe44xxWrite(LED2CONVST, 0X000002);
  afe44xxWrite(LED2CONVEND, 0X0007CF);
  afe44xxWrite(ALED2CONVST, 0X0007D2);
  afe44xxWrite(ALED2CONVEND, 0X000F9F);

  afe44xxWrite(LED1STC, 0X0007D0);
  afe44xxWrite(LED1ENDC, 0X000F9E);
  afe44xxWrite(LED1LEDSTC, 0X0007D0);
  afe44xxWrite(LED1LEDENDC, 0X000F9F);
  afe44xxWrite(ALED1STC, 0X000FA0);
  afe44xxWrite(ALED1ENDC, 0X00176E);
  afe44xxWrite(LED1CONVST, 0X000FA2);
  afe44xxWrite(LED1CONVEND, 0X00176F);
  afe44xxWrite(ALED1CONVST, 0X001772);
  afe44xxWrite(ALED1CONVEND, 0X001F3F);

  afe44xxWrite(ADCRSTCNT0, 0X000000);
  afe44xxWrite(ADCRSTENDCT0, 0X000000);
  afe44xxWrite(ADCRSTCNT1, 0X0007D0);
  afe44xxWrite(ADCRSTENDCT1, 0X0007D0);
  afe44xxWrite(ADCRSTCNT2, 0X000FA0);
  afe44xxWrite(ADCRSTENDCT2, 0X000FA0);
  afe44xxWrite(ADCRSTCNT3, 0X001770);
  afe44xxWrite(ADCRSTENDCT3, 0X001770);

  delay(1000);
//  Serial.println("afe44xx Initialization Done");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void afe44xxWrite (uint8_t address, uint32_t data)
{
  digitalWrite (SS, LOW); // enable device
  SPI.transfer (address); // send address to device
  SPI.transfer ((data >> 16) & 0xFF); // write top 8 bits
  SPI.transfer ((data >> 8) & 0xFF); // write middle 8 bits
  SPI.transfer (data & 0xFF); // write bottom 8 bits
  digitalWrite (SS, HIGH); // disable device
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned long afe44xxRead (uint8_t address)
{
  unsigned long data = 0;
  digitalWrite (SS, LOW); // enable device
  SPI.transfer (address); // send address to device
  //SPI.transfer (data);
  data |= ((unsigned long)SPI.transfer (0) << 16); // read top 8 bits data
  data |= ((unsigned long)SPI.transfer (0) << 8); // read middle 8 bits  data
  data |= SPI.transfer (0); // read bottom 8 bits data
  digitalWrite (SS, HIGH); // disable device


  return data; // return with 24 bits of read data
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void estimate_spo2(uint16_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint16_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid, int32_t *pn_heart_rate, int8_t *pch_hr_valid)
{
  uint32_t un_ir_mean, un_only_once ;
  int32_t k, n_i_ratio_count;
  int32_t i, s, m, n_exact_ir_valley_locs_count, n_middle_idx;
  int32_t n_th1, n_npks, n_c_min;
  int32_t an_ir_valley_locs[15] ;
  int32_t n_peak_interval_sum;

  int32_t n_y_ac, n_x_ac;
  int32_t n_spo2_calc;
  int32_t n_y_dc_max, n_x_dc_max;
  int32_t n_y_dc_max_idx, n_x_dc_max_idx;
  int32_t an_ratio[5], n_ratio_average;
  int32_t n_nume, n_denom ;

  // calculates DC mean and subtract DC from ir
  un_ir_mean = 0;
  for (k = 0 ; k < n_ir_buffer_length ; k++ ) un_ir_mean += pun_ir_buffer[k] ;
  un_ir_mean = un_ir_mean / n_ir_buffer_length ;

  // remove DC and invert signal so that we can use peak detector as valley detector
  for (k = 0 ; k < n_ir_buffer_length ; k++ )
    an_x[k] = -1 * (pun_ir_buffer[k] - un_ir_mean) ;

  // 4 pt Moving Average
  for (k = 0; k < BUFFER_SIZE - MA4_SIZE; k++) {
    an_x[k] = ( an_x[k] + an_x[k + 1] + an_x[k + 2] + an_x[k + 3]) / (int)4;
  }
  // calculate threshold
  n_th1 = 0;
  for ( k = 0 ; k < BUFFER_SIZE ; k++) {
    n_th1 +=  an_x[k];
  }
  n_th1 =  n_th1 / ( BUFFER_SIZE);
  if ( n_th1 < 30) n_th1 = 30; // min allowed
  if ( n_th1 > 60) n_th1 = 60; // max allowed

  for ( k = 0 ; k < 15; k++) an_ir_valley_locs[k] = 0;
  // since we flipped signal, we use peak detector as valley detector
  find_peak( an_ir_valley_locs, &n_npks, an_x, BUFFER_SIZE, n_th1, 4, 15 );//peak_height, peak_distance, max_num_peaks
  n_peak_interval_sum = 0;
  if (n_npks >= 2) {
    for (k = 1; k < n_npks; k++) n_peak_interval_sum += (an_ir_valley_locs[k] - an_ir_valley_locs[k - 1] ) ;
    n_peak_interval_sum = n_peak_interval_sum / (n_npks - 1);
    *pn_heart_rate = (int32_t)( (FS * 60) / n_peak_interval_sum );
    *pch_hr_valid  = 1;
  }
  else  {
    *pn_heart_rate = -999; // unable to calculate because # of peaks are too small
    *pch_hr_valid  = 0;
  }

  //  load raw value again for SPO2 calculation : RED(=y) and IR(=X)
  for (k = 0 ; k < n_ir_buffer_length ; k++ )  {
    an_x[k] =  pun_ir_buffer[k] ;
    an_y[k] =  pun_red_buffer[k] ;
  }

  // find precise min near an_ir_valley_locs
  n_exact_ir_valley_locs_count = n_npks;

  //using exact_ir_valley_locs , find ir-red DC andir-red AC for SPO2 calibration an_ratio
  //finding AC/DC maximum of raw

  n_ratio_average = 0;
  n_i_ratio_count = 0;
  for (k = 0; k < 5; k++) an_ratio[k] = 0;
  for (k = 0; k < n_exact_ir_valley_locs_count; k++) {
    if (an_ir_valley_locs[k] > BUFFER_SIZE ) {
      *pn_spo2 =  -999 ; // do not use SPO2 since valley loc is out of range
      *pch_spo2_valid  = 0;
      return;
    }
  }
  // find max between two valley locations
  // and use an_ratio betwen AC compoent of Ir & Red and DC compoent of Ir & Red for SPO2
  for (k = 0; k < n_exact_ir_valley_locs_count - 1; k++) {
    n_y_dc_max = -16777216 ;
    n_x_dc_max = -16777216;
    if (an_ir_valley_locs[k + 1] - an_ir_valley_locs[k] > 3) {
      for (i = an_ir_valley_locs[k]; i < an_ir_valley_locs[k + 1]; i++) {
        if (an_x[i] > n_x_dc_max) {
          n_x_dc_max = an_x[i];
          n_x_dc_max_idx = i;
        }
        if (an_y[i] > n_y_dc_max) {
          n_y_dc_max = an_y[i];
          n_y_dc_max_idx = i;
        }
      }
      n_y_ac = (an_y[an_ir_valley_locs[k + 1]] - an_y[an_ir_valley_locs[k] ] ) * (n_y_dc_max_idx - an_ir_valley_locs[k]); //red
      n_y_ac =  an_y[an_ir_valley_locs[k]] + n_y_ac / (an_ir_valley_locs[k + 1] - an_ir_valley_locs[k])  ;
      n_y_ac =  an_y[n_y_dc_max_idx] - n_y_ac;   // subracting linear DC compoenents from raw
      n_x_ac = (an_x[an_ir_valley_locs[k + 1]] - an_x[an_ir_valley_locs[k] ] ) * (n_x_dc_max_idx - an_ir_valley_locs[k]); // ir
      n_x_ac =  an_x[an_ir_valley_locs[k]] + n_x_ac / (an_ir_valley_locs[k + 1] - an_ir_valley_locs[k]);
      n_x_ac =  an_x[n_y_dc_max_idx] - n_x_ac;     // subracting linear DC compoenents from raw
      n_nume = ( n_y_ac * n_x_dc_max) >> 7 ; //prepare X100 to preserve floating value
      n_denom = ( n_x_ac * n_y_dc_max) >> 7;
      if (n_denom > 0  && n_i_ratio_count < 5 &&  n_nume != 0)
      {
        an_ratio[n_i_ratio_count] = (n_nume * 100) / n_denom ; //formular is ( n_y_ac *n_x_dc_max) / ( n_x_ac *n_y_dc_max) ;
        n_i_ratio_count++;
      }
    }
  }
  // choose median value since PPG signal may varies from beat to beat
  sort_ascend(an_ratio, n_i_ratio_count);
  n_middle_idx = n_i_ratio_count / 2;

  if (n_middle_idx > 1)
    n_ratio_average = ( an_ratio[n_middle_idx - 1] + an_ratio[n_middle_idx]) / 2; // use median
  else
    n_ratio_average = an_ratio[n_middle_idx ];

  if ( n_ratio_average > 2 && n_ratio_average < 184) {
    n_spo2_calc = uch_spo2_table[n_ratio_average] ;
    *pn_spo2 = n_spo2_calc ;
    *pch_spo2_valid  = 1;//  float_SPO2 =  -45.060*n_ratio_average* n_ratio_average/10000 + 30.354 *n_ratio_average/100 + 94.845 ;  // for comparison with table
  }
  else {
    *pn_spo2 =  -999 ; // do not use SPO2 since signal an_ratio is out of range
    *pch_spo2_valid  = 0;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void find_peak( int32_t *pn_locs, int32_t *n_npks,  int32_t  *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num )
/**
  \brief        Find peaks
  \par          Details
                Find at most MAX_NUM peaks above MIN_HEIGHT separated by at least MIN_DISTANCE

  \retval       None
*/
{
  find_peak_above( pn_locs, n_npks, pn_x, n_size, n_min_height );
  remove_close_peaks( pn_locs, n_npks, pn_x, n_min_distance );
  *n_npks = min( *n_npks, n_max_num );
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void find_peak_above( int32_t *pn_locs, int32_t *n_npks,  int32_t  *pn_x, int32_t n_size, int32_t n_min_height )
/**
  \brief        Find peaks above n_min_height
  \par          Details
                Find all peaks above MIN_HEIGHT

  \retval       None
*/
{
  int32_t i = 1, n_width;
  *n_npks = 0;

  while (i < n_size - 1) {
    if (pn_x[i] > n_min_height && pn_x[i] > pn_x[i - 1]) {   // find left edge of potential peaks
      n_width = 1;
      while (i + n_width < n_size && pn_x[i] == pn_x[i + n_width]) // find flat peaks
        n_width++;
      if (pn_x[i] > pn_x[i + n_width] && (*n_npks) < 15 ) {   // find right edge of peaks
        pn_locs[(*n_npks)++] = i;
        // for flat peaks, peak location is left edge
        i += n_width + 1;
      }
      else
        i += n_width;
    }
    else
      i++;
    //  Serial.println("beat");
  }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance)
/**
  \brief        Remove peaks
  \par          Details
                Remove peaks separated by less than MIN_DISTANCE

  \retval       None
*/
{

  int32_t i, j, n_old_npks, n_dist;

  /* Order peaks from large to small */
  sort_indices_descend( pn_x, pn_locs, *pn_npks );

  for ( i = -1; i < *pn_npks; i++ ) {
    n_old_npks = *pn_npks;
    *pn_npks = i + 1;
    for ( j = i + 1; j < n_old_npks; j++ ) {
      n_dist =  pn_locs[j] - ( i == -1 ? -1 : pn_locs[i] ); // lag-zero peak of autocorr is at index -1
      if ( n_dist > n_min_distance || n_dist < -n_min_distance )
        pn_locs[(*pn_npks)++] = pn_locs[j];
    }
  }

  // Resort indices int32_to ascending order
  sort_ascend( pn_locs, *pn_npks );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sort_ascend(int32_t  *pn_x, int32_t n_size)
/**
  \brief        Sort array
  \par          Details
                Sort array in ascending order (insertion sort algorithm)

  \retval       None
*/
{
  int32_t i, j, n_temp;
  for (i = 1; i < n_size; i++) {
    n_temp = pn_x[i];
    for (j = i; j > 0 && n_temp < pn_x[j - 1]; j--)
      pn_x[j] = pn_x[j - 1];
    pn_x[j] = n_temp;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sort_indices_descend(  int32_t  *pn_x, int32_t *pn_indx, int32_t n_size)
/**
  \brief        Sort indices
  \par          Details
                Sort indices according to descending order (insertion sort algorithm)

  \retval       None
*/
{
  int32_t i, j, n_temp;
  for (i = 1; i < n_size; i++) {
    n_temp = pn_indx[i];
    for (j = i; j > 0 && pn_x[n_temp] > pn_x[pn_indx[j - 1]]; j--)
      pn_indx[j] = pn_indx[j - 1];
    pn_indx[j] = n_temp;
  }
}
double ReadVoltage(uint8_t pin){
  double reading = analogRead(pin); // Reference voltage is 3v3 so maximum reading is 3v3 = 4095 in range 0 to 4095
  if(reading < 1 || reading > 4095) return 0;
  //return (-0.000000000000016 * pow(reading,4) + 0.000000000118171 * pow(reading,3)- 0.000000301211691 * pow(reading,2)+ 0.001109019271794 * reading + 0.034143524634089)*2; //original fir from creator
  return (-.0000000000000096795072211912461* pow(reading,4) + .000000000064564581092594387 * pow(reading,3) - .00000014328287130333392 * pow(reading,2)+ .00090565621090209041 * reading + .11253959753849530)*2;
} 

void LEDFunction (int battStatus){
  switch(battStatus){
    case 0: //Battery criticially low (less than 10%)
    {
      if(elapsed_time_LED > 1000){
        // if the LED is off turn it on and vice-versa:
        ledState = (ledState == LOW) ? HIGH : LOW;

        // set the LED with the ledState of the variable:
        digitalWrite(GRN_LED, LOW);
        digitalWrite(RED_LED, ledState);
        previous_time_LED = millis();
      }
      break;
    }
    case 1: //Battery < 20%
    {
      if(elapsed_time_LED > 1000){
        // if the LED is off turn it on and vice-versa:
        ledState = (ledState == LOW) ? HIGH : LOW;

        // set the LED with the ledState of the variable:
        digitalWrite(GRN_LED, LOW);
        digitalWrite(RED_LED, ledState);
        previous_time_LED = millis();
      }
      break;
    }
    case 2: //Battery > 20%
    {
      if(elapsed_time_LED > 2000){
        // if the LED is off turn it on and vice-versa:
        ledState = (ledState == LOW) ? HIGH : LOW;

        // set the LED with the ledState of the variable:
        digitalWrite(GRN_LED, ledState);
        digitalWrite(RED_LED, LOW);
        previous_time_LED = millis();
      }
      break;
    }
    default:
      break;
    }
  }
