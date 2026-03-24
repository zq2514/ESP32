
// 这段代码可以观察MPU6050
// 输出参数需要处理才能与实际匹配；
// 在esp32上进行测试；


#include <Wire.h>
#include <MPU6050.h>
#include <ESP32Time.h>

MPU6050 mpu;
ESP32Time rtc;

long   accelOffsetX,  accelOffsetY,   accelOffsetZ;


void setup() {
  Serial.begin(9600);
  Wire.begin();
  Serial.println("Initializing MPU6050...");
  mpu.initialize();

  // 检查连接
  if (mpu.testConnection()) {
    Serial.println("MPU6050 connection successful!");
    checkAccelRange();    // 读取MPU6050的量程
    performCalibration();  // 执行校准
  } else {
    Serial.println("MPU6050 connection failed!");
  }

  //rtc.setTime(0, 0, 0, 1, 1, 2026);

}

void loop() {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;

  // 读取加速度和角速度数据
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  // 理想情况下，ax = 0, ay = 0, az = +1g  // Z轴指向重力方向 +1g -> 16384  // 经过实际验证，Z轴指向重力反方向；

  /*
  Serial.print("a/g:\t");
  Serial.print(ax,4); Serial.print("\t");
  Serial.print(ay,4); Serial.print("\t");
  Serial.print(az,4); Serial.print("\t");
  Serial.print(gx,4); Serial.print("\t");
  Serial.print(gy,4); Serial.print("\t");
  Serial.println(gz,4);
  */
  
  readCalibratedAccel();    // 读取实际姿态；
  delay(500);
}



// 读取MPU6050的量程
void checkAccelRange() {
  uint8_t range = mpu.getFullScaleAccelRange();
  
  switch(range) {
    case MPU6050_ACCEL_FS_2:
      Serial.println("量程: ±2g, 使用16384"); break;   // 16384 = 65536/4 
    case MPU6050_ACCEL_FS_4:
      Serial.println("量程: ±4g, 使用8192"); break;    // 8192 = 65536/ 8
    case MPU6050_ACCEL_FS_8:
      Serial.println("量程: ±8g, 使用4096"); break;    // 4096 = 65536/ 16
    case MPU6050_ACCEL_FS_16:
      Serial.println("量程: ±16g, 使用2048"); break;   // 2048 = 65536 / 32
  }
}

// 执行校准
// 1. 将传感器水平静止放置
// 2. 读取多次数据求平均
void performCalibration() {
  Serial.println("开始加速度校准...");
  Serial.println("请将传感器水平静止放置");
  delay(3000);
  
  long sumX = 0, sumY = 0, sumZ = 0;
  const int samples = 1000;
  
  for(int i = 0; i < samples; i++) {
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);
    sumX += ax;
    sumY += ay;
    sumZ += az;
    
    if(i % 100 == 0) Serial.print(".");
    delay(10);
  }
  
  // 计算偏移量（期望：X=0, Y=0, Z=+16384）
  // 这里偏移量是计算了1000个偏移量的平均值；

  accelOffsetX = sumX / samples;
  accelOffsetY = sumY / samples;
  accelOffsetZ = (sumZ / samples) - 16384; // 减去期望的1g
  
  Serial.println("\n校准完成！");
  Serial.print("偏移量 X:"); Serial.print(accelOffsetX);
  Serial.print(" Y:"); Serial.print(accelOffsetY);
  Serial.print(" Z:"); Serial.println(accelOffsetZ);
}


// 应用校准读取数据，上面的函数已经计算好了平均偏移量；
void readCalibratedAccel() {
  int16_t ax, ay, az;
  unsigned long epoch = rtc.getEpoch(); // 获取时间戳
  //unsigned long timestamp = millis();   // 获取当前时间毫秒；
  
  
  mpu.getAcceleration(&ax, &ay, &az);
  
  // 应用校准并转换为g值
  float accelX = (ax - accelOffsetX) / 16384.0;
  float accelY = (ay - accelOffsetY) / 16384.0;
  float accelZ = -(az - accelOffsetZ) / 16384.0;

  // 计算倾斜角度（基于加速度计）
  // atan2 反正切函数 atan2(第一个参数是y坐标，第二个参数是x坐标)
  // 取值范围都是（-pi/2, pi/2）
  /*
  atan2()
  输入: 1,1     输出: 45° (第一象限)
  输入: -1,1    输出: 135° (第二象限)  
  输入: -1,-1   输出: -135° (第三象限)
  输入: 1,-1    输出: -45° (第四象限)
  */



  // 通过查询deepseek，发现这个公式不统一，和安装方向有关，使用这个公式时候，如MPU6050上坐标轴显示，俯视MPU6050，x轴朝正前方，y轴朝左侧，z轴朝上
  float angleX = atan2(accelY, sqrt(accelX * accelX + accelZ * accelZ)) * 180 / PI;   //Roll 横滚
  float angleY = atan2(-accelX, sqrt(accelY * accelY + accelZ * accelZ)) * 180 / PI;  //Pitch  俯仰
  
  Serial.print(epoch); Serial.print(" ");
  Serial.print("X:"); Serial.print(accelX, 4);
  Serial.print("g Y:"); Serial.print(accelY, 4); 
  Serial.print("g Z:"); Serial.print(accelZ, 4);
  Serial.print("g X轴角度:");  Serial.print(angleX, 1);
  Serial.print("° | Y轴角度: ");  Serial.print(angleY, 1);
  Serial.println("°");
}
