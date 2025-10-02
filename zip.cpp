#include <FS.h>
#include "miniz.h"

// Callback function to read data from the input file
size_t read_file_callback(void* pOpaque, mz_uint64 file_ofs, void* pBuf, size_t n) {
    File* inputFile = static_cast<File*>(pOpaque);
    if (inputFile->seek(file_ofs))
        return inputFile->read(static_cast<uint8_t*>(pBuf), n);
    return 0;
}

// open() files before zip()
// close() files after zip()
// speed of zipping on SD_MMC card is about 300 K/s
// return 0: fail
// return 1: success
int zip(File &kmzOutputFile, File &kmlInputFile, const char *archived_name)
{
  size_t kml_file_size = kmlInputFile.size();

  mz_zip_archive zip_archive;
  memset(&zip_archive, 0, sizeof(zip_archive));

  // Set custom callbacks for writing to the SD file
  zip_archive.m_pIO_opaque = &kmzOutputFile; // Pass the File object pointer
  zip_archive.m_pWrite = [](void* pOpaque, mz_uint64 ofs, const void* pBuf, size_t n) -> size_t 
  {
    File* file = static_cast<File*>(pOpaque);
    if (file->seek(ofs)) // Seek to the offset
      return file->write(static_cast<const uint8_t*>(pBuf), n);
    return 0; // Return 0 on error
  };

  // Initialize zip writer to output directly to the SD card file
  // This uses a custom writer function to interface with Arduino File object
  if (!mz_zip_writer_init(&zip_archive, 0)) { // 0 for initial allocation
    Serial.println("mz_zip_writer_init failed!");
    return 0; // error
  }

  // Add the KML file to the KMZ archive using the read callback
  // The internal filename in the KMZ will be "doc.kml" as per KMZ standard
  if (!mz_zip_writer_add_read_buf_callback(
            &zip_archive,
            archived_name,          // Name inside the KMZ archive
            read_file_callback,     // Our custom read callback
            &kmlInputFile,          // The user_data for the callback (our KML File object address)
            kml_file_size,          // max size = Original size of the data
            NULL, // pointer to TIME struct, since 1980-01-01
            NULL, // command
            0, // comment size
            MZ_DEFAULT_LEVEL,  // Compression level
            NULL, // user extra data
            0, // user extra data len
            NULL, // user extra data central
            0 // user extra data central len
      )) {
      Serial.println("mz_zip_writer_add_read_buf_callback() failed!");
      mz_zip_writer_end(&zip_archive); // Clean up zip writer
      return 0; // error
  }

  // Finalize the archive
  if (!mz_zip_writer_finalize_archive(&zip_archive)) {
    Serial.println("mz_zip_writer_finalize_archive failed!");
    mz_zip_writer_end(&zip_archive);
    return 0; // error
  }

  // End the writer
  if (!mz_zip_writer_end(&zip_archive)) {
    Serial.println("mz_zip_writer_end failed!");
    return 0; // error
  }
  return 1; // success
}
