#pragma once
#include <cstdint>
#include <string>

namespace fusionmcp::os_input {
bool Click(std::int32_t x,std::int32_t y,const std::string& button,std::string& error);
bool PressKey(const std::string& name,std::uint16_t& virtualKey,std::string& error);
void ReleaseKey(std::uint16_t virtualKey);
}
