#include "FileSystem.h"

fs::LittleFSFS StaticFS = fs::LittleFSFS();
fs::LittleFSFS StorageFS = fs::LittleFSFS();

bool FileSystem::begin()
{
    Serial.println("Mounting LittleFS...");
    if (!StaticFS.begin(true, MOUNT_STATIC, 5, MOUNT_NAME_STATIC))
    {
        Serial.println("LittleFS Static Data Mount Failed");
        return false;
    }
    else
    {
        Serial.println("LittleFS Static Data Mounted Successfully");
    }

    delay(500); // Give OLED/Sensor time to wake up

    if (!StorageFS.begin(true, MOUNT_USER, 5, MOUNT_NAME_USER))
    {
        Serial.println("LittleFS Persistent Mount Failed");
        return false;
    }
    else
    {
        Serial.println("LittleFS Persistent Mounted Successfully");
    }

    return true;
}