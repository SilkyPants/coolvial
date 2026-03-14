#pragma once
#include <LittleFS.h>

#define MOUNT_NAME_STATIC "static"
#define MOUNT_NAME_USER "storage"

#define MOUNT_STATIC "/" MOUNT_NAME_STATIC
#define MOUNT_USER "/" MOUNT_NAME_USER

#define MOUNT_STATIC_ROOT MOUNT_STATIC "/"
#define MOUNT_USER_ROOT MOUNT_USER "/"

extern fs::LittleFSFS StaticFS;
extern fs::LittleFSFS StorageFS;
class FileSystem {
public:
    static bool begin();
};