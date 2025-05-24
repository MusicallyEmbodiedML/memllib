#include <Arduino.h>
#include <VFS.h>
#include <LittleFS.h>

namespace FlashFS {

    void begin()
    {
        LittleFS.begin();
        VFS.root(LittleFS);
        Serial.println("LittleFS initialized.");
    }
}
