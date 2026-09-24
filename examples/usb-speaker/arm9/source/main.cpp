#include <nds.h>
#include <stdio.h>
#include <stdint.h>

// Размер кадра 256x192 в байтах (RGB555: 2 байта на пиксель = 98 304 байт)
#define FRAME_SIZE (256 * 192 * 2)

// Если буфер передается через Shared WRAM, лучше объявить его как указатель на абсолютный адрес 
// или согласовать адрес с ARM7 через общую структуру / фиксированный участок памяти.
// Пример (замени адрес на тот, который выделен у тебя под буфер в linker script или WRAM):
#define SHARED_FRAME_BUFFER_ADDR 0x02380000 
#define gVideoFrameBuffer ((const u8*)SHARED_FRAME_BUFFER_ADDR)

void initVideo()
{
    // Включаем питание видео-подсистемы и WRAM
    sysSetPower(POWER_ALL_2D);

    // Настраиваем VRAM A под прямой вывод на верхний экран (Main Engine)
    vramSetBankA(VRAM_A_LCD);
    
    // Включаем режим прямого вывода VRAM (Direct Display Mode)
    REG_DISPCNT = MODE_FIFO | DISPLAY_BG0_ACTIVE;
    
    // Устанавливаем режим графики VRAM Display Mode
    videoSetMode(MODE_FB0);
}

int main(void)
{
    // Инициализация видеорежима ARM9
    initVideo();

    // Ждем полной загрузки и готовности ARM7 (IPC Sync bit 7)
    while ((REG_IPC_SYNC & 0x07) != 7)
    {
        swiWaitForVBlank();
    }

    // Очищаем биты синхронизации
    REG_IPC_SYNC &= ~0x0f;

    // Указатель на видеопамять верхнего экрана
    u16* vramBuffer = VRAM_A;

    while (1)
    {
        // Ожидаем прерывания VBlank для синхронизации кадров (60 FPS)
        swiWaitForVBlank();

        // Проверяем, поднял ли ARM7 флаг готовности нового кадра (IPC Sync bit 1)
        if (REG_IPC_SYNC & 0x01)
        {
            // Сбрасываем флаг приема
            REG_IPC_SYNC &= ~0x01;

            // Копируем кадр из разделяемой памяти в видеопамять через быстрый 32-битный DMA (канал 3)
            dmaCopyWords(3, gVideoFrameBuffer, vramBuffer, FRAME_SIZE);
        }
    }

    return 0;
}