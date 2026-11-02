#include <Windows.h>
#include <algorithm>
#include <cctype>
#include "runtime/OsInput.h"

namespace fusionmcp::os_input {
namespace {
struct NamedKey {const char* name;std::uint16_t virtualKey;};
constexpr NamedKey kNamedKeys[]={{"enter",VK_RETURN},{"escape",VK_ESCAPE},{"space",VK_SPACE},{"tab",VK_TAB},{"up",VK_UP},
    {"down",VK_DOWN},{"left",VK_LEFT},{"right",VK_RIGHT},{"shift",VK_SHIFT},{"ctrl",VK_CONTROL},{"alt",VK_MENU}};

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

HWND GameWindow(std::string& error) {
    const auto window=GetForegroundWindow();
    DWORD pid=0;
    if (!window||!GetWindowThreadProcessId(window,&pid)) {error="There is no foreground window";return nullptr;}
    const auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if (!process) {error="The foreground process cannot be opened";return nullptr;}
    std::wstring image(MAX_PATH*4,L'\0');
    DWORD size=static_cast<DWORD>(image.size());
    const bool queried=QueryFullProcessImageNameW(process,0,image.data(),&size)!=0;
    CloseHandle(process);
    if (!queried) {error="The foreground process image cannot be read";return nullptr;}
    image.resize(size);
    const auto slash=image.find_last_of(L'\\');
    const auto name=slash==std::wstring::npos?image:image.substr(slash+1);
    if (CompareStringOrdinal(name.c_str(),-1,L"Fallout4.exe",-1,TRUE)!=CSTR_EQUAL) {
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

bool PressKey(const std::string& name,std::uint16_t& virtualKey,std::string& error) {
    if (!ResolveKey(name,virtualKey)) {
        error="Unknown key name; use a letter, digit, F1..F24, Enter, Escape, Space, Tab, Up, Down, Left, Right, Shift, Ctrl or Alt";
        return false;
    }
    if (!GameWindow(error)) return false;
    return SendKey(virtualKey,0,error);
}

void ReleaseKey(std::uint16_t virtualKey) {
    std::string ignored;
    SendKey(virtualKey,KEYEVENTF_KEYUP,ignored);
}
}
