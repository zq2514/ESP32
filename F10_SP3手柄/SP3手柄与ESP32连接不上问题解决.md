
# 1. 问题描述
想用ESP32-32N连接新买的SP3手柄，看看好不好用，结果按照操作说明完完成后发现SP3手柄依然未连接成功。
操作说明连接：
https://pjfcckenlt.feishu.cn/wiki/Y7UzwWLIHijwRjkOieYczm7JnUb

# 2.	设备型号
ESP32：（我看芯片上刻的是ESP32-32 N4，和淘宝连接上不太一样，查乐鑫官方文档也没找到非常对应的，可能值得就是ESP32-WROOM-32吧，不太确定。）
 ![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/d8c6b528f6ee456a9e25d47ded5989ba.png)

SP3:
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/57be00ac77324a689b0cb2bc7d8abdc5.png)
# 3.	问题排查过程
找客服确认
## 3.1	建议1——确认MAC地址
无误；
## 3.2	建议2——确认开发板型号
无误。
## 3.3	建议3——找软件看手柄的蓝牙地址
客服说暂时找不到了，新电脑上没有软件了。我到了名字，软件叫sixaxiscontroller。
自己查询，软件下载地址：
https://sixaxispairtool.en.lo4d.com/download
 ![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/500a451961714bb7b86cefce4821acbc.png)

下载这个可行；
### 3.3.1	查看地址
无误。
## 3.4	复位手柄再试
指示灯后边还有一个复位孔，可以复位SP3，需要长针。经测试无用。
至此客服依然没有解决方案了。
# 4.	柳暗花明
## 4.1	再看操作文档
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/a57a2015e0f2410ea85036f2ed0964e1.png)

实践图中比操作文档中多了一行：E(14) system_api : Base MAC must be a unicast MAC。意为 基础 MAC 地址必须是单播 MAC 地址。
开始发问：什么是单播MAC地址？
## 4.2	单播MAC地址
详细请参考：https://blog.csdn.net/m0_70572045/article/details/135946266
要点：
 ![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/6288e1792d9e43d19df93cf7e2e340da.png)

# 5.	解决问题
## 5.1	在SixaxisPairTool 软件中修改PS3的MAC地址为单播地址
 ![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/1b11f3bc28254dd381841d38929c8377.png)

修改前：c1:5a:5e:bc:22:08 （组播地址）
修改后：22:33:44:55:66:77 （单播地址）
## 5.2	修改ESP32程序中的MAC地址
重新配对，配对成功！
 ![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/5ec891d8f8344ba7b1b6be09c60c7f4e.png)
调试需要细心。要有目的，要会提问。

