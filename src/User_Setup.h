// TFT_eSPI User Setup for ESP32-S3 AI Board
// ST7789 170x320 1.9" IPS Display

#define USER_SETUP_LOADED

// Driver
#define ST7789_DRIVER

// Display dimensions
#define TFT_WIDTH  170
#define TFT_HEIGHT 320

// ESP32-S3 AI Board pin assignments
#define TFT_MOSI   10   // SDA
#define TFT_SCLK   12   // SCL
#define TFT_CS     13
#define TFT_DC     11
#define TFT_RST    14
#define TFT_BL      3   // Backlight (directly controllable)

// Optional touch - not present on this board
// #define TOUCH_CS -1

// Fonts
#define LOAD_GLCD    // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2   // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4   // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6   // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7   // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8   // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF   // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48

#define SMOOTH_FONT

// SPI frequency
#define SPI_FREQUENCY       80000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
