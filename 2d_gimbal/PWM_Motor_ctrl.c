//
// Created by CHIPS KILLER on 2025/12/13.
//

#include "PWM_Motor_ctrl.h"
#include "mpu6050.h"
#include <math.h>
#include "mahony_fliter.h"


extern struct MAHONY_FILTER_t mahony_filter_1;;
int pwm_offset_yaw;
int pwm_offset_roll;
extern int Pulse_1;//上一次比较值
extern int Pulse_2;
/*
 *x轴朝下，y轴朝右，z轴朝前
 *需解算绕z轴旋转（roll），绕x轴旋转（yaw）
 */
void  Motor_ctrl(MPU6050_t *MPU,double dt,float K/*解算系数*/,float N/*马达系数*/)
{
    /*
     *一阶互补(Fail)
    //First_order_Complementary_Filter
    double angle_gyro_roll=MPU->Gz*dt+angle.roll;
    double angle_acc_roll=atan(MPU->Ay/MPU->Ax)*57.5958;
    angle.roll=K*angle_gyro_roll+(1-K)*angle_acc_roll;//单位为°
0
    double angle_gyro_yaw=MPU->Gx*dt+angle.yaw;
    angle.yaw=angle_gyro_yaw;
    */


    //Motor_handle
    // 先把角度转成 PWM 偏移
    pwm_offset_roll = (int)(N *2.5* mahony_filter_1.roll);
    pwm_offset_yaw  = (int)(N *1.5*mahony_filter_1.yaw);


    // 设定绝对 PWM（需加中位值）
    int center = 75; // 舵机中位
    __HAL_TIM_SET_COMPARE(&htim3 , TIM_CHANNEL_1, center - pwm_offset_yaw);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, center + pwm_offset_roll);

}
