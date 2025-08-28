# TODO

esp32

    [ ] kml: arrows in separate foloder (close/open folders)
    [ ] kml: id's (maybe names too) for folders and icons
    [ ] if wav is not enabled (only kml) then no-sensors reported although sensors work
        (try to check is IRI = 0 as no-sensors condition)
    [ ] what happens to kml and stat in tunnel mode?
    [ ] tunnel mode kml generate follow the same direction
    [ ] optional stat for whole day, allows continuation between reboots
    [ ] OBD testing (check previously failing devices) with new arduino esp32 2.0.x
    [x] beep when SD mount failed (no speak)
    [x] gps off and .sta file remains
    [x] instead of default lat=46, calculate_grid(latitude) at first point of new storage
    [x] arrow heading before turn is a bit off, fixed by static new_heading
    [x] creation timestamp to stat arrows or ascending time
    [ ] avoid double nmea parsing for kml and stat (CPU saving)
    [x] stat speed min-max range
    [ ] kml config option for lines and arrows
    [ ] kml demo circular run
    [x] report pass count on RDS and LCD
    [x] when standing, display LAT/LON in decimal format
    [x] ESP32 reports IRI 20% smaller than wav2kml (disable matrix_write)
    [ ] at each stop (with temperatures) write iri99avg
    [ ] display last iri99avg in .wav files list
    [x] configurable speed for reporting every 100/20 m
    [x] LCD: if no SD card, print insert SD card
    [x] LCD: file list, print 1 line for wav or kml if wav disabled
    [x] FM short beep if no SD
    [x] crash after temperatures in log, sprintf 20 bytes overrun
    [x] right sensor temperature -199.9C, lower SPI freq 8->5 MHz
    [x] support arduino ESP32 v2.0.x and ESP32DMASPI v0.2.0
    [x] allow wifi reconnect at arduino ESP32 v2.0.x, check disabled wifiMulti.run()
    [x] LCD display with freq and status
    [x] LCD display IP address and hostname
    [ ] LCD graphic track display
    [x] LCD cursor FM frequency setting with BTN
    [x] FM avoid freqs closer than 0.3 MHz, jump
    [x] print and say REBOOT on FM/RDS at startup
    [x] BTN freq autorepeat jump 1 MHz
    [x] save FM freq setting to SD card
    [x] search for GPS/OBD if nonzero config exists
    [x] don't report "GO" if no sensors
    [x] when stopped, each minute enter direct mode, read temperatures
    [x] when stopped, each minute initialize sensors
    [x] config file temperature calibration
    [x] write temperature to log at each start
    [ ] RTC support (for OBD without GPS)
    [ ] color scale config and description
    [x] configurable report frequency (m)
    [ ] notify g-range in wav
    [ ] notify g-range in kml
    [ ] kml->kmz zip https://github.com/lbernstone/miniz-esp32
    [ ] kml handle missing sensors
    [x] sensor hotplug (initialized every minute when stopped)
    [x] script for binary exe upload to esp32 and fpga
    [ ] GPS time discontinuety bug warning
    [ ] TA flag at errors
    [ ] low free: erase oldest files, stop logging
    [ ] speech report when connected GPS/OBD
    [ ] speech report remaining minutes
    [x] speech report disk full
    [ ] speech tunnel mode locked speed
    [ ] fix wav file to open with wave.open("file.wav","r")
    [ ] support >9.9 speech
    [ ] all DMA transfer sizes divisible by 4
    [ ] btn to stop logging and close all files
    [x] 60GB free, but shown 3529MB, use uint64_t and float
    [x] print ADX chip not detected, now unconnected ADXRS290 is "detected"
    [x] detecting left ADXRS290
    [x] sometimes false report no L/R sensor
    [ ] OBD2 stop PPS PLL
    [ ] web visited links in different color
    [x] web server LED when activated and connected
    [ ] web MDNS not updating address to dnsmasq
    [x] web server list dump only filenames not full path
    [x] web free using JSON var name for bytes free in dir listing
    [x] web server show only filename, not full path
    [x] web server sort directories/files
    [x] web server files to separate directory
    [x] web keep retrying to connect
    [x] web roaming, multiple ap/pass
    [x] web speak IP address
    [x] EPS32 send to SPI speed, c/speed, report and tag iri
    [x] spoken report when sensors are missing
    [x] WiFi server
    [x] audio output to 3.5 mm jack
    [x] speech immediately at start logging
    [x] start/stop recording at speed hysteresis
    [x] sensor L/R status monitor and error reporting
    [x] RT display disk free
    [x] file named with timestamp
    [x] rename speech files to english filenames
    [x] 1-char 2^n MB free display 0-9
    [x] 1-char sensor status XLRY
    [x] test power lost during logging
    [x] config file for BT MAC, WiFi password and few parameters
    [x] tunnel mode: if signal is lost at > 40 km/h, assume same speed
    [x] CRC for IRI tags
    [x] OBD2 support
    [x] OBD2 umount when BT lost
    [x] OBD2 stop count to lon degs
    [x] tag time and speed for OBD mode
    [x] BT connect GPS/OBD automatic
    [x] RDS display GPS/OBD mode
    [x] OBD2 time from saved last GPS location
    [x] OBD2 start from saved last GPS location
    [x] web upload reports "Error" although it's successful
    [x] get bytes free early to display when GPS is OFF
    [x] web show MB free
    [x] SD card compatiblity sandisk
    [x] web shorten time from power to BTN0 reading
    [x] web server bug eraseing HTML name when file in non-root directory is deleted
    [x] kml OBD mode bugs last latlon overwritten
    [x] kml generation
    [x] kml iterate old logs and finalize
    [x] kml sometimes name (lastnmea) filled with junk string
    [ ] wav log off by 1 byte, peristent at stops
    [ ] sometimes kml not finalized (1 minute not passed?)
    [ ] kml wrong line color and placemarks with gnome firefox maps
    [x] web finalize before starting server
    [x] config log mode wav/kml
    [x] kml date in the document description
    [x] sensor range +-2-4-8 g configurable
    [x] if left sensor is missing, right can be detected
    [x] report identical GPS positions in wav and kml
    [ ] snap length config parameter
    [x] latlon float->double for last digit precision
        lat,lon 45,16 quantizes approx as 46.000003,16.000002
    [x] for first minute reports sensors NONE, delay initial
        report print for sensors to be detected
    [ ] for 1Hz GPS update, make more dense lines
        split line to segments of configurable length
    [ ] fine points
        stat - split data to input struct with double lat/lon
        generate series of double lat/lon in linear segment
        record starting lat/lon,time
        log time and iri values for each point of segment
        distributed timestamped values in space - calculate lat/lon
        for each fine point
        call stat for each fine point and generate split line
        call line to place the line on the generated point
    [ ] make separate code to try this logic
    [ ] on android GPS it says ready before it gets FIX
    [ ] last NMEA written only time on lat/lon
        check for 'A' letter as FIX (insted of 'V')

core

    [ ] filter out glitches: left sensor sometimes in one sample reads all 3 axis
        as 0x00 or 0xFF (wavhexdump.sh 15-bit).
        depends on GP/GN 4/8/16 mA and cable length but no combination fits
    [ ] core analzyer, SPI register report of signal validity
    [ ] on initial screen write REBOOT, power OFF->ON as user guidance
    [x] 12-byte spi buf to wav buf alignment
    [x] trellis weak FM (fixed by signed->unsigned multiply)
    [x] core reports IRI 20% smaller than wav2kml (forgot to disable write_matrix)
    [ ] parametrize FM multiply bits (currently 64 bit result not used fully)
    [ ] handle delay from speed measurement to accelerometer reading (IRI 9.9 after start)
    [ ] increase speech volume (compression?)
    [ ] time sync status
    [ ] BTN irq
    [ ] tyre ribs or motor vibration (RPM) sensing, conversion to speed
    [ ] option to reset slope at stops (configurable)
    [x] support IRI-100 and IRI-20
    [x] spi slave setting for cmd to auto-read registers adxl355=8*2+1, adxrs290=128+8
    [x] damp oscillations at slope DC offset compensation
    [x] at stops, quick slope DC removal
    [x] improved slope DC removal
    [x] slope should not reset at 0-speed
    [x] output register for trellis clean FM signal
    [x] improve audio quality with DACPWM
    [x] latch calc_result from changing while reading
    [x] dual FM frequency output
    [x] FM frequency set
    [x] SPI slave for speed and iri
    [x] random inbalance in L/R calc results (slope ready not used)
    [ ] math to calculate IRI from sensors inside the car

tools (wav2kml etc)

    [x] csv sh script runnable from any directory
    [x] csv dump
    [ ] geostat in python
    [x] if nmea line checksum wrong don't process it
    [ ] on track and placemarks indicate tunnel mode
    [ ] motor vibration (RPM) sensing, conversion to speed
    [ ] calibrate accelerometer heading with GPS
    [ ] use accelerometer to determine heading in tunnel mode
    [ ] check/fix gyro calc, iri100 difference soft/hard calc
    [x] at stop cut the track and restart new
    [x] multiple input files
    [x] placemark every 100 m
    [x] lookat straight above
    [x] report speed range min/max to placemarks
    [x] snap
    [x] colorized description
        https://developers.google.com/kml/documentation/kml_tut
    [ ] improve snap
    [ ] unbreak for android gps

makecircle

    [x] wav generator to generate track of known values

math

    [x] verify method - make the offline analysis
