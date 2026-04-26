# MOTOR PWM Driver

## 硬件映射

| 电机 | 信号 | PWM 功能 | MCU 引脚 |
|------|------|----------|----------|
| 左电机 | MLA | `PWM3N_2` | `P2.5` |
| 左电机 | MLB | `PWM3P_2` | `P2.4` |
| 右电机 | MRA | `PWM4N_2` | `P2.7` |
| 右电机 | MRB | `PWM4P_2` | `P2.6` |

其中 `MRA/MLA = N`，`MRB/MLB = P`。驱动按同一 PWM 通道的互补输出使用：

- 左电机使用 `PWMA CH3`
- 右电机使用 `PWMA CH4`
- 正速度：P 侧为主动方向
- 负速度：N 侧为主动方向
- 速度为 0：关闭对应通道输出，避免互补输出在 0 占空比下残留有效态

## API

```c
void Motor_Init(void);
void Motor_SetSpeed(Motor_Id_t motor, int16 speed);
void Motor_SetBothSpeed(int16 left_speed, int16 right_speed);
void Motor_Stop(Motor_Id_t motor);
void Motor_StopAll(void);
int16 Motor_GetSpeed(Motor_Id_t motor);
```

速度范围为 `-1000 ~ +1000`。超出范围会自动限幅。

## 使用示例

```c
#include "Motor.h"

Motor_Init();
Motor_SetBothSpeed(500, 500);    /* 双电机正转 50% */
Motor_SetSpeed(MOTOR_LEFT, -300); /* 左电机反转 30% */
Motor_StopAll();
```

## 接入注意

- `Motor_Init()` 会把 `PWM3` 切到 `P2.4/P2.5`，把 `PWM4` 切到 `P2.6/P2.7`。
- 本模块使用 `PWMA`，不占用 Timer0/Timer1/Timer2，也不改 Driver 层源码。
- 若后续启用 `APP_PWMA_Output` 或其它使用 `PWMA CH3/CH4` 的示例，会与本模块冲突。
- 若实测正反方向与船体定义相反，只需要在上层对对应电机速度取反，或调整 `Motor_*SetForwardPolarity()` 的正向极性。
