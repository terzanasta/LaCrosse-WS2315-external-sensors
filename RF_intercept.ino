/* Intercepting, La crosse ws-23xx series, external sensors RF transmition */

#define MSG_SIZE 44
#define BIT1_LENGTH 380
#define BIT0_LENGTH 1480
#define TOL 110      //time tolerance for pulses in microseconds
#define SIGNATURE 9  //Weather Station signature for synch. ws-2315-00001001, ws-3600-00000110
#define DATAPIN 2    // Pin 2,  interrupt 0

byte bits[MSG_SIZE];
byte syncSignature = 0;
boolean syncED = 0;
bool received = false;
float T, H, Ws, Wd, Wg, R, P;  //Temperature, Humidity, WindSpeed, WindDirection, WindGustRain,Pressure
int ID, sensor;

/* Interrupt for decoding received pulses */
void PulseDecode() {
  static unsigned long duration = 0;
  static unsigned long lastTime = 0;
  static byte Pindex = 0;
  static unsigned int Pulse = 0;
  static unsigned int lastPulse = 0;
  static boolean HIGHperiod = false;

  // ignore if we haven't processed the previous received signal
  if (received == true)
    return;
  long time = micros();
  Pulse = digitalRead(DATAPIN);
  if (lastPulse == HIGH && Pulse == LOW)
    HIGHperiod = true;
  else
    HIGHperiod = false;

  lastPulse = Pulse;

  duration = time - lastTime;
  lastTime = time;

  if (duration < BIT1_LENGTH - TOL)  //disregard any pulses with time different from the limits
    return;
  if (duration > BIT0_LENGTH + TOL)
    return;
  if (HIGHperiod) {
    if ((duration > BIT1_LENGTH - TOL) && (duration < BIT1_LENGTH + TOL)) {
      if (syncED) {
        bits[Pindex] = 1;
        Pindex++;
      } else {
        syncSignature = syncSignature << 1;
        syncSignature++;
      }
    }
    if ((duration > BIT0_LENGTH - TOL) && (duration < BIT0_LENGTH + TOL)) {
      if (syncED) {
        bits[Pindex] = 0;
        Pindex++;
      } else {
        syncSignature = syncSignature << 1;
      }
    }

    if ((syncSignature ^ SIGNATURE) == 0)  //Check if we have a signature, XOR to see if equal
      syncED = true;
    else
      syncED = false;

    if (Pindex > MSG_SIZE - 1 && syncED) {
      received = true;
      syncED = false;
      //syncSignature = 0;
      return;
    }

    if (Pindex > MSG_SIZE - 1 && !syncED) {
      //Serial.println(Pindex);
      Pindex = 0;
      syncSignature = 0;
      received = false;
      return;
    }
  }
}

long int dataTimer;

void setup() {
  Serial.begin(57600);
  Serial.println("Started.Listening to RF packet");
  pinMode(DATAPIN, INPUT);
  dataTimer = millis();
  attachInterrupt(digitalPinToInterrupt(DATAPIN), PulseDecode, CHANGE);  //(recommended)
}

void loop() {
  if (received == true) {
    detachInterrupt(digitalPinToInterrupt(DATAPIN));
    syncSignature = 0;
    received = false;
    decodeRFtransmition();

    if (sensor == 0) {
      Serial.print("T = ");
      Serial.println(T, 1);
    }
    if (sensor == 1) {
      Serial.print("H = ");
      Serial.println(H, 0);
    }
    if (sensor == 3) {
      Serial.print("Wd = ");
      Serial.println(Wd, 1);
      Serial.print("Ws = ");
      Serial.println(Ws, 1);
    }
    if (sensor == 7) {
      Serial.print("Wg = ");
      Serial.println(Wg, 1);
    }
    if (sensor == 2) {
      Serial.print("R = ");
      Serial.println(R, 2);
    }

    // re-enable interrupt
    attachInterrupt(digitalPinToInterrupt(DATAPIN), PulseDecode, CHANGE);
  }
}

void decodeRFtransmition() {
  int i, ck1, ck2;
  int temp1, temp2, temp3, parity, chksum, WSchksum;
  float val1, val2, val3;

  /*
    Packet Format is 52 bits
    |  bits | nibble
    | ----- | ------
    |  0- 3 | 0 - 0000
    |  4- 7 | 1 - 1001 for WS-2310, 0110 for WS-3600
    The first 8 bits are for signature, we search values from bit 8 and above
    substract 8 from bit position for calculations
    |  8-11 | 2 - Type  GPTT  G=0, P=Parity, Gust=Gust, TT=Type  GTT 000=Temp, 001=Humidity, 010=Rain, 011=Wind, 111-Gust
    | 12-15 | 3 - ID High
    | 16-19 | 4 - ID Low
    | 20-23 | 5 - Data Types  GWRH  G=Gust Sent, W=Wind Sent, R=Rain Sent, H=Humidity Sent
    | 24-27 | 6 - Parity TUU? T=Temp Sent, UU=Next Update, 00=8 seconds, 01=32 seconds, 10=?, 11=128 seconds, ?=?
    | 28-31 | 7 - Value1
    | 32-35 | 8 - Value2
    | 36-39 | 9 - Value3
    | 40-43 | 10 - ~Value1
    | 44-47 | 11 - ~Value2
    | 48-51 | 12 - Check Sum = Nibble sum of nibbles 0-11
  */
  parity = 0;
  for (i = 0; i < MSG_SIZE; i++) {
    if (i == 1 || (i >= 19 && i <= 31))
      parity += bits[i];
  }
  parity = parity & 0x1;

  chksum = 9;  //(WS-2315 type)
  for (i = 0; i < MSG_SIZE - 4; i = i + 4) {
    chksum += 8 * bits[i] + 4 * bits[i + 1] + 2 * bits[i + 2] + bits[i + 3];
  }
  chksum = chksum & 0x0F;

  ck1 = 8 * bits[32] + 4 * bits[33] + 2 * bits[34] + bits[35];
  ck2 = 8 * bits[36] + 4 * bits[37] + 2 * bits[38] + bits[39];
  ck1 = ck1 ^ 0xF;  //XOR
  ck2 = ck2 ^ 0xF;  //XOR

  WSchksum = 8 * bits[40] + 4 * bits[41] + 2 * bits[42] + bits[43];

  temp1 = 8 * bits[20] + 4 * bits[21] + 2 * bits[22] + bits[23];
  temp2 = 8 * bits[24] + 4 * bits[25] + 2 * bits[26] + bits[27];
  temp3 = 8 * bits[28] + 4 * bits[29] + 2 * bits[30] + bits[31];

  if (chksum == WSchksum && temp1 == ck1 && temp2 == ck2 && parity == 0x1) {

    val1 = temp1 * 100.0 + temp2 * 10.0 + temp3;
    val2 = temp1 * 10.0 + temp2;
    val3 = temp1 * 256.0 + temp2 * 16.0 + temp3;
    ;

    ID = 128 * bits[4] + 64 * bits[5] + 32 * bits[6] + 16 * bits[7] + 8 * bits[8] + 4 * bits[9] + 2 * bits[10] + bits[11];
    ;
    sensor = 4 * bits[0] + 2 * bits[2] + bits[3];

    switch (sensor) {
      case 0:                      //   temperature
        T = (val1 - 300.0) * 0.1;  // val1-400 for WS3600
        break;
      case 1:  //   humidity
        H = val2;
        break;
      case 2:  //   Rain
        R = 0.518 * val3;
        break;
      case 3:  //   Wind speed and Direction
        Wd = temp3 * 22.5;
        Ws = (temp1 * 16.0 + temp2) * 0.36;  //km/h
        break;
      case 7:  //   Wind gust and Direction
        Wg = (temp1 * 16.0 + temp2) * 0.36;
        Wd = temp3 * 22.5;
        break;
      default:
        break;
    }
  }
}
