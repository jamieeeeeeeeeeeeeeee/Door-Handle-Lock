#line 1 "/home/runner/work/doorhandlelock/doorhandlelock/device/src/picow/common/pimoroni_bus.cpp"
#include "pimoroni_bus.hpp"

namespace pimoroni {
    SPIPins get_spi_pins(BG_SPI_SLOT slot) {
        switch(slot) {
        case PICO_EXPLORER_ONBOARD:
          return {PIMORONI_SPI_DEFAULT_INSTANCE, SPI_BG_FRONT_CS, SPI_DEFAULT_SCK, SPI_DEFAULT_MOSI, PIN_UNUSED, SPI_DEFAULT_DC, PIN_UNUSED};
        case BG_SPI_FRONT:
          return {PIMORONI_SPI_DEFAULT_INSTANCE, SPI_BG_FRONT_CS, SPI_DEFAULT_SCK, SPI_DEFAULT_MOSI, PIN_UNUSED, SPI_DEFAULT_DC, SPI_BG_FRONT_PWM};
        case BG_SPI_BACK:
          return {PIMORONI_SPI_DEFAULT_INSTANCE, SPI_BG_BACK_CS, SPI_DEFAULT_SCK, SPI_DEFAULT_MOSI, PIN_UNUSED, SPI_DEFAULT_DC, SPI_BG_BACK_PWM};
        }
        return {PIMORONI_SPI_DEFAULT_INSTANCE, SPI_BG_FRONT_CS, SPI_DEFAULT_SCK, SPI_DEFAULT_MOSI, PIN_UNUSED, SPI_DEFAULT_DC, SPI_BG_FRONT_PWM};
    };
}