
#include "stdbool.h"
#include "quantum.h"
#include "config.h"
#include "gpio.h"
#include "analog.h"
#include <print.h>

const pin_t row_pins [] = MATRIX_ROW_PINS;
static uint16_t log_matrix[MATRIX_COLS][MATRIX_ROWS];
static uint16_t ec_noise_treshold[MATRIX_COLS][MATRIX_ROWS];
uint16_t scan_counter = 0;


void adc_int(void){

    palSetLineMode(ANALOG_READINGS_INPUT, PAL_MODE_INPUT_ANALOG);

}

void pins_init(void){

    gpio_write_pin_low(DISCHARGE_PIN);
    gpio_set_pin_output(DISCHARGE_PIN);

    for (uint8_t i = 0; i < MATRIX_ROWS; i++)
    {
        gpio_write_pin_low(row_pins[i]);
        gpio_set_pin_output(row_pins[i]);
        
    }
    
}
 // Вывод сканирования в консоль каждые 500 циклов
 void logger(void){
    if (scan_counter == 500)
    {
        uprintf("\r\n");
        for (uint8_t col = 0; col < MATRIX_COLS; col++)
        {
            for (uint8_t row = 0; row < MATRIX_ROWS; row++)
            {
                uprintf("COL %d ROW %d: %u", col, row, log_matrix[col][row]);
                uprintf("\r\n");
            }
        }
        scan_counter = 0;
    }
    else
    {
        scan_counter++;
    }
 }



 // Сканирование конкретного датчика по адресу в матрице
 uint16_t ec_sw_scan(uint8_t col, uint8_t row){

    uint16_t raw_adc_readings = 0;

    gpio_write_pin_low(row_pins[row]);

    gpio_set_pin_input(DISCHARGE_PIN);

    gpio_write_pin_high(row_pins[row]);

    raw_adc_readings = analogReadPin(ANALOG_READINGS_INPUT);

    gpio_write_pin_low(DISCHARGE_PIN);
    gpio_set_pin_output(DISCHARGE_PIN);

    wait_us(DISCHARGE_TIME_US);

    log_matrix[col][row] = raw_adc_readings; // Логирования сканирований

    return raw_adc_readings;
 }
 // Семплинг и запись порога шума для каждого датчика
 void ec_noise_sample(void){

    for (uint8_t col = 0; col < MATRIX_COLS; col++)
    {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++)
        {
            ec_noise_treshold[col][row] = 0;
        }
    }
   
    for (uint8_t sample = 0; sample < NOISE_THRESHOLD_SAMPLING_COUNT; sample++)
    {
        for (uint8_t col = 0; col < MATRIX_COLS; col++)
        {
            for (uint8_t row = 0; row < MATRIX_ROWS; row++)
            {
                ec_noise_treshold[col][row] += ec_sw_scan(col, row);
            }
        }
    }

    for (uint8_t col = 0; col < MATRIX_COLS; col++)
    {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++)
        {
            ec_noise_treshold[col][row] /= NOISE_THRESHOLD_SAMPLING_COUNT;
            ec_noise_treshold[col][row] += NOISE_OFFSET;
        }
    }
 }

 // Функция сканирования и обновления current matrix
 bool ec_matrix_scan(matrix_row_t current_matrix[]){
    bool has_changed = false;

    for (uint8_t col = 0; col < MATRIX_COLS; col++)
    {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++)
        {
            uint16_t raw_adc_readings = ec_sw_scan(col, row); // Получаем данные сканирования конкретного датчика
            uint8_t key_previous_state = (current_matrix[row] >> col) & 1; // Запрашиваем текущее состояние клавиши (нажата или отпущена)

            if (raw_adc_readings < ec_noise_treshold[col][row])
            {
                continue;
            }
            else if (raw_adc_readings > ACTUATION_LEVEL && key_previous_state == 0)
            {
                current_matrix[row] |= (1 << col); // Нажимаем
                has_changed = true;
            } 
            else if (raw_adc_readings < RELEASE_LEVEL && key_previous_state == 1)
            {
               current_matrix[row] &= ~(1 << col); // Отпускаем
               has_changed = true;
            }
        }
    }
    return has_changed;
 }

 // Инициализация матрицы СТАНДАРТНАЯ
void matrix_init_custom(void) {

    adc_int();
    pins_init();
    ec_noise_sample();

}
 // Сканирование матрицы СТАНДАРТНАЯ
bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    bool matrix_has_changed = ec_matrix_scan(current_matrix);

    logger();

    return matrix_has_changed;
}