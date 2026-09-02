// 一主两从通讯方式实现；
// 在原来基础上添加注释，生成了readme.md文件；
// 修改程序逻辑，手动控制LED灯的亮灭；
// 串口通讯控制指令：
 *  【控制命令】（在串口监视器输入，回车发送）
 *    on  <addr>    开灯，如 on 1 / on 0x02
 *    off <addr>    关灯，如 off 1 / off 2
 *    read <addr>   查询 LED 状态
 *    on/off/read all    对所有已配置从站操作
 *    help          显示帮助