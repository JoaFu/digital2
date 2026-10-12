#include "galaga_dma.h"
#include "main.h"
extern SPI_HandleTypeDef hspi1;
static DMA_HandleTypeDef dmaTx;
static volatile uint8_t ocupado,fallo;
static uint32_t inicio;
volatile uint32_t galagaTransferenciasDMA,galagaErroresDMA;

void GD_Iniciar(void) {
    /* RM0390: SPI1_TX = DMA2, stream 3, canal 3. Inicializacion propia;
       no activar otra instancia para SPI1_TX en CubeMX a la vez. */
    __HAL_RCC_DMA2_CLK_ENABLE();
    dmaTx.Instance=DMA2_Stream3;
    dmaTx.Init.Channel=DMA_CHANNEL_3;
    dmaTx.Init.Direction=DMA_MEMORY_TO_PERIPH;
    dmaTx.Init.PeriphInc=DMA_PINC_DISABLE;
    dmaTx.Init.MemInc=DMA_MINC_ENABLE;
    dmaTx.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;
    dmaTx.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;
    dmaTx.Init.Mode=DMA_NORMAL;
    dmaTx.Init.Priority=DMA_PRIORITY_HIGH;
    dmaTx.Init.FIFOMode=DMA_FIFOMODE_DISABLE;
    if(HAL_DMA_Init(&dmaTx)!=HAL_OK) {Error_Handler();return;}
    __HAL_LINKDMA(&hspi1,hdmatx,dmaTx);
    /* SysTick puede interrumpir al HAL durante un error de cierre SPI. */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    HAL_NVIC_SetPriority(SysTick_IRQn,0,0);
    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn,2,0);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_SetPriority(SPI1_IRQn,2,0);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
}
void GD_Enviar(const uint8_t *datos,uint16_t cantidad) {
    if(ocupado || fallo || !cantidad) {++galagaErroresDMA;Error_Handler();return;}
    inicio=HAL_GetTick(); ocupado=1;
    __DMB();
    if(HAL_SPI_Transmit_DMA(&hspi1,datos,cantidad)!=HAL_OK) {
        ocupado=0; fallo=1; ++galagaErroresDMA;
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port,LCD_CS_Pin,GPIO_PIN_SET);
        Error_Handler();return;
    }
    ++galagaTransferenciasDMA;
}
uint8_t GD_Ocupado(void) {
    if(fallo || (ocupado && (uint32_t)(HAL_GetTick()-inicio)>100U)) {
        HAL_SPI_Abort(&hspi1); ocupado=0;fallo=1;++galagaErroresDMA;
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port,LCD_CS_Pin,GPIO_PIN_SET);
        Error_Handler();
    }
    return ocupado;
}
void GD_Esperar(void) {while(GD_Ocupado()) { /* acotado por watchdog de 100 ms */ }}
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if(hspi!=&hspi1)return;
    /* El HAL ya espero TXE/BSY, limpio OVR y dejo SPI READY. */
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port,LCD_CS_Pin,GPIO_PIN_SET);
    __DMB(); ocupado=0;
}
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    if(hspi!=&hspi1)return;
    fallo=1; /* Abort y diagnostico fuera de la interrupcion. */
}
void DMA2_Stream3_IRQHandler(void) {HAL_DMA_IRQHandler(&dmaTx);}
void SPI1_IRQHandler(void) {HAL_SPI_IRQHandler(&hspi1);}
