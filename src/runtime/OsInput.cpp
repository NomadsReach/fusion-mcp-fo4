#include <Windows.h>
#include <algorithm>
#include <cctype>
#include "runtime/OsInput.h"

namespace fusionmcp::os_input {
namespace {
struct NamedKey {const char* name;std::uint16_t virtualKey;};
constexpr NamedKey kNamedKeys[]={{"enter",VK_RETURN},{"escape",VK_ESCAPE},{"space",VK_SPACE},{"tab",VK_TAB},{"up",VK_UP},
    {"down",VK_DOWN},{"left",VK_LEFT},{"right",VK_RIGHT},{"shift",VK_SHIFT},{"ctrl",VK_CONTROL},{"alt",VK_MENU}};

HWND GameWindow(std::string& error) {
    const auto window=GetForegroundWindow();
    DWORD pid=0;
    if (!window||!GetWindowThreadProcessId(window,&pid)) {error="There is no foreground window";return nullptr;}
    if (pid!=GetCurrentProcessId()) {
        error="The foreground window is not Fallout 4; nothing was sent";
        return nullptr;
    }
    return window;
}

bool Inject(INPUT* inputs,UINT count,std::string& error) {
    if (SendInput(count,inputs,static_cast<int>(sizeof(INPUT)))==count) return true;
    error="SendInput was refused (UIPI or a locked input desktop)";
    return false;
}

bool SendKey(std::uint16_t virtualKey,DWORD flags,std::string& error) {
    INPUT input{};
    input.type=INPUT_KEYBOARD;
    input.ki.wVk=virtualKey;
    input.ki.dwFlags=flags;
    return Inject(&input,1,error);
}
}

bool ResolveKey(const std::string& name,std::uint16_t& virtualKey) {
    std::string key=name;
    for (char& c:key) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (key.size()==1&&std::isalnum(static_cast<unsigned char>(key[0]))) {
        virtualKey=static_cast<std::uint16_t>(std::toupper(static_cast<unsigned char>(key[0])));
        return true;
    }
    if (key.size()>1&&key.size()<=3&&key[0]=='f'&&std::all_of(key.begin()+1,key.end(),[](char c){return c>='0'&&c<='9';})) {
        const int number=std::stoi(key.substr(1));
        if (number<1||number>24) return false;
        virtualKey=static_cast<std::uint16_t>(VK_F1+number-1);
        return true;
    }
    for (const auto& named:kNamedKeys) if (key==named.name) {virtualKey=named.virtualKey;return true;}
    return false;
}

bool Click(std::int32_t x,std::int32_t y,const std::string& button,std::string& error) {
    DWORD down=0,up=0;
    if (button=="left") {down=MOUSEEVENTF_LEFTDOWN;up=MOUSEEVENTF_LEFTUP;}
    else if (button=="right") {down=MOUSEEVENTF_RIGHTDOWN;up=MOUSEEVENTF_RIGHTUP;}
    else if (button=="middle") {down=MOUSEEVENTF_MIDDLEDOWN;up=MOUSEEVENTF_MIDDLEUP;}
    else {error="button must be left, right or middle";return false;}
    const auto window=GameWindow(error);
    if (!window) return false;
    RECT client{};
    if (!GetClientRect(window,&client)||x<0||y<0||x>=client.right||y>=client.bottom) {
        error="x and y must lie inside the game client area";
        return false;
    }
    POINT point{x,y};
    if (!ClientToScreen(window,&point)) {error="ClientToScreen failed";return false;}
    const auto left=GetSystemMetrics(SM_XVIRTUALSCREEN),top=GetSystemMetrics(SM_YVIRTUALSCREEN);
    const auto width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    INPUT inputs[3]{};
    inputs[0].type=INPUT_MOUSE;
    inputs[0].mi.dx=static_cast<LONG>((point.x-left)*65535LL/std::max(width-1,1));
    inputs[0].mi.dy=static_cast<LONG>((point.y-top)*65535LL/std::max(height-1,1));
    inputs[0].mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
    inputs[1].type=INPUT_MOUSE;
    inputs[1].mi.dwFlags=down;
    inputs[2].type=INPUT_MOUSE;
    inputs[2].mi.dwFlags=up;
    return Inject(inputs,3,error);
}

bool PressKey(std::uint16_t virtualKey,std::string& error) {
    if (!GameWindow(error)) return false;
    return SendKey(virtualKey,0,error);
}

void ReleaseKey(std::uint16_t virtualKey) {
    std::string ignored;
    SendKey(virtualKey,KEYEVENTF_KEYUP,ignored);
}

bool SplitText(const std::string& utf8,std::size_t maxCharacters,std::vector<std::wstring>& characters,std::string& error) {
    const auto limit="text must be 1 to "+std::to_string(maxCharacters)+" characters of valid UTF-8";
    const auto length=utf8.empty()?0:MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),static_cast<int>(utf8.size()),nullptr,0);
    if (length<=0) {error=limit;return false;}
    std::wstring wide(static_cast<std::size_t>(length),L'\0');
    if (MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),static_cast<int>(utf8.size()),wide.data(),length)!=length) {
        error=limit;
        return false;
    }
    characters.clear();
    for (std::size_t i=0;i<wide.size();) {
        if (wide[i]<0x20) {error="control characters and newlines are refused; send Enter with input.key";return false;}
        const bool pair=wide[i]>=0xD800&&wide[i]<=0xDBFF&&i+1<wide.size()&&wide[i+1]>=0xDC00&&wide[i+1]<=0xDFFF;
        const std::size_t width=pair?2:1;
        characters.push_back(wide.substr(i,width));
        i+=width;
    }
    if (characters.empty()||characters.size()>maxCharacters) {error=limit;return false;}
    return true;
}

bool CheckGameWindow(std::string& error) {
    return GameWindow(error)!=nullptr;
}

bool TypeCharacter(const std::wstring& character,std::string& error) {
    if (!GameWindow(error)) return false;
    INPUT inputs[4]{};
    UINT count=0;
    for (const wchar_t unit:character) {
        inputs[count].type=INPUT_KEYBOARD;
        inputs[count].ki.wScan=unit;
        inputs[count].ki.dwFlags=KEYEVENTF_UNICODE;
        inputs[count+1].type=INPUT_KEYBOARD;
        inputs[count+1].ki.wScan=unit;
        inputs[count+1].ki.dwFlags=KEYEVENTF_UNICODE|KEYEVENTF_KEYUP;
        count+=2;
    }
    return Inject(inputs,count,error);
}
}
