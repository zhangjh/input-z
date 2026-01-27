# librime 预编译库

## 版本信息

- 版本: 1.16.1
- 来源: https://github.com/rime/librime/releases/tag/1.16.1

## 目录结构

```
deps/librime/
├── include/          # 头文件
│   ├── rime_api.h
│   ├── rime_api_deprecated.h
│   ├── rime_api_stdbool.h
│   └── rime_levers_api.h
├── lib/
│   ├── x64/          # 64位库
│   │   ├── rime.dll
│   │   └── rime.lib
│   └── x86/          # 32位库
│       ├── rime.dll
│       └── rime.lib
└── README.md
```

## 下载方法

运行脚本自动下载32位和64位库：

```powershell
.\scripts\get-rime-windows.ps1
```

## 使用方式

CMake 会根据目标架构自动选择正确的库目录。
