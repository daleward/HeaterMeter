#if defined(ESP8266)
    #define DPIN_LED_CLK    D1
    #define DPIN_LED_DISP0  D2
    #define DPIN_LED_DISP1  D7
    #define DPIN_LED_DISP2  D6
    #define DPIN_LED_DISP3  D5
    #define DPIN_BUTTON     D4
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    #define DPIN_LED_CLK    8
    #define DPIN_LED_DISP0  0
    #define DPIN_LED_DISP1  1
    #define DPIN_LED_DISP2  2
    #define DPIN_LED_DISP3  3
    #define DPIN_BUTTON     9
#else
    #define DPIN_LED_CLK    1
    #define DPIN_LED_DISP0  2
    #define DPIN_LED_DISP1  3
    #define DPIN_LED_DISP2  4
    #define DPIN_LED_DISP3  5
    #define DPIN_BUTTON     0
#endif