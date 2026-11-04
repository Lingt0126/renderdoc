# Windows 独立兼容构建

此目录把 `1601fb4` 的身份字符串、内部着色器及构建配套迁移至本项目 1.48。
标准源码、标准构建输出和 `C:\RenderDocCustom` 保留；兼容产物使用
`rendertest.dll`、`rendertestcmd.exe`、`qrendertest.exe` 和 `rendertestshim32/64.dll`。

## 构建方法

需要 Windows、Python 3.11+、Visual Studio 2022 C++ 工具及本项目原有 Qt/Python/PySide 依赖。
在仓库根目录执行（示例目录可调整，必须位于仓库外）：

```powershell
py -3 util/compat/build_compat.py --build-root E:\RenderDocProject\work\compat-stage1
```

默认依次构建 `Development|x64` 和 `Development|x86`。构建工具复制当前跟踪文件，
保留本地跟踪文件修改，并应用可审查的 `stage1-identity.patch`；不依赖参考仓库，
不重置工作区、不创建分支，也不复用标准构建的中间文件。
本地提供、被 Git 忽略的 PySide SDK 同样复制到独立源码目录。

补丁应用失败会停止，需针对新上游人工调整补丁。每次重新准备时先恢复原始输入，
不会对已改名的文件再次全局替换。`--reuse-source` 仅用于恢复同一份已准备的构建；
补丁哈希改变后必须重新准备。

独立源码、配置和缓存的隔离方式：

| 内容 | 位置／规则 |
|---|---|
| 源码 | `<build-root>\source` |
| 编译输出 | `source\x64\Development`、`source\Win32\Development` |
| 编译中间文件 | 各架构输出下的 `obj` |
| 构建日志 | `<build-root>\build-x64.log`、`build-x86.log` |
| 核心配置／缓存 | `%APPDATA%\rendertest` |
| GUI 配置 | Qt 使用独立的 `qrendertest` 应用名称 |
| 缺省日志／捕获 | `%TEMP%\RenderTest` |

`Directory.Build.props` 为 PCH 生产者和消费者统一设置源路径裁剪；运行时、GUI、
命令行及 shim 分别裁剪对应源码根目录。原许可证和版权声明保留。

## 保留的契约

- 公开 `RENDERDOC_*` 导出、`RENDERDOC_GetAPI` 查询及公开回放标记不改名。
- Python 继续 `import renderdoc`、`import qrenderdoc`，不改模块初始化入口。
- RDC 格式、内部 section 名和版本契约不变。
- 手动 `RENDERDOC_*` 环境设置保留，包括既有米哈游配置。
- 本补丁不引入第二阶段的 IAT、Native resolver、Streamline 或 Detours 修复。

`stage1-migration.json` 列出参考提交对应文件的迁移情况、内部着色器名称及特意保留的契约。
参考分支独有的兼容策略/NVAPI 功能没有顺带迁入。
Android/POSIX 中存在于本项目的字符串调整已迁移，但本工具只交付 Windows 构建，
不声称其他平台包已经构建验证。

## 检查和部署

```powershell
& util/compat/check_stage1.ps1 -BuildRoot E:\RenderDocProject\work\compat-stage1 `
  -ReplayCapture E:\RenderDocProject\captures\YuanShen_GUI_auto_scene_frame28585.rdc

py -3 util/compat/build_compat.py --build-root E:\RenderDocProject\work\compat-stage1 `
  --reuse-source --prepare-only --deploy
```

检查覆盖两架构核心/GUI 单元测试、Python 绑定一致性、D3D11 输出契约、DXGI 工厂选项、
启动及覆盖层解析器契约。可选的已有 RDC 回放在新进程中打开，导出最终 backbuffer PNG。
这些检查是兼容构建的基本回归，不能代替鸣潮角色场景的截帧验收。

部署只允许 `C:\RenderDocCompat`；更新前备份先前兼容部署，复制后核对文件 SHA256。
`compat-build.json` 记录源提交、未提交跟踪文件状态、补丁哈希和部署文件哈希。
测试中的具体 DLL 哈希必须与部署文件一致，不能只依据窗口标题判定版本。

## 三阶段验收边界

1. **拒绝提示检查**：新构建启动鸣潮后未观察到明确的 RenderDoc 拒绝提示。
   仅出现“请通过启动器启动”也通过此阶段，但只说明当前轮次没有观察到 RenderDoc 拒绝。
2. **启动及捕获**：解决官方启动器上下文，实际游戏进程 D3D11 Active，
   在角色场景产生本次请求的新 RDC，退出游戏后以独立进程回放场景、人物和 HUD。
3. **普通 GUI**：直接打开 `C:\RenderDocCompat\qrendertest.exe`，不预设兼容变量、
   不运行启动脚本、不加载 `.cap`，自动选择鸣潮兼容配置并完成同样的场景回放验收。

后两阶段只能在前一阶段有证据通过后继续。连接成功、历史捕获通知或启动画面都不等于
场景回放成功。DLSS 保持关闭，登录与人工画面确认由用户完成。
本轮不增加鸣潮 DX12 或 DLSS 回放支持。
