// TFT_eSPI 2.5.x configuration for CYD ESP32-2432S028R (ILI9341).
// Reference only: TFT_eSPI reads its own User_Setup.h in ~/Arduino/libraries/TFT_eSPI.
// This is NOT a second ESP32 sensor firmware and is NOT an install script.
#define USER_SETUP_INFO "Grow Remote ESP32-2432S028R"
#define ILI9341_DRIVER
#define TFT_WIDTH 240
#define TFT_HEIGHT 320
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define TFT_BL 21
#define TFT_BACKLIGHT_ON HIGH
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define SMOOTH_FONT
#define SPI_FREQUENCY 27000000
#define SPI_READ_FREQUENCY 16000000
#define SPI_TOUCH_FREQUENCY 2500000
// Do not enable USE_HSPI_PORT: touch runs on a separate HSPI instance.
