/*
  Have a FAT Formatted SD Card connected
  to the MMC pins of the ESP32

  The web root is the SD card root folder

  index.htm is the default index (works on subfolders as well)

  upload the contents of "websd" directory, files
  index.htm, styles.css, progress.png, favicon.ico
  to the root of the SD card

  hold BTN0 and plug the USB power
  
  access web by going to
  http://esp32sd.local

  cd websd
  upload local index.htm as remote name /index.htm (needs leading "/")
  curl -X POST -F "a=@index.htm; filename=/index.htm" http://192.168.48.181/edit > /dev/null
  curl -X DELETE --data "a=/dummy.fil" http://192.168.48.181/edit
  curl -X PUT --data "a=/newfolder" http://192.168.48.181/edit > /dev/null
  "a" is any argument name, could be data=, arg=, file= anything
  list directory
  http://192.168.48.181/list?dir=/
*/

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SD_MMC.h>
#include "sdcard.h"

#define DBG_OUTPUT_PORT Serial

//const char* ssid = "ra";
//const char* password = "GigabyteBrix";
//const char* host = "esp32sd";

boolean connectioWasAlive = false;
WiFiMulti wifiMulti;
WebServer server(80);

#define hasSD card_is_mounted
File uploadFile;


void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

bool loadFromSdCard(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) {
    path += "index.htm";
  }

  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".htm")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".wav")) {
    dataType = "audio/wav";
  } else if (path.endsWith(".xml")) {
    dataType = "text/xml";
  } else if (path.endsWith(".kml")) {
    dataType = "application/kml";
  } else if (path.endsWith(".pdf")) {
    dataType = "application/pdf";
  } else if (path.endsWith(".zip")) {
    dataType = "application/zip";
  }

  File dataFile = SD_MMC.open(path.c_str());
  if (dataFile.isDirectory()) {
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD_MMC.open(path.c_str());
  }

  if (!dataFile) {
    return false;
  }

  if (server.hasArg("download")) {
    dataType = "application/octet-stream";
  }

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    DBG_OUTPUT_PORT.println("Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

void handleFileUpload() {
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    DBG_OUTPUT_PORT.print("Upload: START, filename: "); DBG_OUTPUT_PORT.println(upload.filename);
    if (SD_MMC.exists(upload.filename.c_str())) {
      SD_MMC.remove(upload.filename.c_str());
      DBG_OUTPUT_PORT.print("Delete: "); DBG_OUTPUT_PORT.println(upload.filename);
    }
    uploadFile = SD_MMC.open(upload.filename.c_str(), FILE_WRITE);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
    DBG_OUTPUT_PORT.print("Upload: WRITE, Bytes: "); DBG_OUTPUT_PORT.println(upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
    DBG_OUTPUT_PORT.print("Upload: END, Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void deleteRecursive(String path) {
  File file = SD_MMC.open(path.c_str());
  if (!file.isDirectory()) {
    file.close();
    SD_MMC.remove(path.c_str());
    return;
  }

  file.rewindDirectory();
  while (true) {
    File entry = file.openNextFile();
    if (!entry) {
      break;
    }
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory()) {
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SD_MMC.remove(entryPath.c_str());
    }
    yield();
  }
  DBG_OUTPUT_PORT.print("rmdir: "); DBG_OUTPUT_PORT.println(path);
  SD_MMC.rmdir(path.c_str());
  file.close();
}

void handleDelete() {
  if (server.args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  DBG_OUTPUT_PORT.print("Delete: "); DBG_OUTPUT_PORT.println(path);
  if (path == "/" || !SD_MMC.exists(path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate() {
  if (server.args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  DBG_OUTPUT_PORT.print("Create: "); DBG_OUTPUT_PORT.println(path);
  if (path == "/" || SD_MMC.exists(path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if (path.indexOf('.') > 0) {
    File file = SD_MMC.open(path.c_str(), FILE_WRITE);
    if (file) {
      file.write(0);
      file.close();
    }
  } else {
    SD_MMC.mkdir(path.c_str());
  }
  returnOK();
}

void printDirectory() {
  if (!server.hasArg("dir")) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg("dir");
  if (path != "/" && !SD_MMC.exists(path.c_str())) {
    return returnFail("BAD PATH");
  }
  File dir = SD_MMC.open(path.c_str());
  if (!dir.isDirectory()) {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  SD_status();
  String output;
  output  = "{\"free_bytes\":";
  output += (float)free_bytes; // FIXME free_bytes are uint64_t
  output += ",\"dir\":[";
  server.sendContent(output);

  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }
    if(cnt == 0)
      output = "";
    else
      output = ",";

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    if(String(entry.name()).startsWith("/"))
      output += entry.name(); // assumed dir path included
    else // dir path should be prepended
    {
      output += path;
      if(!path.endsWith("/"))
        output += "/";
      output += entry.name();
    }
    output += "\",\"size\":";
    output += entry.size();
    output += "}";
    server.sendContent(output);
    entry.close();
  }
  server.sendContent("]}");
  dir.close();
}

void handleNotFound() {
  if (hasSD != 0 && loadFromSdCard(server.uri())) {
    return;
  }
  String message = "SDCARD Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  DBG_OUTPUT_PORT.print(message);
}

// call this from loop() repeatedly
int monitorWiFi()
{
  if(WiFi.status() == WL_CONNECTED)
  {
    if(connectioWasAlive == false)
    {
      connectioWasAlive = true;
      DBG_OUTPUT_PORT.print("Connected to \"");
      DBG_OUTPUT_PORT.print(WiFi.SSID().c_str());
      DBG_OUTPUT_PORT.print("\" as IP: ");
      DBG_OUTPUT_PORT.println(WiFi.localIP());
    }
    return 1;
  }
  else // WiFi.status() != WL_CONNECTED
  {
    if(connectioWasAlive == true)
    {
      connectioWasAlive = false;
      DBG_OUTPUT_PORT.println("Disconnected, trying to reconnect");
    }
    // on arduino esp32 v2.0.2 if wifiMulti.run()
    // is called when already connected, web will stop working
    wifiMulti.run();
  }
  return 0;
}

// how to handle hasSD?
// by some shared global var maybe
void web_setup(void) {
  //DBG_OUTPUT_PORT.begin(115200);
  //DBG_OUTPUT_PORT.setDebugOutput(true);
  //DBG_OUTPUT_PORT.print("\n");

  for(int i = 0; i < AP_MAX; i++)
    if(AP_PASS[i].length() > 2)
    {
      int delimpos = AP_PASS[i].indexOf(':');
      if(delimpos > 1)
      {
        String ap_name = AP_PASS[i].substring(0, delimpos);
        String ap_pass = AP_PASS[i].substring(delimpos+1);
        #if 0
        DBG_OUTPUT_PORT.print("ap_name: \"");
        DBG_OUTPUT_PORT.print(ap_name);
        DBG_OUTPUT_PORT.print("\" ap_pass: \"");
        DBG_OUTPUT_PORT.print(ap_pass);
        DBG_OUTPUT_PORT.println("\"");
        #endif
        wifiMulti.addAP(ap_name.c_str(), ap_pass.c_str());
      }
    }
  monitorWiFi();

  if (MDNS.begin(DNS_HOST.c_str())) {
    MDNS.addService("http", "tcp", 80);
    #if 0
    DBG_OUTPUT_PORT.println("MDNS responder started");
    DBG_OUTPUT_PORT.print("You can now connect to http://");
    DBG_OUTPUT_PORT.print(DNS_HOST.c_str());
    DBG_OUTPUT_PORT.println(".local");
    #endif
  }

  server.on("/list", HTTP_GET, printDirectory);
  server.on("/edit", HTTP_DELETE, handleDelete);
  server.on("/edit", HTTP_PUT, handleCreate);
  server.on("/edit", HTTP_POST, []() {
    returnOK();
  }, handleFileUpload);
  server.onNotFound(handleNotFound);

  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");
}
