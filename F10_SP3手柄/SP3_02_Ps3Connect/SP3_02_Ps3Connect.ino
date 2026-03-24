/*
 * 连接测试代码：已验证
 */
#include <Ps3Controller.h>

void setup()
{
    Serial.begin(115200);
    Ps3.begin("22:33:44:55:66:77");
    Serial.println("Ready.");
    
}

void loop()
{
  if (Ps3.isConnected()){
    Serial.println("Connected!");
  }

  delay(3000);
}
