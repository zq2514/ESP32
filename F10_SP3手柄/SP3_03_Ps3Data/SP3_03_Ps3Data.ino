/*
 * 测试右边四个按键代码：已验证
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
    if(Ps3.isConnected()){

        if( Ps3.data.button.cross ){
            Serial.println("Pressing the cross button");
        }

        if( Ps3.data.button.square ){
            Serial.println("Pressing the square button");
        }

        if( Ps3.data.button.triangle ){
            Serial.println("Pressing the triangle button");
        }

        if( Ps3.data.button.circle ){
            Serial.println("Pressing the circle button");
        }

    }
}
