/**
 *******************************************************************************
 * @file    main.c
 * @author  Monthe Youmbi Yvan Fabrice / Gemini
 * @brief   Station DEEP Finale Corrigée - DHT11 + APDS9960 + BMP180 + Buzzer PA11
 *******************************************************************************
 */

#include "config.h"
#include "stm32g4_sys.h"
#include "stm32g4_uart.h"
#include "stm32g4_gpio.h"
#include "DHT11/stm32g4_dht11.h"
#include "TFT_ili9341/stm32g4_ili9341.h"
#include "TFT_ili9341/stm32g4_fonts.h"
#include "APDS9960/stm32g4_apds9960.h"
#include "BMP180/stm32g4_bmp180.h"
#include <stdio.h>
#include <stdbool.h>

/* --- Paramètres Système --- */
#define DELAI_OBSCURITE_MS  10000

/* --- Broches d'Alerte Humidité --- */
#define LED_HUM_MAX_GPIO    GPIOB
#define LED_HUM_MAX_PIN     GPIO_PIN_4
#define LED_HUM_MIN_GPIO    GPIOB
#define LED_HUM_MIN_PIN     GPIO_PIN_5

/* --- Variables Globales --- */
int seuil_max = 70; // Seuil max humidité par défaut
int seuil_min = 30; // Seuil min humidité par défaut
bool mode_config = false;
bool regle_max = true;
uint32_t debut_obscurite = 0;
bool est_en_veille = false;

BMP180_t BMP180_Data;
typedef enum { ACCUEIL, STATION_ACTIVE } state_t;
state_t etat = ACCUEIL;

/* --- Variables Bouton PA0 --- */
uint32_t btn_timer_start = 0;
uint32_t last_release_time = 0;
bool bouton_deja_traite = false;

/* --- Variables de cadencement non-bloquant pour l'alerte Buzzer --- */
uint32_t dernier_bip_alerte = 0;
bool buzzer_actif_alerte = false;

/* --- Prototypes --- */
static void init_system(void);
static void page_bienvenue(void);
static void station_interface(void);
static void gestion_bouton_interactif(void);
static void mettre_a_jour_mesures(uint16_t lum, uint8_t prox);
static void refresh_config_display(void);
static void declencher_bip_evenement(uint16_t duree_ms);

/* ---------------------------------------------------------------------------*/

int main(void) {
    uint16_t luminosite = 0;
    uint8_t proximite = 0;

    init_system();
    page_bienvenue();

    // Initialisation du timer de veille
    debut_obscurite = HAL_GetTick();

    while (1) {
        // 1. LECTURE CAPTEURS
        APDS9960_readAmbientLight(&luminosite);
        APDS9960_readProximity(&proximite);

        // 2. GESTION DU BOUTON
        gestion_bouton_interactif();

        // 3. LOGIQUE DE VEILLE & RÉVEIL AUTOMATIQUE (Basée sur la luminosité)
        if (!mode_config && etat == STATION_ACTIVE) {
            if (!est_en_veille) {
                if (luminosite > 0) {
                    debut_obscurite = HAL_GetTick(); // Reset tant qu'il y a de la lumière
                } else if (HAL_GetTick() - debut_obscurite > DELAI_OBSCURITE_MS) {
                    ILI9341_DisplayOff();
                    est_en_veille = true;
                }
            }
            else {
                // Réveil immédiat si retour de la lumière
                if (luminosite > 0) {
                    ILI9341_DisplayOn();
                    est_en_veille = false;
                    debut_obscurite = HAL_GetTick();
                    declencher_bip_evenement(80); // Petit bip au réveil automatique
                }
            }
        }

        // 4. MISE À JOUR AFFICHAGE
        if (etat == STATION_ACTIVE && !est_en_veille && !mode_config) {
            mettre_a_jour_mesures(luminosite, proximite);
        }

        HAL_Delay(20);
    }
}

/**
 * @brief Produit un signal sonore franc et synchrone (bloquant mais très court).
 */
static void declencher_bip_evenement(uint16_t duree_ms) {
    HAL_GPIO_WritePin(BUZZER_GPIO, BUZZER_PIN, 1);
    HAL_Delay(duree_ms);
    HAL_GPIO_WritePin(BUZZER_GPIO, BUZZER_PIN, 0);
}

static void mettre_a_jour_mesures(uint16_t lum, uint8_t prox) {
    uint8_t hi, hd, ti, td;
    char buf[32];

    // --- LECTURE BMP180 ---
    BSP_BMP180_StartTemperature(&BMP180_Data);
    HAL_Delay(BMP180_Data.Delay/1000 + 1);
    BSP_BMP180_ReadTemperature(&BMP180_Data);

    BSP_BMP180_StartPressure(&BMP180_Data, BMP180_Oversampling_UltraHighResolution);
    HAL_Delay(BMP180_Data.Delay/1000 + 1);
    BMP180_ReadPressure(&BMP180_Data);

    // --- LECTURE DHT11 ---
    if (BSP_DHT11_state_machine_get_datas(&hi, &hd, &ti, &td) == END_OK) {

        // --- ALERTES HUMIDITÉ (PB4/PB5) + BUZZER CADENCÉ ---
        if (hi >= seuil_max || hi <= seuil_min) {
            if (hi >= seuil_max) {
                HAL_GPIO_WritePin(LED_HUM_MAX_GPIO, LED_HUM_MAX_PIN, 1); // Allume PB4
                HAL_GPIO_WritePin(LED_HUM_MIN_GPIO, LED_HUM_MIN_PIN, 0); // Éteint PB5
            } else {
                HAL_GPIO_WritePin(LED_HUM_MIN_GPIO, LED_HUM_MIN_PIN, 1); // Allume PB5
                HAL_GPIO_WritePin(LED_HUM_MAX_GPIO, LED_HUM_MAX_PIN, 0); // Éteint PB4
            }

            // Gestion du bip d'alerte intermittent (Toutes les 500 ms) sans bloquer le système
            if (HAL_GetTick() - dernier_bip_alerte > 500) {
                dernier_bip_alerte = HAL_GetTick();
                buzzer_actif_alerte = !buzzer_actif_alerte;
                HAL_GPIO_WritePin(BUZZER_GPIO, BUZZER_PIN, buzzer_actif_alerte ? 1 : 0);
            }
        } else {
            // Situation normale : Tout éteint
            HAL_GPIO_WritePin(LED_HUM_MAX_GPIO, LED_HUM_MAX_PIN, 0);
            HAL_GPIO_WritePin(LED_HUM_MIN_GPIO, LED_HUM_MIN_PIN, 0);
            HAL_GPIO_WritePin(BUZZER_GPIO, BUZZER_PIN, 0);
            buzzer_actif_alerte = false;
        }

        // Affichage LCD : Température & Humidité
        snprintf(buf, sizeof(buf), "%d.%d C ", ti, td);
        ILI9341_Puts(20, 82, buf, &Font_16x26, ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK);
        snprintf(buf, sizeof(buf), "%d.%d %% ", hi, hd);
        ILI9341_Puts(20, 122, buf, &Font_16x26, ILI9341_COLOR_CYAN, ILI9341_COLOR_BLACK);
    }

    // Affichage LCD : Pression (BMP180)
    snprintf(buf, sizeof(buf), "Pres: %ld Pa", BMP180_Data.Pressure);
    ILI9341_Puts(20, 160, buf, &Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    // Affichage LCD : Altitude & Lux
    snprintf(buf, sizeof(buf), "Alt: %.1f m  Lux: %d", BMP180_Data.Altitude, lum);
    ILI9341_Puts(20, 190, buf, &Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    // Affichage Limites Humidité
    snprintf(buf, sizeof(buf), "Limites H: %d%% - %d%%", seuil_min, seuil_max);
    ILI9341_Puts(20, 215, buf, &Font_7x10, ILI9341_COLOR_RED, ILI9341_COLOR_BLACK);
}

static void gestion_bouton_interactif(void) {
    bool btn_presse = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET);

    if (btn_presse) {
        if (btn_timer_start == 0) btn_timer_start = HAL_GetTick();

        // Détection de l'appui long (2 secondes) -> COMMUTATION MODE CONFIG
        if (!bouton_deja_traite && (HAL_GetTick() - btn_timer_start > 2000)) {
            bouton_deja_traite = true;
            if (etat == STATION_ACTIVE) {
                mode_config = !mode_config;

                declencher_bip_evenement(120); // Bip de changement de mode (plus long)

                if (mode_config) {
                    ILI9341_Fill(ILI9341_COLOR_BLUE);
                    refresh_config_display();
                } else {
                    ILI9341_Fill(ILI9341_COLOR_BLACK);
                    station_interface();
                }
            }
        }
    } else {
        // Bouton relâché
        if (btn_timer_start != 0) {
            uint32_t duree = HAL_GetTick() - btn_timer_start;

            if (!bouton_deja_traite && duree > 50) { // Appui court valide
                if (mode_config) {
                    declencher_bip_evenement(60); // Bip court pour chaque incrémentation

                    if (HAL_GetTick() - last_release_time < 400) {
                        regle_max = !regle_max; // Double clic détecté
                    } else {
                        if (regle_max) { seuil_max++; if(seuil_max > 95) seuil_max = 60; }
                        else { seuil_min++; if(seuil_min > 50) seuil_min = 10; }
                    }
                    refresh_config_display();
                    last_release_time = HAL_GetTick();
                }
                else if (etat == ACCUEIL) {
                    // 1. ALLUMAGE IMMÉDIAT ET CONJOINT DES LEDS ET DU BUZZER
                    HAL_GPIO_WritePin(BUZZER_GPIO, BUZZER_PIN, 1);
                    HAL_GPIO_WritePin(LED_GREEN_GPIO, LED_GREEN_PIN, 1);
                    HAL_GPIO_WritePin(LED_RED_GPIO, LED_RED_PIN, 1);
                    HAL_GPIO_WritePin(LED_BLUE_GPIO, LED_BLUE_PIN, 1);
                    HAL_GPIO_WritePin(LED_HUM_MAX_GPIO, LED_HUM_MAX_PIN, 1);
                    HAL_GPIO_WritePin(LED_HUM_MIN_GPIO, LED_HUM_MIN_PIN, 1);

                    // Le buzzer fait sa petite sonnerie initiale de 150ms puis se coupe
                    HAL_Delay(150);
                    HAL_GPIO_WritePin(BUZZER_GPIO, BUZZER_PIN, 0);

                    // Les LEDS poursuivent leur allumage pour atteindre un total de 1 seconde (150ms + 850ms)
                    HAL_Delay(850);

                    // Extinction complète de la séquence de test
                    HAL_GPIO_WritePin(LED_GREEN_GPIO, LED_GREEN_PIN, 0);
                    HAL_GPIO_WritePin(LED_RED_GPIO, LED_RED_PIN, 0);
                    HAL_GPIO_WritePin(LED_BLUE_GPIO, LED_BLUE_PIN, 0);
                    HAL_GPIO_WritePin(LED_HUM_MAX_GPIO, LED_HUM_MAX_PIN, 0);
                    HAL_GPIO_WritePin(LED_HUM_MIN_GPIO, LED_HUM_MIN_PIN, 0);

                    // 2. PASSAGE À L'ÉTAT STATION ACTIVE
                    ILI9341_Fill(ILI9341_COLOR_BLACK);
                    station_interface();
                    BSP_DHT11_init(GPIOA, GPIO_PIN_1);
                    etat = STATION_ACTIVE;
                    debut_obscurite = HAL_GetTick();

                    declencher_bip_evenement(80); // Bip de confirmation d'entrée dans la station
                }
            }
            btn_timer_start = 0;
            bouton_deja_traite = false;
        }
    }
}

static void refresh_config_display(void) {
    char buf[32];
    ILI9341_Puts(20, 20, "REGLAGE SEUILS HUM.", &Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLUE);
    ILI9341_Puts(20, 80, regle_max ? "> MAX" : "  MAX", &Font_11x18, regle_max ? ILI9341_COLOR_YELLOW : ILI9341_COLOR_WHITE, ILI9341_COLOR_BLUE);
    snprintf(buf, sizeof(buf), "  %d %%", seuil_max);
    ILI9341_Puts(20, 105, buf, &Font_16x26, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLUE);
    ILI9341_Puts(20, 160, !regle_max ? "> MIN" : "  MIN", &Font_11x18, !regle_max ? ILI9341_COLOR_YELLOW : ILI9341_COLOR_WHITE, ILI9341_COLOR_BLUE);
    snprintf(buf, sizeof(buf), "  %d %%", seuil_min);
    ILI9341_Puts(20, 185, buf, &Font_16x26, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLUE);
}

static void init_system(void) {
    HAL_Init();
    BSP_GPIO_enable();
    BSP_UART_init(UART2_ID, 115200);

    // Initialisation des capteurs
    APDS9960_init();
    APDS9960_enableLightSensor(false);
    APDS9960_enableProximitySensor(false);

    if (BSP_BMP180_Init(&BMP180_Data, NULL, 0) != BMP180_Result_Ok) {
        printf("Erreur BMP180\n");
    }

    // Config des Pins de la Nucleo G431 (Les macros BUZZER_ et LED_ proviennent directement de config.h)
    BSP_GPIO_pin_config(GPIOA, GPIO_PIN_0, GPIO_MODE_INPUT, GPIO_PULLUP, GPIO_SPEED_FREQ_LOW, GPIO_NO_AF);
    BSP_GPIO_pin_config(LED_GREEN_GPIO, LED_GREEN_PIN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_NO_AF);
    BSP_GPIO_pin_config(LED_RED_GPIO, LED_RED_PIN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_NO_AF);
    BSP_GPIO_pin_config(LED_BLUE_GPIO, LED_BLUE_PIN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_NO_AF);

    // Configurations des sorties d'alertes humidité et du buzzer (PA11)
    BSP_GPIO_pin_config(LED_HUM_MAX_GPIO, LED_HUM_MAX_PIN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_NO_AF);
    BSP_GPIO_pin_config(LED_HUM_MIN_GPIO, LED_HUM_MIN_PIN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_NO_AF);
    BSP_GPIO_pin_config(BUZZER_GPIO, BUZZER_PIN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, GPIO_NO_AF);

    ILI9341_Init();
    ILI9341_Rotate(ILI9341_Orientation_Landscape_2);
}

static void page_bienvenue(void) {
    ILI9341_Fill(ILI9341_COLOR_BLACK);
    ILI9341_Puts(30, 60, "PROJET DEEP", &Font_16x26, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    ILI9341_Puts(30, 120, "Yvan & Bryan", &Font_11x18, ILI9341_COLOR_RED, ILI9341_COLOR_BLACK);
    ILI9341_Puts(30, 180, "Appuyez sur PA0", &Font_7x10, ILI9341_COLOR_YELLOW, ILI9341_COLOR_BLACK);
}

static void station_interface(void) {
    ILI9341_Puts(10, 10, "STATION ACTIVE", &Font_7x10, ILI9341_COLOR_GREEN, ILI9341_COLOR_BLACK);
    ILI9341_Puts(20, 60, "Temp/Hum :", &Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
}
