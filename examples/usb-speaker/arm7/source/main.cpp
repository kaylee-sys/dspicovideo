#include <nds/ndstypes.h>

// Жестко задаем конфигурацию TinyUSB перед подключением заголовков
#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_NONE
#endif

#ifndef CFG_TUD_ENABLED
#define CFG_TUD_ENABLED 1
#endif

#ifndef CFG_TUD_VENDOR
#define CFG_TUD_VENDOR 1
#endif

#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE
#endif

#ifndef TUP_DCD_ENDPOINT_MAX
#define TUP_DCD_ENDPOINT_MAX 8
#endif

extern "C" {
#include "tusb_config.h"

// Переопределяем макросы на случай, если tusb_config.h перебивает их
#undef CFG_TUD_ENABLED
#define CFG_TUD_ENABLED 1

#undef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE

#include "tusb.h"
#include "class/vendor/vendor_device.h"
}

#include <libtwl/rtos/rtosIrq.h>
#include <libtwl/rtos/rtosThread.h>
#include <libtwl/rtos/rtosEvent.h>
#include <libtwl/ipc/ipcSync.h>
#include <libtwl/ipc/ipcFifoSystem.h>
#include <libtwl/sys/sysPower.h>
#include <libtwl/sio/sioRtc.h>
#include <libtwl/sio/sio.h>
#include <libtwl/gfx/gfxStatus.h>
#include <libtwl/mem/memSwap.h>
#include <libtwl/i2c/i2cMcu.h>
#include <libtwl/spi/spiPmic.h>

#include "common.h"
#include "ExitMode.h"
#include "Arm7State.h"
#include "usb_descriptors.h"

#define FRAME_SIZE (256 * 192 * 2)

u8 gVideoFrameBuffer[FRAME_SIZE] alignas(32);
volatile u32 gFrameBytesReceived = 0;

static rtos_thread_t sUsbThread;
static u32 sUsbThreadStack[512];

static rtos_event_t sVBlankEvent;
static ExitMode sExitMode;
static Arm7State sState;

static void vblankIrq(u32 irqMask)
{
    (void)irqMask;
    rtos_signalEvent(&sVBlankEvent);
}

static void usbThreadMain(void* arg)
{
    (void)arg;
    while (true)
    {
        tud_task();
    }
}

// Актуальная сигнатура TinyUSB с 3 параметрами
extern "C" void tud_vendor_rx_cb(uint8_t idx, const uint8_t *buffer, uint32_t bufsize)
{
    (void)idx;
    (void)buffer;
    (void)bufsize;

    uint32_t available = tud_vendor_available();
    
    while (available > 0)
    {
        uint32_t bytesNeeded = FRAME_SIZE - gFrameBytesReceived;
        uint32_t toRead = (available < bytesNeeded) ? available : bytesNeeded;

        uint32_t readCount = tud_vendor_read(&gVideoFrameBuffer[gFrameBytesReceived], toRead);
        gFrameBytesReceived += readCount;

        if (gFrameBytesReceived >= FRAME_SIZE)
        {
            gFrameBytesReceived = 0;
            ipc_setArm7SyncBits(1); 
        }

        available = tud_vendor_available();
    }
}

static void initializeArm7()
{
    rtos_initIrq();
    rtos_startMainThread();
    ipc_initFifoSystem();

    sio_setGpioSiIrq(false);

    rtc_init();

    rtos_createEvent(&sVBlankEvent);
    rtos_setIrqFunc(RTOS_IRQ_VBLANK, vblankIrq);
    rtos_enableIrqMask(RTOS_IRQ_VBLANK);
    gfx_setVBlankIrqEnabled(true);

    tusb_init();

    rtos_createThread(&sUsbThread, 3, usbThreadMain, NULL, sUsbThreadStack, sizeof(sUsbThreadStack));
    rtos_wakeupThread(&sUsbThread);

    ipc_setArm7SyncBits(7);
}

int main()
{
    sState = Arm7State::Idle;
    (void)sState;
    (void)sExitMode;

    initializeArm7();

    while (true)
    {
        rtos_waitEvent(&sVBlankEvent, true, true);
    }

    return 0;
}
