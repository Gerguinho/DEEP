/**
 *******************************************************************************
 * @file    config.h
 * @author  Monthe Youmbi Yvan Fabrice / BRYAN GERGINIO
 * @date    2026
 * @brief   Fichier de configuration de la Station DEEP conforme au main.c
 *******************************************************************************
 */


#ifndef CONFIG_H_
#define CONFIG_H_


#include "stm32g4xx_hal.h"


#define LED_GREEN_PIN       GPIO_PIN_8
#define LED_GREEN_GPIO      GPIOA

#define LED_RED_PIN         GPIO_PIN_13
#define LED_RED_GPIO        GPIOC

#define LED_BLUE_PIN        GPIO_PIN_10
#define LED_BLUE_GPIO       GPIOA


#define BUZZER_PIN          GPIO_PIN_11
#define BUZZER_GPIO         GPIOA


#define UART2_ON_PA3_PA2
#define UART1_ON_PA10_PA9

#define USE_BSP_TIMER       1
#define USE_BSP_EXTIT       1

#define USE_RTC             0

#define USE_ADC             0
    #define USE_IN1         0
    #define USE_IN2         0
    #define USE_IN3         0
    #define USE_IN4         0
    #define USE_IN10        0
    #define USE_IN13        0
    #define USE_IN17        0

#define USE_DAC             0

/*------------------Afficheurs------------------*/
#define USE_ILI9341         1 // ACTIVÉ : Écran TFT requis pour le projet
#if USE_ILI9341
    #define USE_XPT2046         0 // Écran tactile désactivé
    #define USE_FONT7x10        1 // ACTIVÉ : Utilisé dans main.c
    #define USE_FONT11x18       1 // ACTIVÉ : Utilisé dans main.c
    #define USE_FONT16x26       1 // ACTIVÉ : Utilisé dans main.c
#endif

#define USE_MLX90614        0
#define USE_EPAPER          0
#define USE_WS2812          0

/*------------------Capteurs I2C et One-Wire------------------*/
#define USE_MPU6050         0
#define USE_APDS9960        1 // ACTIVÉ : Requis pour la lumière et la proximité
#define USE_BMP180          1 // ACTIVÉ : Requis pour la pression et l'altitude
#define USE_BH1750FVI       0
#define USE_DHT11           1 // ACTIVÉ : Requis pour la température et l'humidité
#define USE_DS18B20         0
#define USE_YX6300          0
#define USE_MATRIX_KEYBOARD 0
#define USE_HCSR04          0
#define USE_GPS             0
#define USE_LD19            0
#define USE_NFC03A1         0
#define USE_VL53L0          0
#define USE_IR_RECEIVER     0
#define USE_IR_EMITTER      0

/*------------------Expanders------------------*/
#define USE_MCP23017        0
#define USE_MCP23S17        0
#define USE_SD_CARD         0

/*------------------Actionneurs------------------*/
#define USE_MOTOR_DC        0
#define USE_STEPPER_MOTOR   0

/*------------------Périphériques (Gestion Auto) ------------------*/

#if USE_MLX90614 || USE_MPU6050 || USE_APDS9960  || USE_BH1750FVI || USE_BMP180 || USE_MCP23017 || USE_VL53L0
    #define USE_I2C             1 // Active automatiquement l'I2C pour l'APDS et le BMP
#else
    #ifndef USE_I2C
        #define USE_I2C             0
    #endif
#endif
#define I2C_TIMEOUT         5 // ms

#if USE_ILI9341 || USE_SD_CARD || USE_MCP23S17 || USE_EPAPER
    #define USE_SPI             1 // Active automatiquement le SPI pour l'écran ILI9341
#else
    #ifndef USE_SPI
        #define USE_SPI             0
    #endif
#endif

#define USE_TESTBOARD               0

#endif /* CONFIG_H_ */
