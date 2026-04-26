# PID Fixed-Point Controller

## 概述

`Code_boweny/Function/PID/` 提供通用位置式 PID 控制器，适合电机速度环、航向环、姿态角速度环等上层控制逻辑复用。

模块特点：

- 不使用浮点运算
- 增益采用 Q10 定点格式
- 每个 `PID_Controller_t` 独立维护状态
- 支持输出限幅
- 支持积分限幅，降低积分饱和风险
- 首次更新时微分项为 0，避免启动瞬间突跳

## 文件结构

```text
Code_boweny/Function/PID/
├── PID.h       # 类型定义、宏、API 声明
├── PID.c       # PID 实现
└── README.md   # 本文档
```

## 增益格式

PID 增益使用 Q10 定点数：

```c
#define PID_GAIN_Q      10
#define PID_GAIN_SCALE  (1L << PID_GAIN_Q)
```

含义：

- `1024` 表示 `1.0`
- `512` 表示 `0.5`
- `2048` 表示 `2.0`

示例：

```c
PID_GAIN_FROM_INT(1)  /* 1.0 */
512                   /* 0.5 */
```

## API

```c
void PID_Init(PID_Controller_t *pid,
              int16 kp, int16 ki, int16 kd,
              int16 output_min, int16 output_max,
              int32 integral_min, int32 integral_max);

void PID_Reset(PID_Controller_t *pid);
void PID_SetTarget(PID_Controller_t *pid, int16 target);
void PID_SetGains(PID_Controller_t *pid, int16 kp, int16 ki, int16 kd);
void PID_SetOutputLimit(PID_Controller_t *pid, int16 output_min, int16 output_max);
void PID_SetIntegralLimit(PID_Controller_t *pid, int32 integral_min, int32 integral_max);
int16 PID_Update(PID_Controller_t *pid, int16 measured);
int16 PID_UpdateTarget(PID_Controller_t *pid, int16 target, int16 measured);
```

## 使用示例

```c
#include "PID.h"

static PID_Controller_t left_speed_pid;

void Control_Init(void)
{
    PID_Init(&left_speed_pid,
             900, 80, 20,          /* kp=0.879, ki=0.078, kd=0.019 */
             -1000, 1000,          /* 输出对应 Motor_SetSpeed 范围 */
             -3000L, 3000L);       /* 积分限幅 */
}

void Control_Update(int16 target_speed, int16 measured_speed)
{
    int16 pwm_cmd;

    pwm_cmd = PID_UpdateTarget(&left_speed_pid, target_speed, measured_speed);
    Motor_SetSpeed(MOTOR_LEFT, pwm_cmd);
}
```

## 公式

```text
error      = target - measured
integral  = clamp(integral + error)
derivative = error - prev_error
output    = (kp * error + ki * integral + kd * derivative) >> 10
output    = clamp(output)
```

## 注意事项

- `PID_Update()` 返回值为 `int16`，通常可直接映射到电机 PWM 命令或舵机控制量。
- `kp/ki/kd` 都是 Q10 定点值，不要传入浮点数。
- 若重新进入控制模式，建议先调用 `PID_Reset()` 清除历史积分和误差。
- 若控制方向相反，应在上层调整误差方向或对输出取反。
