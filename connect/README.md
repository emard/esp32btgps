# Cables and connectors

ADXL355 12-pin male PMOD pinout (parts up, looking at the pins)

|    1    |    2    |    3    |    4    |    5    |    6    |
| ------- | ------- | ------- | ------- | ------- | ------- |
| 01:CS   | 02:MOSI | 03:MISO | 04:SCLK | 05:GND  | 06:3.3V |
| 07:INT1 | 08:NC   | 09:INT2 | 10:DRDY | 11:GND  | 12:3.3V |

14-pin female connector (alignment notch down, looking at the holes).
Pins 13 and 14 of 14-pin cable not connected.
Cable crimped to pins 1-12.

|    1    |    2    |    3    |    4    |    5    |    6    |    7    |
| ------- | ------- | ------- | ------- | ------- | ------- | ------- |
| 02:CS   | 04:MOSI | 06:MISO | 08:SCLK | 10:GND  | 12:3.3V | 14:     |
| 01:INT1 | 03:NC   | 05:INT2 | 07:DRDY | 09:GND  | 11:3.3V | 13:     |

12-pin cable going out of 14-pin female connector:

|   1   |   2   |   3   |   4   |   5   |   6   |   7   |   8   |   9   |  10   |  11   |  12   |
| ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- |
| INT1  |  CS   |  NC   | MOSI  | INT2  | MISO  | DRDY  | SCLK  |  GND  |  GND  | 3.3V  | 3.3V  |

Cable reduction: from 12-pin cable, peel out 3 wires,
leaving 9 pins to fit into DB9 connector:

|   1   |   2   |   3   |   4   |   5   |   6   |   7   |   8   |   9   |  10   |  11   |  12   |
| ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- |
| INT1  |  CS   |       | MOSI  | INT2  | MISO  | DRDY  | SCLK  |       |  GND  | 3.3V  |       |

DB9 male pinout:

|   1   |   6   |   2   |   7   |   3   |   8   |   4   |   9   |   5   |
| ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- |
| INT1  |       | MOSI  |       | MISO  |       | SCLK  |       | 3.3V  |
|       |  CS   |       | INT2  |       | DRDY  |       |  GND  |       |

40-pin female connector for ULX3S (alignment notch down, looking at the
holes):

|    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |    9    |   10    |   11    |   12    |   13    |   14    |   15    |   ...   |
| ------- | ------- | ------- | ------- | ------- | ------- | ------- | ------- | ------- | ------- | ------- | ------- | ------- | ------- | ------- | ------- |
| 02:3.3V | 04:GND  | 06:GP14 | 08:GP15 | 10:GP16 | 12:GP17 | 14:GP18 | 16:GP19 | 18:GP20 | 20:3.3V | 22:GPD  | 24:GP21 | 26:GP22 | 28:GP23 | 30:GP24 |   ...   |
| 01:3.3V | 03:GND  | 05:GN14 | 07:GN15 | 09:GN16 | 11:GN17 | 13:GN18 | 15:GN19 | 17:GN20 | 19:3.3V | 21:GND  | 23:GN21 | 25:GN22 | 27:GN23 | 29:GN24 |   ...   |

40-pin flat cable:

|  1   |  2   |  3  |  4  |  5   |  6   |  7   |  8   |  9   |  10  |  11  |  12  |  13  |  14  |  15  |  16  |  17  |  18  |  19  |  20  | 21  | 22  |  23  |  24  |  25  |  26  |  27  |  28  |  29  |  30  | ... |
| ---- | ---- | --- | --- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | --- | --- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | --- |
| 3.3V | 3.3V | GND | GND | GN14 | GP14 | GN15 | GP15 | GN16 | GP16 | GN17 | GP17 | GN18 | GP18 | GN19 | GP19 | GN20 | GP20 | 3.3V | 3.3V | GND | GND | GN21 | GP21 | GN22 | GP22 | GN23 | GP23 | GN24 | GP24 | ... |

cable reduction: peel out wires to fit into two DB9 connectors:

|  1   |  2   |  3  |  4  |  5   |  6   |  7   |  8   |  9   |  10  |  11  |  12  |  13  |  14  |  15  |  16  |  17  |  18  |  19  |  20  | 21  | 22  |  23  |  24  |  25  |  26  |  27  |  28  |  29  |  30  | ... |
| ---- | ---- | --- | --- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | --- | --- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | --- |
|      | 3.3V | GND |     | GN14 | GP14 | GN15 | GP15 | GN16 |      | GN17 | GP17 |      |      |      |      |      |      |      | 3.3V | GND |     | GN21 | GP21 | GN22 | GP22 | GN23 |      | GN24 | GP24 | ... |

DB9 female pinout (ULX3S, looking at the holes):

|   5   |   9   |  4   |  8   |  3   |  7   |  2   |  6   |  1   |
| ----- | ----- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 3.3V  |       | GN14 |      | GN15 |      | GN16 |      | GP17 |
|       |  GND  |      | GP14 |      | GP15 |      | GN17 |      |

|   5   |   9   |  4   |  8   |  3   |  7   |  2   |  6   |  1   |
| ----- | ----- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 3.3V  |       | GN21 |      | GN22 |      | GN23 |      | GP24 |
|       |  GND  |      | GP21 |      | GP22 |      | GN24 |      |

External antennas to 40-pin flat cable:

|  16  |  18  |
| ---- | ---- |
| GP19 | GP20 |
| ANT1 | ANT2 |
