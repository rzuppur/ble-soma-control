#pragma once
#include <string>

namespace BLESomaControl::Internal {
    inline std::string& trimString(std::string& s)
    {
        s.erase(s.find_last_not_of(" \t\n\r\f\v") + 1);
        s.erase(0, s.find_first_not_of(" \t\n\r\f\v"));
        return s;
    }
}
