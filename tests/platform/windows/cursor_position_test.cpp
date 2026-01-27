/**
 * 光标位置获取测试
 * 
 * 测试各种获取光标位置的方法在不同场景下的表现
 */

#ifdef _WIN32

#include <windows.h>
#include <msctf.h>
#include <imm.h>
#include <iostream>
#include <string>

#pragma comment(lib, "imm32.lib")

// 测试 GetGUIThreadInfo
void testGetGUIThreadInfo() {
    std::cout << "\n=== Test GetGUIThreadInfo ===" << std::endl;
    
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        std::cout << "  No foreground window" << std::endl;
        return;
    }
    
    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);
    std::wcout << L"  Foreground window: " << title << std::endl;
    
    DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
    std::cout << "  Thread ID: " << threadId << std::endl;
    
    GUITHREADINFO gti;
    gti.cbSize = sizeof(GUITHREADINFO);
    
    if (GetGUIThreadInfo(threadId, &gti)) {
        std::cout << "  hwndActive: " << gti.hwndActive << std::endl;
        std::cout << "  hwndFocus: " << gti.hwndFocus << std::endl;
        std::cout << "  hwndCapture: " << gti.hwndCapture << std::endl;
        std::cout << "  hwndCaret: " << gti.hwndCaret << std::endl;
        std::cout << "  rcCaret: (" << gti.rcCaret.left << ", " << gti.rcCaret.top 
                  << ", " << gti.rcCaret.right << ", " << gti.rcCaret.bottom << ")" << std::endl;
        
        RECT& rc = gti.rcCaret;
        if (rc.right > rc.left || rc.bottom > rc.top) {
            HWND caretWnd = gti.hwndCaret ? gti.hwndCaret : (gti.hwndFocus ? gti.hwndFocus : hwnd);
            POINT pt = { rc.left, rc.bottom };
            ClientToScreen(caretWnd, &pt);
            std::cout << "  Screen position: (" << pt.x << ", " << pt.y << ")" << std::endl;
        } else {
            std::cout << "  Caret rect is empty!" << std::endl;
        }
    } else {
        std::cout << "  GetGUIThreadInfo failed: " << GetLastError() << std::endl;
    }
}

// 测试 GetCaretPos (只在拥有光标的线程中有效)
void testGetCaretPos() {
    std::cout << "\n=== Test GetCaretPos ===" << std::endl;
    
    POINT caretPos;
    if (GetCaretPos(&caretPos)) {
        std::cout << "  Caret pos: (" << caretPos.x << ", " << caretPos.y << ")" << std::endl;
        
        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            ClientToScreen(hwnd, &caretPos);
            std::cout << "  Screen pos: (" << caretPos.x << ", " << caretPos.y << ")" << std::endl;
        }
    } else {
        std::cout << "  GetCaretPos failed (expected if not in caret owner thread)" << std::endl;
    }
}

// 测试 ImmGetCompositionWindow
void testImmGetCompositionWindow() {
    std::cout << "\n=== Test ImmGetCompositionWindow ===" << std::endl;
    
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        std::cout << "  No foreground window" << std::endl;
        return;
    }
    
    // 获取焦点窗口
    DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
    GUITHREADINFO gti;
    gti.cbSize = sizeof(GUITHREADINFO);
    HWND focusWnd = hwnd;
    if (GetGUIThreadInfo(threadId, &gti) && gti.hwndFocus) {
        focusWnd = gti.hwndFocus;
    }
    
    std::cout << "  Testing on foreground window..." << std::endl;
    HIMC hIMC = ImmGetContext(hwnd);
    if (hIMC) {
        COMPOSITIONFORM cf = {};
        if (ImmGetCompositionWindow(hIMC, &cf)) {
            std::cout << "  Style: " << cf.dwStyle << std::endl;
            std::cout << "  ptCurrentPos: (" << cf.ptCurrentPos.x << ", " << cf.ptCurrentPos.y << ")" << std::endl;
            std::cout << "  rcArea: (" << cf.rcArea.left << ", " << cf.rcArea.top 
                      << ", " << cf.rcArea.right << ", " << cf.rcArea.bottom << ")" << std::endl;
            
            if (cf.ptCurrentPos.x != 0 || cf.ptCurrentPos.y != 0) {
                POINT pt = cf.ptCurrentPos;
                ClientToScreen(hwnd, &pt);
                std::cout << "  Screen position: (" << pt.x << ", " << pt.y << ")" << std::endl;
            }
        } else {
            std::cout << "  ImmGetCompositionWindow failed" << std::endl;
        }
        
        CANDIDATEFORM cdf = {};
        if (ImmGetCandidateWindow(hIMC, 0, &cdf)) {
            std::cout << "  CandidateForm Style: " << cdf.dwStyle << std::endl;
            std::cout << "  CandidateForm ptCurrentPos: (" << cdf.ptCurrentPos.x << ", " << cdf.ptCurrentPos.y << ")" << std::endl;
        } else {
            std::cout << "  ImmGetCandidateWindow failed" << std::endl;
        }
        
        ImmReleaseContext(hwnd, hIMC);
    } else {
        std::cout << "  ImmGetContext failed on foreground window" << std::endl;
    }
    
    if (focusWnd != hwnd) {
        std::cout << "  Testing on focus window..." << std::endl;
        hIMC = ImmGetContext(focusWnd);
        if (hIMC) {
            COMPOSITIONFORM cf = {};
            if (ImmGetCompositionWindow(hIMC, &cf)) {
                std::cout << "  Style: " << cf.dwStyle << std::endl;
                std::cout << "  ptCurrentPos: (" << cf.ptCurrentPos.x << ", " << cf.ptCurrentPos.y << ")" << std::endl;
                
                if (cf.ptCurrentPos.x != 0 || cf.ptCurrentPos.y != 0) {
                    POINT pt = cf.ptCurrentPos;
                    ClientToScreen(focusWnd, &pt);
                    std::cout << "  Screen position: (" << pt.x << ", " << pt.y << ")" << std::endl;
                }
            } else {
                std::cout << "  ImmGetCompositionWindow failed on focus window" << std::endl;
            }
            ImmReleaseContext(focusWnd, hIMC);
        } else {
            std::cout << "  ImmGetContext failed on focus window" << std::endl;
        }
    }
}

int main() {
    std::cout << "Cursor Position Test" << std::endl;
    std::cout << "====================" << std::endl;
    std::cout << "\nUsage:" << std::endl;
    std::cout << "  1. Press Enter" << std::endl;
    std::cout << "  2. You have 3 seconds to switch to another app and click in a text field" << std::endl;
    std::cout << "  3. The test will run and show results" << std::endl;
    std::cout << "\nPress Enter to start..." << std::endl;
    
    while (true) {
        std::cin.get();
        
        std::cout << "\n3..." << std::flush;
        Sleep(1000);
        std::cout << " 2..." << std::flush;
        Sleep(1000);
        std::cout << " 1..." << std::flush;
        Sleep(1000);
        std::cout << " Testing!\n" << std::endl;
        
        testGetGUIThreadInfo();
        testGetCaretPos();
        testImmGetCompositionWindow();
        
        std::cout << "\nPress Enter to test again, Ctrl+C to exit..." << std::endl;
    }
    
    return 0;
}

#else
int main() {
    std::cout << "This test is Windows-only" << std::endl;
    return 0;
}
#endif
