#pragma once
#include <stdint.h>

class SerialPort {
public:
    static void Init();
    static void WriteChar(char a);
    static void WriteString(const char* str);
};
