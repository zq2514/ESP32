// 创建esp32 WiFi的 AP热点模式，相当于ESP32 做路由器；
// 程序可行；

#include <WiFi.h>
const char *ssid = "ESP32_WiFi_ZQ";     // 创建WIFI名称
const char *password = "12345678zq";  // WIFI密码
IPAddress local_IP(192,168,1,10);   // WIFI地址 可以使用Ping功能来测试测试是否联通；类似与PLC组网；
IPAddress gateway(192,168,1,10);    // 
IPAddress subnet(255,255,255,0);    // 子网掩码；
void setup() {
  Serial.begin(115200);
  Serial.println();

  WiFi.mode(WIFI_AP); //AP模式
  WiFi.softAPConfig(local_IP, gateway, subnet); // 设置AP地址
  WiFi.softAP(ssid, password); // 启动AP成功返回1
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP()); // 打印IP地址

  WiFi.softAPsetHostname("CHIPHOME"); // 设置主机名
  Serial.print("Hostname: ");
  Serial.println(WiFi.softAPgetHostname()); // 打印主机名

  Serial.print("MAC Address: ");
  Serial.println(WiFi.softAPmacAddress()); // 打印MAC地址
}

void loop(){
  Serial.print("当前连接客户端数: ");
  Serial.println(WiFi.softAPgetStationNum()); //打印客户端连接数量
  delay(5000);
}
