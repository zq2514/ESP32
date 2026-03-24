import re
import pandas as pd
import numpy as np
import os


def parse_data_file(file_path):
    """解析文件，返回字典列表"""
    pattern = re.compile(
        r'(\d+)\s+X:([-+]?\d*\.?\d+)g\s+Y:([-+]?\d*\.?\d+)g\s+Z:([-+]?\d*\.?\d+)g\s+'
        r'X轴角度:([-+]?\d*\.?\d+)°\s+\|\s+Y轴角度:\s*([-+]?\d*\.?\d+)°'
    )
    data = []
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                if not line:
                    continue
                match = pattern.match(line)
                if not match:
                    print(f"警告：第{line_num}行格式无法解析，已跳过：{line}")
                    continue
                timestamp = int(match.group(1))
                x_acc = float(match.group(2))
                y_acc = float(match.group(3))
                z_acc = float(match.group(4))
                x_angle = float(match.group(5))
                y_angle = float(match.group(6))
                data.append({
                    'timestamp': timestamp,
                    'x_acc_g': x_acc,
                    'y_acc_g': y_acc,
                    'z_acc_g': z_acc,
                    'x_angle_deg': x_angle,
                    'y_angle_deg': y_angle
                })
    except Exception as e:
        print(f"读取文件出错：{e}")
        return None
    return data


def rotation_matrix(roll_rad, pitch_rad):
    """计算从物体坐标系到全局坐标系的旋转矩阵（先绕X轴转roll，再绕Y轴转pitch）"""
    cφ = np.cos(roll_rad)
    sφ = np.sin(roll_rad)
    cθ = np.cos(pitch_rad)
    sθ = np.sin(pitch_rad)
    R = np.array([
        [cθ, sθ * sφ, -sθ * cφ],
        [0, cφ, sφ],
        [sθ, -cθ * sφ, cθ * cφ]
    ])
    return R


def compute_trajectory(df):
    """根据DataFrame计算轨迹，返回带有速度、位移的新DataFrame"""
    # 复制一份避免修改原始数据
    result = df.copy()

    # 单位转换
    g = 9.8  # m/s²
    result['x_acc_ms2'] = result['x_acc_g'] * g
    result['y_acc_ms2'] = result['y_acc_g'] * g
    result['z_acc_ms2'] = result['z_acc_g'] * g
    result['x_angle_rad'] = np.radians(result['x_angle_deg'])
    result['y_angle_rad'] = np.radians(result['y_angle_deg'])

    # 初始化列表
    n = len(result)
    vx = np.zeros(n)
    vy = np.zeros(n)
    vz = np.zeros(n)
    sx = np.zeros(n)
    sy = np.zeros(n)
    sz = np.zeros(n)

    # 第一点不计算速度/位移（视为初始状态）
    for i in range(1, n):
        # 时间间隔（毫秒转秒）
        dt = (result.loc[i, 'timestamp'] - result.loc[i - 1, 'timestamp']) / 1000.0

        # 上一时刻和当前时刻的加速度（物体坐标系）
        acc_body_prev = np.array([
            result.loc[i - 1, 'x_acc_ms2'],
            result.loc[i - 1, 'y_acc_ms2'],
            result.loc[i - 1, 'z_acc_ms2']
        ])
        acc_body_curr = np.array([
            result.loc[i, 'x_acc_ms2'],
            result.loc[i, 'y_acc_ms2'],
            result.loc[i, 'z_acc_ms2']
        ])

        # 旋转矩阵（使用当前时刻的角度？还是平均值？这里使用当前时刻的角度，简单处理）
        # 更精确可用平均角度，但这里假设角度变化缓慢
        roll_curr = result.loc[i, 'x_angle_rad']
        pitch_curr = result.loc[i, 'y_angle_rad']
        R_curr = rotation_matrix(roll_curr, pitch_curr)

        roll_prev = result.loc[i - 1, 'x_angle_rad']
        pitch_prev = result.loc[i - 1, 'y_angle_rad']
        R_prev = rotation_matrix(roll_prev, pitch_prev)

        # 转换到全局坐标系
        acc_global_prev = R_prev @ acc_body_prev
        acc_global_curr = R_curr @ acc_body_curr

        # 减去重力，得到运动加速度
        a_motion_prev = acc_global_prev - np.array([0, 0, g])
        a_motion_curr = acc_global_curr - np.array([0, 0, g])

        # 梯形法积分求速度
        vx[i] = vx[i - 1] + (a_motion_prev[0] + a_motion_curr[0]) / 2 * dt
        vy[i] = vy[i - 1] + (a_motion_prev[1] + a_motion_curr[1]) / 2 * dt
        vz[i] = vz[i - 1] + (a_motion_prev[2] + a_motion_curr[2]) / 2 * dt

        # 梯形法积分求位移（使用刚计算的速度）
        sx[i] = sx[i - 1] + (vx[i - 1] + vx[i]) / 2 * dt
        sy[i] = sy[i - 1] + (vy[i - 1] + vy[i]) / 2 * dt
        sz[i] = sz[i - 1] + (vz[i - 1] + vz[i]) / 2 * dt

    # 将结果加入DataFrame
    result['vx_ms'] = vx
    result['vy_ms'] = vy
    result['vz_ms'] = vz
    result['sx_m'] = sx
    result['sy_m'] = sy
    result['sz_m'] = sz

    return result


def save_to_excel(df, output_path):
    """保存DataFrame到Excel，指定列顺序"""
    # 定义最终列顺序
    columns_order = [
        'timestamp',
        'x_acc_g', 'y_acc_g', 'z_acc_g',
        'x_angle_deg', 'y_angle_deg',
        'x_acc_ms2', 'y_acc_ms2', 'z_acc_ms2',
        'x_angle_rad', 'y_angle_rad',
        'vx_ms', 'vy_ms', 'vz_ms',
        'sx_m', 'sy_m', 'sz_m'
    ]
    # 只保留存在的列
    existing_cols = [col for col in columns_order if col in df.columns]
    df_out = df[existing_cols]

    try:
        df_out.to_excel(output_path, index=False, engine='openpyxl')
        print(f"数据已成功保存到：{output_path}")
    except Exception as e:
        print(f"保存Excel文件时出错：{e}")


def main():
    # 输入输出文件
    input_file = "sensor_data.txt"  # 可修改
    output_file = "trajectory_output.xlsx"

    if not os.path.exists(input_file):
        input_file = input("请输入数据文件的完整路径：").strip()
        if not input_file:
            print("未提供文件路径，程序退出。")
            return

    print("正在解析文件...")
    records = parse_data_file(input_file)
    if not records:
        print("解析失败或无有效数据。")
        return

    df = pd.DataFrame(records)
    print(f"成功解析 {len(df)} 条记录。")

    print("正在计算轨迹...")
    df_traj = compute_trajectory(df)

    print("正在保存结果...")
    save_to_excel(df_traj, output_file)

    # 可选：简单打印位移结果
    print("\n最终位移（最后一行）：")
    final = df_traj.iloc[-1]
    print(f"X方向位移: {final['sx_m']:.3f} m")
    print(f"Y方向位移: {final['sy_m']:.3f} m")
    print(f"Z方向位移: {final['sz_m']:.3f} m")


if __name__ == "__main__":
    main()
