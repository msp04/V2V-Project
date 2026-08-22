// =====================================================
// ROVER 2 - ESP32 V2V ROVER
// Bluetooth + WiFi UDP V2V + MPU6050 + Motor Control
// =====================================================

#include <WiFi.h>
#include <WiFiUdp.h>
#include "BluetoothSerial.h"
#include <Wire.h>
#include <math.h>
#include <string.h>

// ===============================
// ROVER ID
// ===============================
#define VEHICLE_ID 1

// =====================================================
// WIFI CONFIG
// =====================================================

const char* WIFI_SSID     = "Samruddhi";
const char* WIFI_PASSWORD = "Sammu@2005";

const uint16_t V2V_PORT = 4210;
IPAddress broadcastIP;

WiFiUDP udp;

// =====================================================
// MPU6050 CONFIGURATION
// =====================================================

#define MPU_ADDR 0x68

#define MPU_SDA 25
#define MPU_SCL 33

#define PWR_MGMT_1   0x6B
#define SMPLRT_DIV   0x19
#define CONFIG_REG   0x1A
#define GYRO_CONFIG  0x1B
#define ACCEL_CONFIG 0x1C
#define ACCEL_XOUT_H 0x3B

// =====================================================
// TIMING
// =====================================================

unsigned long lastConnectionCheck = 0;
unsigned long lastDisplay = 0;
unsigned long lastSpeedUpdate = 0;
unsigned long lastMPURead = 0;
unsigned long lastApproachMessage = 0;

const unsigned long CONNECTION_CHECK_INTERVAL = 10000;
const unsigned long DISPLAY_INTERVAL = 2000;
const unsigned long MPU_READ_INTERVAL = 50;

// =====================================================
// MPU STATUS
// =====================================================

bool mpuConnected = false;

// =====================================================
// CALIBRATION OFFSETS
// =====================================================

float accelOffsetX = 0.0;
float accelOffsetY = 0.0;
float accelOffsetZ = 0.0;

float gyroOffsetX = 0.0;
float gyroOffsetY = 0.0;
float gyroOffsetZ = 0.0;

// =====================================================
// SPEED
// =====================================================

float speedMS = 0.0;
float speedKMPH = 0.0;

// =====================================================
// ACCIDENT THRESHOLDS
// =====================================================

const float IMPACT_THRESHOLD = 3.0;
const float SEVERE_IMPACT_THRESHOLD = 5.0;
const float ROLLOVER_ANGLE = 60.0;
const float ROTATION_THRESHOLD = 180.0;

bool tiltAlreadyReported = false;
bool impactAlreadyReported = false;

char orientation[24] = "UPRIGHT";

// =====================================================
// RAW MPU VALUES
// =====================================================

int16_t curRawAx = 0;
int16_t curRawAy = 0;
int16_t curRawAz = 0;

// =====================================================
// MOTOR DRIVER
// =====================================================

#define IN1 26
#define IN2 27
#define IN3 12
#define IN4 13

#define INVERT_LEFT   false
#define INVERT_RIGHT  false

// =====================================================
// MOVEMENT
// =====================================================

#define STOPPED  0
#define FORWARD  1
#define BACKWARD 2
#define LEFT     3
#define RIGHT    4

enum MotorDir {
  M_STOP,
  M_FWD,
  M_BWD
};

uint8_t movement = STOPPED;

// =====================================================
// BLUETOOTH
// =====================================================

BluetoothSerial BT;

// =====================================================
// V2V PACKET
// =====================================================

typedef struct {
  uint8_t id;
  uint8_t movement;

  int16_t ax;
  int16_t ay;
  int16_t az;

  uint32_t time;
} V2VPacket;

V2VPacket myPacket;
V2VPacket otherPacket;

uint32_t lastSend = 0;

// =====================================================
// VEHICLE APPROACHING DETECTION
// =====================================================

bool vehicleDetected = false;

uint8_t approachingVehicleID = 0;

unsigned long lastVehicleSeen = 0;

const unsigned long VEHICLE_TIMEOUT = 1000;
const unsigned long APPROACH_MESSAGE_INTERVAL = 2000;

// =====================================================
// CHECK MPU6050
// =====================================================

bool checkMPU6050()
{
  Wire.beginTransmission(MPU_ADDR);

  byte error = Wire.endTransmission();

  return (error == 0);
}

// =====================================================
// WRITE REGISTER
// =====================================================

void writeRegister(byte reg, byte value)
{
  Wire.beginTransmission(MPU_ADDR);

  Wire.write(reg);
  Wire.write(value);

  Wire.endTransmission();
}

// =====================================================
// INITIALIZE MPU6050
// =====================================================

bool initializeMPU()
{
  if (!checkMPU6050())
  {
    return false;
  }

  writeRegister(PWR_MGMT_1, 0x00);

  delay(100);

  writeRegister(SMPLRT_DIV, 0x07);
  writeRegister(CONFIG_REG, 0x03);
  writeRegister(GYRO_CONFIG, 0x08);
  writeRegister(ACCEL_CONFIG, 0x10);

  delay(100);

  return checkMPU6050();
}

// =====================================================
// READ MPU6050
// =====================================================

bool readMPU6050(
  int16_t &ax,
  int16_t &ay,
  int16_t &az,
  int16_t &gx,
  int16_t &gy,
  int16_t &gz
)
{
  Wire.beginTransmission(MPU_ADDR);

  Wire.write(ACCEL_XOUT_H);

  if (Wire.endTransmission(false) != 0)
  {
    return false;
  }

  Wire.requestFrom(MPU_ADDR, 14);

  if (Wire.available() < 14)
  {
    return false;
  }

  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();

  // Temperature
  Wire.read();
  Wire.read();

  gx = (Wire.read() << 8) | Wire.read();
  gy = (Wire.read() << 8) | Wire.read();
  gz = (Wire.read() << 8) | Wire.read();

  return true;
}

// =====================================================
// CALIBRATION
// =====================================================

bool calibrateMPU()
{
  delay(10);

  const int samples = 300;

  long long sumAx = 0;
  long long sumAy = 0;
  long long sumAz = 0;

  long long sumGx = 0;
  long long sumGy = 0;
  long long sumGz = 0;

  int validSamples = 0;

  for (int i = 0; i < samples; i++)
  {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;

    if (readMPU6050(ax, ay, az, gx, gy, gz))
    {
      sumAx += ax;
      sumAy += ay;
      sumAz += az;

      sumGx += gx;
      sumGy += gy;
      sumGz += gz;

      validSamples++;
    }

    delay(10);
  }

  if (validSamples < 200)
  {
    return false;
  }

  float avgAx = (float)sumAx / validSamples;
  float avgAy = (float)sumAy / validSamples;
  float avgAz = (float)sumAz / validSamples;

  float avgGx = (float)sumGx / validSamples;
  float avgGy = (float)sumGy / validSamples;
  float avgGz = (float)sumGz / validSamples;

  float accelX = avgAx / 4096.0;
  float accelY = avgAy / 4096.0;
  float accelZ = avgAz / 4096.0;

  if (fabs(accelX) > 0.7)
    accelOffsetX = accelX - ((accelX > 0) ? 1.0 : -1.0);
  else
    accelOffsetX = accelX;

  if (fabs(accelY) > 0.7)
    accelOffsetY = accelY - ((accelY > 0) ? 1.0 : -1.0);
  else
    accelOffsetY = accelY;

  if (fabs(accelZ) > 0.7)
    accelOffsetZ = accelZ - ((accelZ > 0) ? 1.0 : -1.0);
  else
    accelOffsetZ = accelZ;

  gyroOffsetX = avgGx / 65.5;
  gyroOffsetY = avgGy / 65.5;
  gyroOffsetZ = avgGz / 65.5;

  delay(500);

  return true;
}

// =====================================================
// UPDATE MPU
// =====================================================

void updateMPU()
{
  int16_t rawAx, rawAy, rawAz;
  int16_t rawGx, rawGy, rawGz;

  if (!readMPU6050(rawAx, rawAy, rawAz, rawGx, rawGy, rawGz))
  {
    mpuConnected = false;
    return;
  }

  curRawAx = rawAx;
  curRawAy = rawAy;
  curRawAz = rawAz;

  float rawAccelX = rawAx / 4096.0;
  float rawAccelY = rawAy / 4096.0;
  float rawAccelZ = rawAz / 4096.0;

  float ax = rawAccelX - accelOffsetX;
  float ay = rawAccelY - accelOffsetY;
  float az = rawAccelZ - accelOffsetZ;

  float gyroX = (rawGx / 65.5) - gyroOffsetX;
  float gyroY = (rawGy / 65.5) - gyroOffsetY;
  float gyroZ = (rawGz / 65.5) - gyroOffsetZ;

  float totalAcceleration =
    sqrt(ax * ax + ay * ay + az * az);

  // =================================================
  // ORIENTATION
  // =================================================

  float roll =
    atan2(rawAccelY, rawAccelZ) * 180.0 / PI;

  float pitch =
    atan2(
      -rawAccelX,
      sqrt(rawAccelY * rawAccelY +
           rawAccelZ * rawAccelZ)
    ) * 180.0 / PI;

  if (fabs(roll) < 30 && fabs(pitch) < 30)
  {
    strcpy(orientation, "UPRIGHT");
  }
  else if (roll >= 30 && roll < 60)
  {
    strcpy(orientation, "TILTED RIGHT");
  }
  else if (roll <= -30 && roll > -60)
  {
    strcpy(orientation, "TILTED LEFT");
  }
  else if (pitch >= 30 && pitch < 60)
  {
    strcpy(orientation, "TILTED BACKWARD");
  }
  else if (pitch <= -30 && pitch > -60)
  {
    strcpy(orientation, "TILTED FORWARD");
  }
  else
  {
    strcpy(orientation, "ROLLOVER / EXTREME TILT");
  }

  // =================================================
  // SPEED ESTIMATION
  // =================================================

  float linearAcceleration =
    totalAcceleration - 1.0;

  if (fabs(linearAcceleration) < 0.05)
  {
    linearAcceleration = 0.0;
  }

  unsigned long currentTime = millis();

  float dt =
    (currentTime - lastSpeedUpdate) / 1000.0;

  lastSpeedUpdate = currentTime;

  if (dt > 0 && dt < 1.0)
  {
    speedMS +=
      linearAcceleration * 9.80665 * dt;
  }

  if (speedMS < 0)
  {
    speedMS = 0;
  }

  speedKMPH = speedMS * 3.6;

  if (fabs(totalAcceleration - 1.0) < 0.06)
  {
    speedMS *= 0.90;

    speedKMPH = speedMS * 3.6;

    if (speedKMPH < 0.5)
    {
      speedKMPH = 0.0;
      speedMS = 0.0;
    }
  }

  if (speedKMPH > 200)
  {
    speedKMPH = 200.0;

    speedMS = speedKMPH / 3.6;
  }

  // =================================================
  // ACCIDENT DETECTION
  // =================================================

  bool impact =
    (totalAcceleration >= IMPACT_THRESHOLD);

  bool severeImpact =
    (totalAcceleration >= SEVERE_IMPACT_THRESHOLD);

  bool rollover =
    (fabs(roll) >= ROLLOVER_ANGLE ||
     fabs(pitch) >= ROLLOVER_ANGLE);

  bool abnormalRotation =
    (fabs(gyroX) >= ROTATION_THRESHOLD ||
     fabs(gyroY) >= ROTATION_THRESHOLD ||
     fabs(gyroZ) >= ROTATION_THRESHOLD);

  bool accidentDetected =
    impact ||
    severeImpact ||
    rollover ||
    abnormalRotation;

  (void)accidentDetected;

  // =================================================
  // TILT MESSAGE
  // =================================================

  if (rollover && !tiltAlreadyReported)
  {
    tiltAlreadyReported = true;

    Serial.print("!!! TILT DETECTED !!! Roll: ");
    Serial.print(roll, 1);

    Serial.print(" Pitch: ");
    Serial.println(pitch, 1);
  }
  else if (!rollover && tiltAlreadyReported)
  {
    tiltAlreadyReported = false;
  }

  // =================================================
  // IMPACT MESSAGE
  // =================================================

  if (impact && !impactAlreadyReported)
  {
    impactAlreadyReported = true;

    if (severeImpact)
    {
      Serial.print("!!! SEVERE IMPACT DETECTED !!! ");
    }
    else
    {
      Serial.print("!!! IMPACT DETECTED !!! ");
    }

    Serial.print("Accel: ");
    Serial.print(totalAcceleration, 2);
    Serial.println("g");
  }
  else if (!impact && impactAlreadyReported)
  {
    impactAlreadyReported = false;
  }

  // =================================================
  // SPEED + ORIENTATION DISPLAY
  // =================================================

  if (millis() - lastDisplay >= DISPLAY_INTERVAL)
  {
    lastDisplay = millis();

    Serial.print("Speed: ");
    Serial.print(speedKMPH, 2);

    Serial.print(" km/h | Orientation: ");
    Serial.println(orientation);
  }
}

// =====================================================
// LOW LEVEL MOTOR CONTROL
// =====================================================

void leftMotor(MotorDir dir)
{
  if (INVERT_LEFT)
  {
    if (dir == M_FWD)
      dir = M_BWD;

    else if (dir == M_BWD)
      dir = M_FWD;
  }

  switch (dir)
  {
    case M_FWD:

      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);

      break;

    case M_BWD:

      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);

      break;

    default:

      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);

      break;
  }
}

// =====================================================
// RIGHT MOTOR
// =====================================================

void rightMotor(MotorDir dir)
{
  if (INVERT_RIGHT)
  {
    if (dir == M_FWD)
      dir = M_BWD;

    else if (dir == M_BWD)
      dir = M_FWD;
  }

  switch (dir)
  {
    case M_FWD:

      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);

      break;

    case M_BWD:

      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);

      break;

    default:

      digitalWrite(IN3, LOW);
      digitalWrite(IN4, LOW);

      break;
  }
}

// =====================================================
// VEHICLE MOVEMENT
// =====================================================

void stopVehicle()
{
  leftMotor(M_STOP);
  rightMotor(M_STOP);

  movement = STOPPED;
}

void forward()
{
  leftMotor(M_FWD);
  rightMotor(M_FWD);

  movement = FORWARD;
}

void backward()
{
  leftMotor(M_BWD);
  rightMotor(M_BWD);

  movement = BACKWARD;
}

void left()
{
  leftMotor(M_BWD);
  rightMotor(M_FWD);

  movement = LEFT;
}

void right()
{
  leftMotor(M_FWD);
  rightMotor(M_BWD);

  movement = RIGHT;
}

// =====================================================
// BLUETOOTH CONTROL
// =====================================================

void bluetoothControl()
{
  if (!BT.available())
    return;

  char c = BT.read();

  if (c == 'F' || c == 'f')
    forward();

  else if (c == 'B' || c == 'b')
    backward();

  else if (c == 'L' || c == 'l')
    left();

  else if (c == 'R' || c == 'r')
    right();

  else if (c == 'S' || c == 's')
    stopVehicle();
}

// =====================================================
// COMPUTE WIFI BROADCAST ADDRESS
// =====================================================

IPAddress computeBroadcastIP()
{
  IPAddress ip = WiFi.localIP();

  IPAddress mask = WiFi.subnetMask();

  IPAddress bcast;

  for (int i = 0; i < 4; i++)
  {
    bcast[i] =
      (ip[i] & mask[i]) |
      (~mask[i] & 0xFF);
  }

  return bcast;
}

// =====================================================
// WIFI CONNECTION
// =====================================================

void connectWiFi()
{
  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  Serial.print("Connecting to WiFi");

  unsigned long start = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 15000
  )
  {
    delay(250);

    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    broadcastIP =
      computeBroadcastIP();

    Serial.println();

    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Broadcast target: ");
    Serial.println(broadcastIP);

    udp.begin(V2V_PORT);
  }
  else
  {
    Serial.println();

    Serial.println(
      "WiFi connect failed — will retry in background."
    );
  }
}

// =====================================================
// SEND V2V PACKET
// =====================================================

void sendData()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  myPacket.id = VEHICLE_ID;

  myPacket.movement = movement;

  myPacket.time = millis();

  myPacket.ax = curRawAx;
  myPacket.ay = curRawAy;
  myPacket.az = curRawAz;

  udp.beginPacket(
    broadcastIP,
    V2V_PORT
  );

  udp.write(
    (uint8_t *)&myPacket,
    sizeof(myPacket)
  );

  udp.endPacket();
}

// =====================================================
// RECEIVE V2V PACKET
// =====================================================

void receiveData()
{
  int packetSize = udp.parsePacket();

  if (packetSize != sizeof(V2VPacket))
  {
    if (packetSize > 0)
      udp.flush();

    return;
  }

  uint8_t buf[sizeof(V2VPacket)];

  udp.read(
    buf,
    sizeof(buf)
  );

  memcpy(
    &otherPacket,
    buf,
    sizeof(otherPacket)
  );

  // -------------------------------------------------
  // IGNORE OUR OWN PACKET
  // -------------------------------------------------

  if (otherPacket.id == VEHICLE_ID)
  {
    return;
  }

  // -------------------------------------------------
  // OTHER ROVER DETECTED
  // -------------------------------------------------

  vehicleDetected = true;

  approachingVehicleID =
    otherPacket.id;

  lastVehicleSeen =
    millis();

  // -------------------------------------------------
  // DISPLAY VEHICLE ID
  // -------------------------------------------------

  if (
    millis() - lastApproachMessage >=
    APPROACH_MESSAGE_INTERVAL
  )
  {
    lastApproachMessage = millis();

    Serial.println();
    Serial.println(
      "================================"
    );

    Serial.print(
      "!!! VEHICLE APPROACHING !!!"
    );

    Serial.println();

    Serial.print(
      "ROVER ID: "
    );

    Serial.println(
      approachingVehicleID
    );

    Serial.println(
      "================================"
    );
  }
}

// =====================================================
// CHECK WHETHER OTHER ROVER IS STILL PRESENT
// =====================================================

void checkVehicleTimeout()
{
  if (
    vehicleDetected &&
    millis() - lastVehicleSeen >
    VEHICLE_TIMEOUT
  )
  {
    vehicleDetected = false;

    Serial.println(
      "Other vehicle no longer detected."
    );
  }
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  // =================================================
  // MOTOR PINS
  // =================================================

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopVehicle();

  // =================================================
  // BLUETOOTH
  // =================================================

  BT.begin("ROVER_1");

  // =================================================
  // MPU6050
  // =================================================

  Wire.begin(
    MPU_SDA,
    MPU_SCL
  );

  delay(100);

  if (initializeMPU())
  {
    mpuConnected = true;

    calibrateMPU();
  }
  else
  {
    mpuConnected = false;

    Serial.println(
      "MPU6050 NOT CONNECTED"
    );
  }

  // =================================================
  // TIMERS
  // =================================================

  lastConnectionCheck = millis();

  lastDisplay = millis();

  lastSpeedUpdate = millis();

  lastMPURead = millis();

  lastApproachMessage = millis();

  // =================================================
  // WIFI + UDP
  // =================================================

  connectWiFi();

  Serial.println();
  Serial.println(
    "================================"
  );

  Serial.println(
    "ROVER 1 READY"
  );

  Serial.println(
    "Vehicle ID: 1"
  );

  Serial.println(
    "Bluetooth: ROVER_1"
  );

  Serial.println(
    "V2V UDP: ACTIVE"
  );

  Serial.println(
    "================================"
  );
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop()
{
  // =================================================
  // BLUETOOTH
  // =================================================

  bluetoothControl();

  // =================================================
  // MPU CONNECTION CHECK
  // =================================================

  if (
    millis() - lastConnectionCheck >=
    CONNECTION_CHECK_INTERVAL
  )
  {
    lastConnectionCheck = millis();

    bool currentStatus =
      checkMPU6050();

    if (
      mpuConnected &&
      !currentStatus
    )
    {
      mpuConnected = false;

      Serial.println(
        "MPU6050 disconnected!"
      );
    }
    else if (
      !mpuConnected &&
      currentStatus
    )
    {
      if (initializeMPU())
      {
        mpuConnected = true;

        calibrateMPU();

        speedMS = 0.0;

        speedKMPH = 0.0;

        lastSpeedUpdate =
          millis();

        Serial.println(
          "MPU6050 reconnected."
        );
      }
    }

    // =================================================
    // WIFI RECONNECT
    // =================================================

    if (
      WiFi.status() != WL_CONNECTED
    )
    {
      connectWiFi();
    }
  }

  // =================================================
  // MPU SAMPLING
  // =================================================

  if (
    mpuConnected &&
    millis() - lastMPURead >=
    MPU_READ_INTERVAL
  )
  {
    lastMPURead = millis();

    updateMPU();
  }

  // =================================================
  // RECEIVE V2V
  // =================================================

  receiveData();

  // =================================================
  // CHECK OTHER VEHICLE
  // =================================================

  checkVehicleTimeout();

  // =================================================
  // SEND V2V EVERY 200 ms
  // =================================================

  if (
    millis() - lastSend >= 200
  )
  {
    lastSend = millis();

    sendData();
  }

  // =================================================
  // SMALL DELAY
  // =================================================

  delay(1);
}