#line 1 "/home/runner/work/doorhandlelock/doorhandlelock/device/src/picow/common/pimoroni_bus.hpp"
#pragma once
#include "pimoroni_common.hpp"
#include "hardware/gpio.h"
#include "hardware/spi.h"

namespace pimoroni {
    struct SPIPins {
        spi_inst_t *spi;
        uint cs;
        uint sck;
        uint mosi;
        uint miso;
        uint dc;
        uint bl;
    };

    struct ParallelPins {
        uint cs;
        uint dc;
        uint wr_sck;
        uint rd_sck;
        uint d0;
        uint bl;
    };

    SPIPins get_spi_pins(BG_SPI_SLOT slot);
}