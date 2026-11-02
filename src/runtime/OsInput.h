#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace fusionmcp::os_input {
bool Click(std::int32_t x,std::int32_t y,const std::string& button,std::string& error);
bool ResolveKey(const std::string& name,std::uint16_t& virtualKey);
bool PressKey(std::uint16_t virtualKey,std::string& error);
void ReleaseKey(std::uint16_t virtualKey);
bool SplitText(const std::string& utf8,std::size_t maxCharacters,std::vector<std::wstring>& characters,std::string& error);
bool CheckGameWindow(std::string& error);
bool TypeCharacter(const std::wstring& character,std::string& error);
}
