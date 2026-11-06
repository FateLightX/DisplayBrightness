# DisplayBrightness

[English](README.md)

通过 macOS 显示参数和 DDC/CI VCP 0x10 为外接显示器提供亮度接口的实验性 Lilu 插件。

**当前版本为 0.0.31，已在一次本机启动中确认三台 DisplayPort 显示器提前声明成功、原生亮度 API 可用、硬件亮度状态一致。** 尚未直接确认界面滑块，也未完成多次冷启动、睡眠唤醒和热插拔验证。具体受测构建见[运行样本](docs/DEVELOPMENT.md#observed-runtime-sample)。

## 运行要求

| 项目 | 要求 |
| --- | --- |
| 架构 | x86_64 |
| 内核 | Darwin 25.6 |
| IOGraphics UUID | `74AA6483-8AB4-3A94-8143-5E306C16D7DA` |
| Lilu | 1.7.3 或更高 |
| 显示连接 | 可用的 IOFramebuffer I2C 路径和 DDC/CI VCP 0x10 |

内核版本和 IOGraphics 检查用于限制 ABI 兼容范围。其他系统构建、显卡、转接设备、睡眠唤醒和热插拔仍待验证。

## 工作方式

- 随 Lilu 自动启用，添加 `-dboff` 可禁用。
- 对三个已验证的 **EDID 基础块指纹**，在显示器注册前声明亮度接口。这些指纹来自 U2790B、U28P2G6B 和 U2701B；同型号的另一台设备不一定匹配。
- 初始化前 50% 仅作界面占位，所有亮度写入均被拒绝且不留待以后执行。
- 开机满 30 秒后才开始 DDC，探测窗口最长 60 秒。首次有效读取会替换占位值，随后开放正常调节。
- 每次写入前重新检查设备身份、连接拓扑及硬件范围，写入后读回验证。
- 其他有效外接显示器 EDID 仍可进入通用探测流程，读取成功后才发布接口。这种延迟发布后的原生滑块发现问题仍未解决。

30 秒是启动避让窗口，不代表显卡已经就绪，也不能中断正在执行的 AMD 同步调用。运行时不需要助手程序或 LaunchDaemon。

## 从源码构建

本仓库提供源码。需要 Xcode 命令行工具、包含开发头文件的 [Lilu DEBUG kext](https://github.com/acidanthera/Lilu/releases) 和 [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK)。

在仓库根目录执行：

```sh
python3 driver/test.py
python3 driver/build.py --lilu /path/to/Lilu.kext --sdk /path/to/MacKernelSDK
```

产物位于 `build/0.0.31/`，包括 kext、配套 dSYM、`DisplayBrightness-0.0.31-DEBUG.zip` 和构建清单。构建脚本不会安装或加载驱动。

## 安装与回退

1. 备份实际启动 EFI，并保留可用的恢复启动方式。
2. 将 `DisplayBrightness.kext` 放入 `EFI/OC/Kexts`。
3. 在 OpenCore `Kernel -> Add` 中启用，放在 Lilu 之后。已有安装替换同名 kext 即可，无需增加本插件专用配置。
4. 若显示器菜单提供 DDC/CI 开关，将其启用，再重启验证。

回退时恢复旧 kext 或禁用对应条目；在现有启动参数中追加 `-dboff` 也可在重启后禁用。保留其他启动参数。

没有滑块时，先检查版本与 `DisplayBrightnessEarlyDeclarationFirstReason`、`DisplayBrightnessInterfaceState`，再检查系统亮度 API。已加载或 DDC 读取成功，并不等于原生入口可用。完整只读命令见[排查说明](docs/TROUBLESHOOTING.md)。

## 开发与验证状态

目前通过 **25 项 C++ 测试、2 项源码检查**，以及编译、链接、ad-hoc 签名和调试符号匹配检查。这些结果不等于实机启动或显示控制验证。

- [驱动设计与测试范围](docs/DEVELOPMENT.md)
- [版本记录](CHANGELOG.md)
- [提交可复现的问题](https://github.com/FateLightX/DisplayBrightness/issues)

仓库保留 [Lilu 许可声明](LICENSE-Lilu.txt)。感谢 [Lilu 贡献者](https://github.com/acidanthera/Lilu/graphs/contributors)和 [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK)。
