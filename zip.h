#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include "miniz.h"

// open() files before zip()
// close() files after zip()
// speed of zipping on SD_MMC card is about 300 K/s
int zip(File &kmzOutputFile, File &kmlInputFile, const char *archived_name);
