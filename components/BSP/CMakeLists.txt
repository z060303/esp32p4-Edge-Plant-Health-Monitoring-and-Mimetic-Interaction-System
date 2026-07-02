set(src_dirs
            LED
            MYIIC
            LCD
            PLANT_I2C
            SHT30
            BH1750
            SOFTIIC
            SOILDSENCER
            MMWAVE
            TOUCHSENCER
            )

set(include_dirs
            LED
            MYIIC
            LCD
            PLANT_I2C
            SHT30
            BH1750
            SOFTIIC
            SOILDSENCER
            MMWAVE
            TOUCHSENCER
            )

set(requires
            driver
            esp_lcd
            esp_common
            esp_adc
            esp_driver_touch_sens
            )

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)
