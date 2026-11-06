# DisplayBrightness

[English](README.md)

通过 macOS 显示参数和 DDC/CI VCP 0x10 为外接显示器提供亮度接口的实验性 Lilu 插件。

**当前版本为 1.0.0 测试版，改为首次本地登录完成后启动 DDC，取消固定 30 秒等待。** 本地测试与构建检查通过，尚未实机启动；登录事件时序及启动性能仍待验证。文中指定的 [0.0.32 运行样本](docs/DEVELOPMENT.md#observed-runtime-sample)已观察到缓存提前恢复，但不代表新调度方式已经通过验证。

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
- 有有效系统偏好记录时，在显示器注册前使用该设备上次经过硬件验证的亮度声明接口。没有固定初始百分比，也没有型号白名单。
- 读取硬件成功或写后读回验证成功后，将当前值、硬件最大值和完整 EDID 基础块打包为一条带版本的记录，通过 macOS 自带 IOGraphics 偏好机制按显示器保存。
- 没有有效记录时，登录后读取硬件学习。**首次学习启动可能没有原生滑块；记录成功保留后，下一次启动才可验证提前发现。**
- 监听内核控制台会话通知，订阅后立即检查当前状态。确认首次本地登录完成且目标显示器已注册后，才安排 DDC；没有到时强行放行，也不定时轮询登录状态。
- 后台一次安排一台显示器探测，任务间归还执行机会。每台屏保留独立的有限重试窗口，等待登录不消耗探测预算。首次有效读取替换历史值后才开放调节，此前请求全部丢弃，缓存值不会进入写入队列。
- 每次写入前重新检查设备身份、连接拓扑及硬件范围，写入后读回验证。
- 保留已有原生亮度接口。记录损坏、不匹配或偏好机制不可用时，退回硬件学习，不填充假值。

登录回调只唤醒预先分配的任务，不等待硬件。低优先级 DDC 仍会获取图形锁，异步执行并不等于启动零开销；登录完成也不能证明显卡就绪或中断正在执行的 AMD 同步调用。缓存是历史值，显示器菜单或其他系统修改过亮度后可能与硬件不同。运行时不需要助手程序或 LaunchDaemon。启动验证步骤见[排查说明](docs/TROUBLESHOOTING.md#verify-login-gating-and-startup-performance)。

## 从源码构建

本仓库提供源码。需要 Xcode 命令行工具、包含开发头文件的 [Lilu DEBUG kext](https://github.com/acidanthera/Lilu/releases) 和 [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK)。

在仓库根目录执行：

```sh
python3 driver/test.py
python3 driver/build.py --lilu /path/to/Lilu.kext --sdk /path/to/MacKernelSDK
```

产物位于 `build/1.0.0/`，包括 kext、配套 dSYM、`DisplayBrightness-1.0.0-DEBUG.zip` 和构建清单。ZIP 同时包含中英文 README、设计与排查文档。构建脚本不会安装或加载驱动，签名、UUID 和校验和的核对方法见[产物校验](docs/DEVELOPMENT.md#build-artifacts)。

## 安装与回退

1. 备份实际启动 EFI，并保留可用的恢复启动方式。
2. 将 `DisplayBrightness.kext` 放入 `EFI/OC/Kexts`。
3. 在 OpenCore `Kernel -> Add` 中启用，放在 Lilu 之后。已有安装替换同名 kext 即可，无需增加本插件专用配置。
4. 若显示器菜单提供 DDC/CI 开关，将其启用，再重启验证。

回退时恢复旧 kext 或禁用对应条目；在现有启动参数中追加 `-dboff` 也可在重启后禁用。保留其他启动参数。

### 升级到 1.0.0

| 原版本 | 升级后的预期行为 |
| --- | --- |
| 0.0.31 及以前 | 尚无此功能保存的硬件历史记录；完成本地登录后学习实际亮度，原生滑块可能要到下次启动才出现。 |
| 0.0.32 | 继续使用有效的硬件历史记录；DDC 从固定等待 30 秒改为首次登录完成且显示器已注册后启动。 |
| 0.0.33 | 调整版本编号，沿用相同的登录门控和系统偏好记录格式。 |

替换已有 kext 后重启并核对加载版本即可，无需清空缓存、添加新启动参数或安装登录助手。登录前可以已有缓存声明的接口，但硬件读写仍受登录门控限制。

没有滑块时，先检查版本与 `DisplayBrightnessEarlyDeclarationFirstReason`、`DisplayBrightnessInterfaceState`，再检查系统亮度 API。已加载或 DDC 读取成功，并不等于原生入口可用。完整只读命令见[排查说明](docs/TROUBLESHOOTING.md)。

## 开发与验证状态

目前通过 **29 项 C++ 测试、2 项源码检查**，以及编译、链接、ad-hoc 签名和调试符号匹配检查。这些结果不等于实机启动、启动性能、跨重启持久化或显示控制验证。

- [驱动设计与测试范围](docs/DEVELOPMENT.md)
- [版本记录](CHANGELOG.md)
- [提交可复现的问题](https://github.com/FateLightX/DisplayBrightness/issues)

仓库保留 [Lilu 许可声明](LICENSE-Lilu.txt)。感谢 [Lilu 贡献者](https://github.com/acidanthera/Lilu/graphs/contributors)和 [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK)。
