# TODO

esp32

    [ ] SD card compatiblity sandisk
    [ ] sensor hotplug
    [ ] RTC support (for OBD without GPS)
    [ ] LCD display with freq and status
    [ ] LCD display IP address and hostname
    [ ] LCD graphic track display
    [ ] GPS time discontinuety warning
    [ ] OBD2 stop PPS PLL
    [ ] TA flag at errors
    [ ] low free: erase oldest files, stop logging
    [ ] speech report when connected BPS/OBD
    [ ] speech report remaining minutes and disk full
    [ ] speech tunnel mode locked speed
    [ ] get bytes free early to display when GPS is OFF
    [ ] fix wav file to open with wave.open("file.wav","r")
    [ ] >9.9 speak "out of scale"
    [ ] btn to stop logging
    [ ] web roaming, multiple ap/pass
    [ ] wep upload success readyState == 4, status == 200
    [ ] web server LED blink when activated and connected
    [ ] web server show SD free MB
    [ ] web server files to separate directory
    [ ] web MDNS not updating address to dnsmasq
    [ ] web speak when can't connect
    [ ] web server list dump only filenames not full path
    [x] web server show only filename, not full path
    [x] web server sort directories/files
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
    [x] BT connect OBD/GPS automatic
    [x] RDS display GPS/OBD mode
    [x] OBD2 time from saved last GPS location
    [x] OBD2 start from saved last GPS location
    [x] web upload reports "Error" although it's successful

core

    [ ] significant L/R unbalance at taking the turns
        clipping (out of range) or
        different state of slope DC offset compensation?
    [ ] damp oscillations at slope DC offset compensation
    [ ] core fm filter and downsample not working (trellis?)
    [ ] handle delay from speed measurement to accelerometer reading
    [ ] log sensor temperature
    [ ] increase speech volume (compression?)
    [ ] time sync status
    [ ] BTN irq
    [ ] motor vibration (RPM) sensing, conversion to speed
    [x] slope should not reset at 0-speed
    [x] output register for trellis clean FM signal
    [x] improve audio quality with DACPWM
    [x] latch calc_result from changing while reading
    [x] dual frequency output 87.6 and 107.9
    [x] SPI slave for speed and iri
    [x] random inbalance in L/R calc results (slope ready not used)

wav2kml

    [ ] csv dump
    [ ] snap https://automating-gis-processes.github.io/2017/lessons/L3/nearest-neighbour.html
        find approached point, path is linear, binary search to refine
        approached area
    [ ] on track and placemarks indicate tunnel mode
    [ ] motor vibration (RPM) sensing, conversion to speed
    [ ] calibrate accelerometer heading with GPS
    [ ] use accelerometer to determine heading in tunnel mode
    [x] at stop cut the track and restart new
    [x] multiple input files
    [x] placemark every 100 m
    [x] lookat straight above
    [x] report speed range min/max to placemarks
