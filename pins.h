#if 0
// devkit esp32
// BTN has inverted logic, 0 when pressed
#define PIN_BTN   0
#define PIN_LED   2
#define PIN_PPS  25
#define PIN_IRQ  26
#endif
#if 0
// ULX3S v3.0.x
#define PIN_PPS  25
#define PIN_IRQ  32
#define PIN_LED   5
#define LED_ON    1
#define LED_OFF   0
#define PIN_SCK   0
#define PIN_MISO 33
#define PIN_MOSI 16
#define PIN_CSN  17
#endif
#if 1
// ULX3S v3.1.x
#define PIN_PPS  25
#define PIN_IRQ  32
// #define PIN_LED   5 // shared with JTAG, avoid using during development
#define LED_ON    0
#define LED_OFF   1
#define PIN_SCK   0
#define PIN_MISO 33
#define PIN_MOSI 26
#define PIN_CSN  27
#endif
