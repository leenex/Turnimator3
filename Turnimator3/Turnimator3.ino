#include <PS2X_lib.h>  //for MOEBIUS
#include "FaBoPWM_PCA9685.h"
// Hacked beyond recognition by Jan Atle Ramsli, Oslo, 2020
//#include "servo.hpp"

FaBoPWM faboPWM;
int pos = 0;

//PS2手柄引脚；PS2 handle Pin
#define PS2_DAT        13
#define PS2_CMD        11
#define PS2_SEL        10
#define PS2_CLK        12

//MOTOR CONTROL Pin
#define DIRA1 0
#define DIRA2 1
#define DIRB1 2
#define DIRB2 3
#define DIRC1 4
#define DIRC2 5
#define DIRD1 6
#define DIRD2 7



#define pressures   true
//#define pressures   false
// #define rumble      true
#define rumble      false

PS2X ps2x; // create PS2 Controller Class

//right now, the library does NOT support hot pluggable controllers, meaning
//you must always either restart your Arduino after you connect the controller,
//or call config_gamepad(pins) again after connecting the controller.

int error = 0;
byte type = 0;
byte vibrate = 0;

void (* resetFunc) (void) = 0;

/*
   Depending on how you wired the motors, you may have to switch the DIRxy constants here, or redefine them
   above.

*/
#define MOTORA_FORWARD(pwm)    faboPWM.set_channel_value(DIRA2,pwm);faboPWM.set_channel_value(DIRA1, 0)
#define MOTORA_STOP(x)         faboPWM.set_channel_value(DIRA2,0);faboPWM.set_channel_value(DIRA1, 0)
#define MOTORA_BACKOFF(pwm)    faboPWM.set_channel_value(DIRA2,0);faboPWM.set_channel_value(DIRA1, pwm)

#define MOTORB_FORWARD(pwm)    faboPWM.set_channel_value(DIRB1,pwm);faboPWM.set_channel_value(DIRB2, 0)
#define MOTORB_STOP(x)         faboPWM.set_channel_value(DIRB1,0);faboPWM.set_channel_value(DIRB2, 0)
#define MOTORB_BACKOFF(pwm)    faboPWM.set_channel_value(DIRB1,0);faboPWM.set_channel_value(DIRB2, pwm)

#define MOTORC_FORWARD(pwm)    faboPWM.set_channel_value(DIRC1,pwm);faboPWM.set_channel_value(DIRC2, 0)
#define MOTORC_STOP(x)         faboPWM.set_channel_value(DIRC1,0);faboPWM.set_channel_value(DIRC2, 0)
#define MOTORC_BACKOFF(pwm)    faboPWM.set_channel_value(DIRC1,0);faboPWM.set_channel_value(DIRC2, pwm)

#define MOTORD_FORWARD(pwm)    faboPWM.set_channel_value(DIRD1,pwm);faboPWM.set_channel_value(DIRD2, 0)
#define MOTORD_STOP(x)         faboPWM.set_channel_value(DIRD1,0);faboPWM.set_channel_value(DIRD2, 0)
#define MOTORD_BACKOFF(pwm)    faboPWM.set_channel_value(DIRD1,0);faboPWM.set_channel_value(DIRD2, pwm)

#define SERIAL  Serial

//#define SERIAL  Serial3

#define LOG_DEBUG

#ifdef LOG_DEBUG
#define M_LOG SERIAL.print
#else
#define M_LOG
#endif

//PWM参数
#define MAX_PWM   2048
#define MIN_PWM   300

// Converts stick values of [-127, ... , 127] to values between MIN_PWM and MAX_PWM
#define STICK_TO_PWM(X) (abs(X)*16)


////////////////////////////////////////
//控制电机运动    宏定义
//    ↑A-----B↑
//     |  ↑  |
//     |  |  |
//    ↑C-----D↑
void go_forward(int speed, int direction)
{
  int pwm_l;
  int pwm_r;
  pwm_l = pwm_r = STICK_TO_PWM(speed);
  if (direction < 0) {
    pwm_l -= STICK_TO_PWM(direction);
  }
  if (direction > 0) {
    pwm_r -= STICK_TO_PWM(direction);
  }
  MOTORA_FORWARD(pwm_l); MOTORB_FORWARD(pwm_r);
  MOTORC_FORWARD(pwm_l); MOTORD_FORWARD(pwm_r);
}

/////////////////////////////////
//    ↓A-----B↓
//     |  |  |
//     |  ↓  |
//    ↓C-----D↓
void go_backward(int speed, int direction)
{
  int pwm_l;
  int pwm_r;
  pwm_l = pwm_r = STICK_TO_PWM(speed);
  if (direction < 0) {
    pwm_l -= STICK_TO_PWM(direction);
  }
  if (direction > 0) {
    pwm_r -= STICK_TO_PWM(direction);
  }

  MOTORA_BACKOFF(pwm_l); MOTORB_BACKOFF(pwm_r);
  MOTORC_BACKOFF(pwm_l); MOTORD_BACKOFF(pwm_r);
}

/////////////////////////////
//    =A-----B↑
//     |   ↖ |
//     | ↖   |
//    ↑C-----D=
void skid_left_forward(int speed, int direction)
{
  int pwm_ll; // lower left (C)
  int pwm_ur; // upper right (B)
  pwm_ll = pwm_ur = STICK_TO_PWM(speed);
  if (direction < 0) {
    pwm_ll -= STICK_TO_PWM(direction);
  }
  if (direction > 0) {
    pwm_ur -= STICK_TO_PWM(direction);
  }

  MOTORA_STOP(0); MOTORB_FORWARD(pwm_ur);
  MOTORC_FORWARD(pwm_ll); MOTORD_STOP(0);
}

/////////////////////////
//    ↓A-----B↑
//     |  ←  |
//     |  ←  |
//    ↑C-----D↓
void skid_left(int speed, int direction)
{
  int pwm_f; // front wheels (A,B)
  int pwm_b; // rear wheels (C,D)
  pwm_f = pwm_b = STICK_TO_PWM(speed);
  if (direction < 0) {
    pwm_f -= STICK_TO_PWM(direction);
  }
  if (direction > 0) {
    pwm_b -= STICK_TO_PWM(direction);
  }

  MOTORA_BACKOFF(pwm_f); MOTORB_FORWARD(pwm_f);
  MOTORC_FORWARD(pwm_b); MOTORD_BACKOFF(pwm_b);
}

//////////////////////////////////////////////////
// BUG: REVIEW
//
//    ↓A-----B=
//     | ↙   |
//     |   ↙ |
//    =C-----D↓
void skid_left_backward(int speed, int direction)
{
  int pwm_lr; // lower right (D)
  int pwm_ul; // upper left (A)
  pwm_lr = pwm_ul = STICK_TO_PWM(speed);
  if (direction < 0) {
    pwm_ul -= STICK_TO_PWM(direction);
  }
  if (direction > 0) {
    pwm_lr -= STICK_TO_PWM(direction);
  }

  MOTORA_BACKOFF(pwm_ul); MOTORB_STOP(0);
  MOTORC_STOP(0); MOTORD_BACKOFF(pwm_lr);
}

//////////////////////////////////////////////
//    ↑A-----B=
//     | ↗   |
//     |   ↗ |
//    =C-----D↑
void skid_right_forward(int speed, int direction)
{
  int pwm_lr; // lower right (D)
  int pwm_ul; // upper left (A)
  pwm_lr = pwm_ul = STICK_TO_PWM(speed);
  if (direction < 0) {
    pwm_ul -= STICK_TO_PWM(direction);
  }
  if (direction > 0) {
    pwm_lr -= STICK_TO_PWM(direction);
  }

  MOTORA_FORWARD(pwm_ul); MOTORB_STOP(0);
  MOTORC_STOP(0); MOTORD_FORWARD(pwm_lr);
}

//    ↑A-----B↓
//     |  →  |
//     |  →  |
//    ↓C-----D↑
void skid_right(int speed, int direction)
{
  int pwm_f; // front wheels
  int pwm_b; // rear wheels
  pwm_f = pwm_b = STICK_TO_PWM(speed);
  if (direction < 0) {
    pwm_f -= STICK_TO_PWM(direction);
  }
  if (direction > 0) {
    pwm_b -= STICK_TO_PWM(direction);
  }

  MOTORA_FORWARD(pwm_f); MOTORB_BACKOFF(pwm_f);
  MOTORC_BACKOFF(pwm_b); MOTORD_FORWARD(pwm_b);
}

////////////////////////////////////
// BUG: REVIEW
//    =A-----B↓
//     |   ↘ |
//     | ↘   |
//    ↓C-----D=
void skid_right_backward(int speed, int direction)
{
  int pwm_ll; // lower left (C)
  int pwm_ur; // upper right (B)
  pwm_ll = pwm_ur = STICK_TO_PWM(speed);
  if (direction < 0) {
    pwm_ll -= STICK_TO_PWM(direction);
  }
  if (direction > 0) {
    pwm_ur -= STICK_TO_PWM(direction);
  }

  MOTORA_STOP(0); MOTORB_BACKOFF(pwm_ur);
  MOTORC_BACKOFF(pwm_ll); MOTORD_STOP(0);
}

//////////////////////////////////////////////
//    ↑A-----B↓
//     | ↗ ↘ |
//     | ↖ ↙ |
//    ↑C-----D↓
void rotate_right(int speed)  //tate_1(uint8_t pwm_A,uint8_t pwm_B,uint8_t pwm_C,uint8_t pwm_D)
{
  int pwm = STICK_TO_PWM(speed);

  MOTORA_FORWARD(pwm); MOTORB_BACKOFF(pwm);
  MOTORC_FORWARD(pwm); MOTORD_BACKOFF(pwm);
}

////////////////////////////////////////////////////
//    ↓A-----B↑
//     | ↙ ↖ |
//     | ↘ ↗ |
//    ↓C-----D↑
void rotate_left(int speed)  // rotate_2(uint8_t pwm_A,uint8_t pwm_B,uint8_t pwm_C,uint8_t pwm_D)
{
  int pwm = STICK_TO_PWM(speed);

  MOTORA_BACKOFF(pwm); MOTORB_FORWARD(pwm);
  MOTORC_BACKOFF(pwm); MOTORD_FORWARD(pwm);
}

//    =A-----B=
//     |  =  |
//     |  =  |
//    =C-----D=
void STOP()
{
  MOTORA_STOP(0); MOTORB_STOP(0);
  MOTORC_STOP(0); MOTORD_STOP(0);
}

void IO_init()
{
  STOP();
}

void setup()
{
  IO_init();
  SERIAL.begin(9600);
  if (faboPWM.begin())
  {
    Serial.println("PCA9685 detected");
    faboPWM.init(300);
  }
  faboPWM.set_hz(50);
  SERIAL.print("Start");

  delay(300) ; //added delay to give wireless ps2 module some time to startup, before configuring it
  //CHANGES for v1.6 HERE!!! **************PAY ATTENTION*************

  //setup pins and settings: GamePad(clock, command, attention, data, Pressures?, Rumble?) check for error
  error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);

  if (error == 0) {
    Serial.print("Found Controller, configured OK\n");
    Serial.print("pressures = ");
    if (pressures)
      Serial.println("true ");
    else
      Serial.println("false");
    Serial.print("rumble = ");
    if (rumble)
      Serial.println("true)");
    else
      Serial.println("false");
    Serial.println("Try out all the buttons, X will vibrate the controller, faster as you press harder;");
    Serial.println("holding L1 or R1 will print out the analog stick values.");
    Serial.println("Note: Go to www.billporter.info for updates and to report bugs.");
  }
  else if (error == 1)
  {
    Serial.println("No controller found\n");
    resetFunc();

  }

  else if (error == 2)
    Serial.println("Controller found but not accepting commands. see readme.txt to enable debug. Visit www.billporter.info for troubleshooting tips");

  else if (error == 3)
    Serial.println("Controller refusing to enter Pressures mode, may not support it. ");

  Serial.print(ps2x.Analog(1), HEX);

  type = ps2x.readType();
  switch (type) {
    case 0:
      Serial.println("Unknown Controller type found ");
      break;
    case 1:
      Serial.println("DualShock Controller found ");
      break;
    case 2:
      Serial.println("GuitarHero Controller found ");
      break;
    case 3:
      Serial.println("Wireless Sony DualShock Controller found ");
      break;
  }
}



void loop()
{

  byte play = 10;
  /* You must Read Gamepad to get new values and set vibration values
    ps2x.read_gamepad(small motor on/off, larger motor strenght from 0-255)
    if you don't enable the rumble, use ps2x.read_gamepad(); with no values
    You should call this at least once a second
  */
  ps2x.read_gamepad(false, vibrate); //read controller and set large motor to spin at 'vibrate' speed


  //start 开始运行，电机初PWM为120；
  if (ps2x.Button(PSB_START))  {
    Serial.println("Start");
  }

  if (ps2x.Button(PSB_PAD_UP)) {     //will be TRUE as long as button is pressed
    Serial.print("Up held this hard: ");
    Serial.println(ps2x.Analog(PSAB_PAD_UP), DEC);
  }
  if (ps2x.Button(PSB_PAD_RIGHT)) {

    rotate_right(127);
  }
  if (ps2x.Button(PSB_PAD_LEFT)) {
    rotate_left(127);
  }
  if (ps2x.Button(PSB_PAD_DOWN)) {
    Serial.print("DOWN held this hard: ");
    Serial.println(ps2x.Analog(PSAB_PAD_DOWN), DEC);
  }
  // Stop
  if (ps2x.Button(PSB_SELECT)) {
    Serial.println("stop");
    STOP();
  }
  // 左平移
  if (ps2x.Button(PSB_PINK)) {
    Serial.println("motor_pmove_left");
  }
  // 右平移
  if (ps2x.Button(PSB_RED)) {
    Serial.println("motor_pmove_right");
  }
  delay(20);


  int stick_l_y = ps2x.Analog(PSS_LY) - 128; // Subtract 128 to produce values -128 to +128
  int stick_l_x = ps2x.Analog(PSS_LX) - 128;
  int stick_r_y = ps2x.Analog(PSS_RY) - 128;
  int stick_r_x = ps2x.Analog(PSS_RX) - 128;

  if (stick_l_y > play && stick_l_x > play) {
    skid_left_forward(stick_l_y, stick_r_x);
    delay(20);
    return;
  }

  if (stick_l_y > play && stick_l_x < 0-play) {
    skid_right_forward(stick_l_y, stick_r_x);
    delay(20);
    return;
  }

  if (stick_l_y < 0-play && stick_l_x > play) {
    skid_left_backward(stick_l_y, stick_r_x);
    delay(20);
    return;
  }

  if (stick_l_y < 0-play && stick_l_x < 0-play) {
    skid_right_backward(stick_l_y, stick_r_x);
    delay(20);
    return;
  }

  if (stick_l_y > 0) //前进
  {
    go_forward(stick_l_y, stick_r_x);
    delay(20);
  }
  //后退
  if (stick_l_y < 0)
  {
    go_backward(stick_l_y, stick_r_x);
    delay(20);
  }
  //左转
  if (stick_l_x > 0)
  {
    skid_left(stick_l_x, stick_r_x);
    delay(20);
  }
  //    //右转
  if (stick_l_x < 0)
  {
    skid_right(stick_l_x, stick_r_x);
    delay(20);
  }

  if (stick_r_y < 0-play){
    rotate_left(stick_r_y);
    delay(20);
  }

  if (stick_r_y > play){
    rotate_right(stick_r_y);
    delay(20);
  }
  
  //    //如果摇杆居中
  if (stick_l_x == 0 && stick_l_y == 0)
  {

    STOP();
    delay(20);
  }
  

}
