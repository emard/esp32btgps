// open() files before zip()
// close() files after zip()
// speed of zipping on SD_MMC card is about 300 K/s
// return 0: fail
// return 1: success
int zip(File &kmzOutputFile, File &kmlInputFile, const char *archived_name);
