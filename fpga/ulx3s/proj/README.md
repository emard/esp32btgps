# ADXL355 logger large project

# TODO

    [x] when reading 2-byte buffer pointer
        latch LSB byte when reading LSB for
        consistent 16-bit reading
    [x] write to SPI IO for tagging
    [x] write to SPI IO for audio message
    [x] write to SPI IO for RDS display
    [ ] generate RDS message
    [x] mount sandisk SD card
    [x] SD card hotplug
    [ ] sensors hotplug
    [ ] stop logging below minimal speed (hysteresis 2-7 km/h)
    [x] check NMEA crc
    [x] parse NMEA to get time and date
    [x] separate NMEA related funcs to nmea module
    [x] set system time from NMEA
    [ ] create log directory
    [ ] file names with creation time string
    [ ] at low disk space, erase oldest data until 100 MB free
    [ ] more audio messages
    [ ] speak waiting for GPS signal
    [ ] speak saving data
    [ ] speak low battery
    [ ] speak no sensors (left, right)
    [ ] speak each 100 m
    [ ] speak raw measurement estimate
    [ ] speak erasing old data
    [ ] fix bugs described in http://4river.a.la9.jp/gps/report/GLO.htm
