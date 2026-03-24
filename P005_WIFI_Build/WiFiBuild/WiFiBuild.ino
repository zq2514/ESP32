#include<WiFi.h>

//设置名称和密码
const char * ssid = "ESP23_AP";
const char * password = "123456qq";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  // 创建热点
  WiFi.softAP(ssid, password);

  //打印热点
  Serial.print("WiFi 接入的IP：");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  // put your main code here, to run repeatedly:

}
