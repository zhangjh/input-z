/**
 * TSF 类型定义
 * Task 1.1: Windows 平台目录结构
 * 
 * 定义 TSF 相关的数据结构和类型别名
 */

#ifndef SUYAN_WINDOWS_TSF_TYPES_H
#define SUYAN_WINDOWS_TSF_TYPES_H

#ifdef _WIN32

// Qt 头文件必须在 Windows 头文件之前包含
// 因为 Windows 头文件定义了 Bool 宏，与 Qt 的 qmetatype.h 冲突
#include <QtCore/qglobal.h>

#include <windows.h>
#include <msctf.h>
#include <string>

namespace suyan {

/**
 * TSF 编辑会话 Cookie
 * 用于在编辑会话中访问文档
 */
using EditCookie = TfEditCookie;

/**
 * TSF 客户端 ID
 * 标识输入法实例
 */
using ClientId = TfClientId;

/**
 * 文本范围信息
 */
struct TextRange {
    LONG start;     // 起始位置
    LONG end;       // 结束位置
};

/**
 * 光标矩形信息
 */
struct CaretRect {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
    
    int x() const { return static_cast<int>(left); }
    int y() const { return static_cast<int>(top); }
    int width() const { return static_cast<int>(right - left); }
    int height() const { return static_cast<int>(bottom - top); }
};

/**
 * 显示属性
 * 用于设置 preedit 文本的显示样式
 */
struct DisplayAttribute {
    TF_DISPLAYATTRIBUTE attr;
    GUID guid;
};

} // namespace suyan

#endif // _WIN32

#endif // SUYAN_WINDOWS_TSF_TYPES_H
