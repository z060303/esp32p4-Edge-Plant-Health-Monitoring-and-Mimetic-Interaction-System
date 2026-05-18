set(src_dirs
            LED
            MYIIC
            XL9555
            SHT30
            MYTIMER
            BH1750
            SOFTIIC
            SOILDSENCER
            SPILCD)

set(include_dirs
            LED
            MYIIC
            XL9555
            SHT30
            MYTIMER
            BH1750
            SOFTIIC
            SOILDSENCER
            SPILCD)

set(requires
            driver
            esp_lcd
            esp_adc)

idf_component_register(SRC_DIRS ${src_dirs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})
                       
component_compile_options(-ffast-math -O3 -Wno-error=format=-Wno-format)