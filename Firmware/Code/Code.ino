`#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;


#define M1_PIN 0
#define M2_PIN 2
#define M3_PIN 3
#define M4_PIN 8


#define CH1 0
#define CH2 1
#define CH3 2
#define CH4 3

#define PWM_FREQ 25000
#define PWM_RES 8   


#define MOTOR_MIN 25     
#define MOTOR_MAX 240
#define MOTOR_ARM 15


float kp = 0.18;
float ki = 0.00;
float kd = 0.003;


typedef struct {
  int throttle;
  int roll;
  int pitch;
  int yaw;
  bool armed;
} ControlData;

ControlData rx;


float rollPID, pitchPID, yawPID;
float rollError, pitchError, yawError;
float lastRollErr, lastPitchErr, lastYawErr;


float gx, gy, gz;
unsigned long lastTime;

void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  memcpy(&rx, data, sizeof(rx));
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  esp_now_init();
  esp_now_register_recv_cb(onReceive);

  
  Wire.begin();
  mpu.initialize();
  mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_1000);

  
  ledcSetup(CH1, PWM_FREQ, PWM_RES);
  ledcSetup(CH2, PWM_FREQ, PWM_RES);
  ledcSetup(CH3, PWM_FREQ, PWM_RES);
  ledcSetup(CH4, PWM_FREQ, PWM_RES);

  ledcAttachPin(M1_PIN, CH1);
  ledcAttachPin(M2_PIN, CH2);
  ledcAttachPin(M3_PIN, CH3);
  ledcAttachPin(M4_PIN, CH4);

  lastTime = micros();
}

void loop() {
  
  if (!rx.armed) {
    stopAllMotors();
    return;
  }


  float dt = (micros() - lastTime) / 1000000.0;
  lastTime = micros();

  
  mpu.getRotation(&gx, &gy, &gz);
  rollRate  = gx / 131.0;
  pitchRate = gy / 131.0;
  yawRate   = gz / 131.0;

  
  rollError  = rx.roll  - rollRate;
  pitchError = rx.pitch - pitchRate;
  yawError   = rx.yaw   - yawRate;

  rollPID  = kp * rollError  + kd * ((rollError  - lastRollErr)  / dt);
  pitchPID = kp * pitchError + kd * ((pitchError - lastPitchErr) / dt);
  yawPID   = kp * yawError   + kd * ((yawError   - lastYawErr)   / dt);

  lastRollErr  = rollError;
  lastPitchErr = pitchError;
  lastYawErr   = yawError;

  int base = map(rx.throttle, 0, 255, MOTOR_MIN, MOTOR_MAX);

  // X quad motor mixing
  int m1 = base + pitchPID + rollPID - yawPID;
  int m2 = base + pitchPID - rollPID + yawPID;
  int m3 = base - pitchPID - rollPID - yawPID;
  int m4 = base - pitchPID + rollPID + yawPID;

  writeMotor(CH1, m1);
  writeMotor(CH2, m2);
  writeMotor(CH3, m3);
  writeMotor(CH4, m4);
}

void writeMotor(int ch, int val) {
  val = constrain(val, MOTOR_ARM, MOTOR_MAX);
  ledcWrite(ch, val);
}

void stopAllMotors() {
  ledcWrite(CH1, 0);
  ledcWrite(CH2, 0);
  ledcWrite(CH3, 0);
  ledcWrite(CH4, 0);
}
