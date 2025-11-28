
#include <quantum.h>
#include "config.h"
#include <matrix.h>
#include <avr/io.h>
#include <gpio.h>
#include <print.h>
#include <analog.h>


const pin_t row_pins[] = MATRIX_ROWS_PINS;
const pin_t amux_sel[] = AMUX_SEL_PINS;
uint16_t ec_noise_floor[MATRIX_ROWS][MATRIX_COLS];


// Инициализация АЦП
void adc_init(void){
    analogReference(ADC_REF_POWER);
    //ADMUX = (1 << REFS0); // Опорное напряжение == Vcc
    //ADCSRA |= (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2); // Делитель 128 для установки частоты ацп 125Кгц
    //ADCSRA |= (1 << ADEN); // Включение АЦП
    DIDR0 |= (1 << ADC0D); // Отключение цифрогово входа на входном пине F0
}

// Инициализация пинов
void pins_init(void){

    gpio_set_pin_input(ANALOG_READINGS_INPUT); // Аналоговый пин становится входом
    gpio_set_pin_output(AMUX_EN_PINS); // AMUX_EN становится выходом
    gpio_write_pin_low(DISCHARGE_PIN); // Инициализация пина для разрядки ряда
    gpio_set_pin_output(DISCHARGE_PIN); // Инициализация пина для разрядки ряда

    for (uint8_t i = 0; i < (sizeof(row_pins) / sizeof(row_pins[0])); i++) // Выставляем управлящие пины ряды матрицы как выходы + low
    {
        gpio_set_pin_output(row_pins[i]);
        gpio_write_pin_low(row_pins[i]); 
    }

    for (uint8_t i = 0; i < (sizeof(amux_sel) / sizeof(amux_sel[0])); i++) // Выставляем управлящие пины мультиплексора как выходы + low
    {
        gpio_set_pin_output(amux_sel[i]);
        gpio_write_pin_low(amux_sel[i]);
    }
}

// Функция зарядки ряда
void row_charge(pin_t pin){

    gpio_set_pin_input(DISCHARGE_PIN); // Отключаем пин разрядки

    gpio_write_pin_high(pin); // Включаем зарядку
    wait_us(CHARGE_TIME_US); // Ждем зарядку
}

// Функция разрядки отрезка от MUX до ADC
void pin_discharge(void)
{
    gpio_write_pin_low(DISCHARGE_PIN); // Разрядный пин в low
    gpio_set_pin_output(DISCHARGE_PIN); // Включаем разрядку
    wait_us(DISCHARGE_TIME_US); // Ждем разрядки
}

// Функция для выбора каналов мультиплексора
void mux_channel_select(uint8_t col) 
{
    for (uint8_t i = 0; i < (sizeof(amux_sel) / sizeof(amux_sel[0])); i++) // Выбираем значение low или high на основе номера нужной колонки
    {
        gpio_write_pin(amux_sel[i], (col >> i) & 1);
    }
    wait_us(10); // Ждем на всякий случай чтобы точно переключилось
    gpio_write_pin_low(AMUX_EN_PINS); // MUX вкл
}

// Сканирование RAW значений с конкретного датчика по адресу в матрице
uint16_t ec_sw_scan_raw(uint8_t col, uint8_t row)
{
    uint16_t raw_adc_readings = 0;
    mux_channel_select(col);

    gpio_write_pin_low(row_pins[row]);
    wait_us(DISCHARGE_TIME_US); // Разряжаем ряд

    ATOMIC_BLOCK_FORCEON                                                        
    {
        row_charge(row_pins[row]);
        wait_us(10);
        raw_adc_readings = analogReadPin(ANALOG_READINGS_INPUT); // Читаем и записываем в переменную
    }

    gpio_write_pin_high(AMUX_EN_PINS); // MUX выкл

    pin_discharge();

    return raw_adc_readings;
}

void ec_matrix_noise_sample(void)
{
    for (uint8_t col = 0; col < MATRIX_COLS; col++) // Заполняем таблицу нулями
    {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++)
        {
            ec_noise_floor[col][row] = 0;
        } 
    }
    
    for (uint8_t sample = 0; sample < NOISE_FLOOR_SAMPLING_COUNT; sample++) // Производим семплинг и записываем значения
    {
        for (uint8_t col = 0; col < MATRIX_COLS; col++)
        {
            for (uint8_t row = 0; row < MATRIX_ROWS; row++)
            {
                ec_noise_floor[col][row] += ec_sw_scan_raw(col, row);
            } 
        }
    }

    for (uint8_t col = 0; col < MATRIX_COLS; col++) // Усредняем значения
    {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++)
        {
            ec_noise_floor[col][row] /= NOISE_FLOOR_SAMPLING_COUNT;
        } 
    }
}

//Сканирование матрицы и обновление current_matrix
bool ec_matrix_scan(matrix_row_t current_matrix[])
{
    bool matrix_has_changed = false;
    for (uint8_t col = 0; col < MATRIX_COLS; col++)
    {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++)
        {
            uint16_t raw_adc_readings = ec_sw_scan_raw(col, row);
            uint8_t previous_state = (current_matrix[row] >> col) & 1;

            if (raw_adc_readings < RELEASE_LEVEL && previous_state == 1) // Сравниваем текущее состояние с предыдущим и решаем в каком состоянии клавиша сейчас
            {
                current_matrix[row] &= ~(1 << col); // Отпускаем
                matrix_has_changed = true;
            }
            else if (raw_adc_readings > ACTUATION_LEVEL && previous_state == 0)
            {
                current_matrix[row] |= (1 << col); // Нажимаем
                matrix_has_changed = true;
            }
        }
    }
    return matrix_has_changed;
}

// Инициализация при старте (СТАНДАРТНАЯ ФУНКЦИЯ)
void matrix_init_custom(void){
    
    pins_init();
    adc_init();
    ec_matrix_noise_sample();
}


// Скан матрицы (СТАНДАРТНАЯ ФУНКЦИЯ)
bool matrix_scan_custom(matrix_row_t current_matrix[]){
    bool matrix_has_changed = ec_matrix_scan(current_matrix);

    wait_ms(100); // Cнижаем частоту опроса для тестов
    return matrix_has_changed;
}
