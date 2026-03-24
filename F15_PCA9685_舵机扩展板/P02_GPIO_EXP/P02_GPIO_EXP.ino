/*
 * Original library source: https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library
 * 
 * This is the Arduino code PCA6985 16 channel servo controller
 * to control 32 Servo Motors with ESP32 board without WiFi

  get this code and wiring from for this video:  http://robojax.com/RJT268
 * 
 * This is V4 Video on PCA9685: https://youtu.be/JFdXB8Za5Os
 * 
 * watch the video for details (V1) and demo http://youtu.be/y8X9X10Tn1k

 *  
   
 * Written/updated by Ahmad Shamshiri for Robojax Video channel www.Robojax.com
 * Date: Dec 15, 2019, in Ajax, Ontario, Canada

 * Disclaimer: this code is "AS IS" and for educational purpose only.


or make donation using PayPal http://robojax.com/L/?id=64

 *  * This code is "AS IS" without warranty or liability. Free to be used as long as you keep this note intact.* 
 * This code has been download from Robojax.com
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <Wire.h>

#include <Adafruit_PWMServoDriver.h>

// called this way, it uses the default address 0x40
Adafruit_PWMServoDriver board1 = Adafruit_PWMServoDriver(0x40);
// Adafruit_PWMServoDriver board2 = Adafruit_PWMServoDriver(0x41);

// Depending on your servo make, the pulse width min and max may vary, you 
// want these to be as small/large as possible without hitting the hard stop
// for max range. You'll have to tweak them as necessary to match the servos you
// have!
// Watch video V1 to understand the two lines below: http://youtu.be/y8X9X10Tn1k
#define SERVOMIN  125 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  575 // this is the 'maximum' pulse length count (out of 4096)


int servoNumber = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("32 channel Servo test!");

  board1.begin();
  //board2.begin();  
  board1.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
  //board2.setPWMFreq(60);
  //yield();
}

// the code inside loop() has been updated by Robojax
void loop() {


    for( int angle =0; angle<181; angle +=10){
      board1.setPWM(12, 0, angleToPulse(angle) );
    }
  
// robojax PCA9865 16 channel Servo control
  delay(100);

  board1.setPWM(0, 0, 550 );
  board1.setPWM(1, 0, 0);

  delay(3000);
  board1.setPWM(0, 0, 0 );
  board1.setPWM(1, 0, 0);
  delay(2000);
  board1.setPWM(0, 0, 0 );
  board1.setPWM(1, 0, 550);
  delay(3000);
}

/*
 * angleToPulse(int ang)
 * gets angle in degree and returns the pulse width
 * also prints the value on seial monitor
 * written by Ahmad Shamshiri for Robojax, Robojax.com
 */
int angleToPulse(int ang){
   int pulse = map(ang,0, 180, SERVOMIN,SERVOMAX);// map angle of 0 to 180 to Servo min and Servo max 
   Serial.print("Angle: ");Serial.print(ang);
   Serial.print(" pulse: ");Serial.println(pulse);
   return pulse;
}
