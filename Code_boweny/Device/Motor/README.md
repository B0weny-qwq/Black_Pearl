# Motor PWM 驱动

`Code_boweny/Device/Motor/` 使用 STC32G 的 PWMA CH3/CH4 控制左右两路直流电机。

## 硬件映射

| 电机 | 信号 | PWM 功能 | MCU 引脚 |
|------|------|----------|----------|
| 左电机 | MLA | `PWM3N_2` | `P2.5` |
| 左电机 | MLB | `PWM3P_2` | `P2.4` |
| 右电机 | MRA | `PWM4N_2` | `P2.7` |
| 右电机 | MRB | `PWM4P_2` | `P2.6` |

其中 `MRA/MLA` 为 N 输出，`MRB/MLB` 为 P 输出。正速度使用 P 侧作为主动方向，负速度使用 N 侧作为主动方向；速度为 0 时关闭对应通道输出。

## API

```c
void Motor_Init(void);
void Motor_SetSpeed(Motor_Id_t motor, int16 speed);
void Motor_SetBothSpeed(int16 left_speed, int16 right_speed);
void Motor_Stop(Motor_Id_t motor);
void Motor_StopAll(void);
int16 Motor_GetSpeed(Motor_Id_t motor);
```

速度范围为 `-1000 ~ +1000`，超出范围会自动限幅。

## 使用示例

```c
#include "Motor.h"

Motor_Init();
Motor_SetBothSpeed(500, 500);     /* 双电机正转 50% */
Motor_SetSpeed(MOTOR_LEFT, -300); /* 左电机反转 30% */
Motor_StopAll();
```

## 接入注意事项

- `Motor_Init()` 会将 `PWM3` 切到 `P2.4/P2.5`，将 `PWM4` 切到 `P2.6/P2.7`。
- 本模块使用 `PWMA`，不占用 Timer0/Timer1/Timer2，也不修改 Driver 层源码。
- 若启用 `APP_PWMA_Output` 或其他使用 `PWMA CH3/CH4` 的示例，会与本模块冲突。
- 若实测正反方向与船体定义相反，可在上层对对应电机速度取反。
