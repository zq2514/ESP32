# 一般情况下，这个占空比在多大？50%是不是太大了，我感觉电机刚才很烫，并且堵转没有转起来

2804 是云台电机，不是高速电机——它设计工作在小电流、低速、大扭矩场景。50% 占空比太大了！

正常参数参考
占空比	效果
5~10%	低速平稳运转（云台跟踪模式）
10~20%	中速，日常测试主力区间
20~30%	高速，已经很快了
> 40%	电流很大，容易烧电机/驱动板
50% 占空比 + 堵转 = 电流全灌进静止的线圈里，几秒钟就能烫手。先停下别试了，让电机冷却。

正确流程
电机冷却后，按这个来：


duty 10
align
start
如果转不起来，2 度 2 度往上加：


stop
duty 12
start
不行就 duty 14 ... 一般 10%~18% 肯定能转起来了。

关键：2804 电机正常运转时几乎不发热。如果电机发热 = 占空比太高或者相位偏移不对导致堵转。先把 offset 校准准，再用低占空比，它就凉快平稳了。


# 封装
目标结构

P01_BLDC_2804_Motor/
├── platformio.ini
├── include/
│   ├── config.h          # 所有引脚、参数、常量定义
│   ├── encoder.h         # AS5600 编码器接口声明
│   └── motor.h           # 电机控制接口声明（换向+速度PI+使能）
└── src/
    ├── main.cpp           # setup() + loop()，只做调度
    ├── encoder.cpp        # 编码器实现（I2C读写、检测、扫描）
    ├── motor.cpp          # 电机实现（6步换向、PI速度环、软启动）
    └── commands.cpp       # 串口命令解析（help/start/stop/sweep/ol...）
各文件职责
文件	内容	~行数
config.h	GPIO 定义、极对数、PWM 参数、AS5600 寄存器、PI 增益	60
encoder.h	encoder_init() / read_angle() / check_magnet() / scan_i2c()	20
encoder.cpp	上面函数的实现，I2C 通信细节，错误计数	200
motor.h	motor_init() / enable() / disable() / set_duty() / set_speed() / run()	25
motor.cpp	换向表、LEDC 初始化、PI 控制器、软启斜坡、sweep 扫描、openloop	400
commands.cpp	串口命令解析、help/status 打印	200
main.cpp	setup() 依次初始化各模块，loop() 调串口处理+电机运行	50
接口设计（不暴露全局变量）

// encoder.h  —— 只通过函数访问
bool    encoder_init();
bool    encoder_is_found();
float   encoder_read_radians();    // NAN on error
bool    encoder_magnet_ok();
void    encoder_scan_i2c();

// motor.h  —— 只通过函数访问
void    motor_init();
void    motor_enable();
void    motor_disable();
void    motor_set_duty(float pct_0_100);
void    motor_set_speed(float rpm);   // +CW, -CCW, 0=quit speed mode
void    motor_set_offset(float deg);
void    motor_set_direction(bool cw);
void    motor_sweep_offset();         // 自动扫描
void    motor_openloop(float duty, uint32_t ms);
void    motor_run();                  // 每个 loop 周期调一次
void    motor_print_status();         // status 输出