#include "dht11.h"
#include "main.h"
#include "tim.h"

#define DHT11_PORT GPIOA
#define DHT11_PIN GPIO_PIN_1

static void DHT11_Pin_Output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; 
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

static void DHT11_Pin_Input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

static void DHT11_Delay_us(uint16_t us)
{
    uint16_t start;

    start = __HAL_TIM_GET_COUNTER(&htim2);

    while((uint16_t)(__HAL_TIM_GET_COUNTER(&htim2) - start) < us)
    {

    }
}

static uint8_t DHT11_WaitForLevel(uint8_t level, uint16_t timeout)
{
    uint16_t start = __HAL_TIM_GET_COUNTER(&htim2);

    while(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) != level)
    {
        if((uint16_t)(__HAL_TIM_GET_COUNTER(&htim2) - start) > timeout)
        {
            return 0; // Timeout
        }
    }
    return 1; // Success
}

uint8_t DHT11_READ(uint8_t *temp, uint8_t *humi)
{
    uint8_t data[5] = {0};
    uint8_t i, j;

    if(temp == NULL || humi == NULL)
    {
        return 0;
    }
    // Start signal
    DHT11_Pin_Output();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(20); // Hold low for at least 18ms
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    DHT11_Pin_Input();

    // Wait for response
    DHT11_Delay_us(30); // Wait for 20-40us
    if(!DHT11_WaitForLevel(GPIO_PIN_RESET, 100)) return 0; // No response
    if(!DHT11_WaitForLevel(GPIO_PIN_SET, 100)) return 0; // No response

    // Read 40 bits (5 bytes)
    for(i = 0; i < 5; i++)
    {
        for(j = 0; j < 8; j++)
        {
            if(!DHT11_WaitForLevel(GPIO_PIN_RESET, 100)) return 0; // Timeout waiting for low
            if(!DHT11_WaitForLevel(GPIO_PIN_SET, 100)) return 0; // Timeout waiting for high

            DHT11_Delay_us(40); // Wait for 40us to sample the bit

            if(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
            {
                data[i] |= (1 << (7 - j)); // Set bit
            }
        }
    }

    // Checksum validation
    if(data[4] != (data[0] + data[1] + data[2] + data[3])) return 0; // Checksum error

    *humi = data[0];
    *temp = data[2];

    return 1; // Success
}
