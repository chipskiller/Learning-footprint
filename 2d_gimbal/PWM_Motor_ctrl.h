//
// Created by CHIPS KILLER on 2025/12/13.
//

#ifndef MPU6050_TEST1_ANGLE_H
#define MPU6050_TEST1_ANGLE_H

#include "mpu6050.h"
#include "tim.h"
#include "stm32f1xx_hal_tim.h"

#endif //MPU6050_TEST1_ANGLE_H
typedef struct
{
    double yaw;
    double pitch;
    double roll;

}Data_t;

void Motor_ctrl(MPU6050_t *MPU,double dt,float K/*解算系数*/,float N/*马达系数*/);
extern Data_t angle;//上一次的角度