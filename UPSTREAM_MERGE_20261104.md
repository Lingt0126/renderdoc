# 2026-11-04 官方上游完整合并

当前 `Dev` 已完整合入官方 `baldurk/renderdoc:v1.x`，并更新本机运行副本。
用户已反馈合并后版本“人工验证基本通过”；本记录分别列出自动检查结果及用户验收反馈。

## 历史与范围

| 项目 | 提交 |
| --- | --- |
| 合并前 Dev／第一父提交 | `fa6c2e21ecac3ef1bb49b2f7228321ba031e87e6` |
| 官方上游／第二父提交 | `34fbfb4352d65a1fb609afba356aac03af1786a3` |
| 共同祖先 | `83de1c287a13c464db98efcc66d2fcf43c7fa324` |
| 合并提交／本轮二进制构建来源 | `f49a31ca16b9d01c3b020cf9ce4790f6a5b1df98` |

在原 `Dev` 上执行普通 `--no-ff` 合并，没有新建分支、rebase、squash 或重写历史。
官方 145 个新增提交和本分支 13 个定制提交均保留；205 个上游修改文件自动合并，无文本冲突。
已检查合并前 Dev 和官方目标提交均为合并结果的祖先。

主要更新包括 D3D12 ExecuteIndirect 空顶点／索引缓冲和绑定状态修复、着色器字节码与反射处理、
DXBC/DXIL 调试、Python 回调生命周期及捕获访问线程修复、像素着色器调试入口、常量缓冲筛选、
Vulkan 扩展及其他平台修复。上游版本号升为 1.48；这是开发版本。
[完整上游差异](https://github.com/baldurk/renderdoc/compare/83de1c287a13c464db98efcc66d2fcf43c7fa324...34fbfb4352d65a1fb609afba356aac03af1786a3)。

保留普通 GUI 的自动兼容解析、米哈游 D3D11 选项、标准 Windows／Steam 启动和 EOS 修复，
以及 D3D12 上传快照、捕获顺序和管线库定制。对应启动策略、Win32、DXGI 和 D3D12 命令队列
文件与合并前一致；重叠的 D3D12 文件保留双方不同函数的修改。
未把本次更新解释为 DLSS 回放或鸣潮不兼容警告已解决。

原工作区三个 Win32 文件已逐一备份并确认原始 SHA256 不变；`.idea/` 和
`CONTROL_RESONANT_LOCAL_VALIDATION.md` 未加入提交。

## 自动验证

| 检查 | 结果 |
| --- | --- |
| 完整解决方案 Development x64／x86 构建 | 均通过，二进制版本为 v1.48 / f49a31c |
| x64 核心单元测试 | 74 个用例、21,763,255 项断言通过 |
| x86 核心单元测试 | 74 个用例、21,763,222 项断言通过 |
| x64／x86 GUI 单元测试 | 各 7 个用例、1530 项断言通过，包含 71 项启动策略／配置断言 |
| x64／x86 Python | 绑定一致性通过；compatibilityProfile 默认值及四个值的读写通过 |
| D3D11／DXGI | 48 项设备输出及捕获策略检查、6 项工厂检查通过 |
| Windows 启动 | ordinary、bootstrap、invalid-pid 结果及启动诊断检查通过 |
| EOS 解析器 | 两个 overlay 名称的共存检查通过 |
| D3D12 管线库 | 注入后的 Store／Release／Load 及参数契约检查通过 |
| 公开回放初始化 | 源构建和 C: 部署副本均返回 REPLAY_LOG_INITIALISED=1 |
| 部署自测 | x64／x86 启动策略 71 项断言及 Python 绑定一致性均通过 |
| 历史／文件保留 | 两个父分支的祖先检查、原工作区文件哈希检查通过 |

核心测试首轮有一个用例的两项断言失败：`os_specific.cpp` 无条件读取 `HOME`，
当前 Windows 环境没有此变量。复跑只在测试子进程中将 `HOME` 指向真实 Windows 用户目录，
没有修改父进程／系统环境、产品代码或测试断言，也没有过滤失败用例。
首轮失败日志和完整通过日志均保留，项目修复账本记录为 `E-20261104-009`。

本机证据目录为 `E:\RenderDocProject\work\upstream-merge-20261104`，包括
`build-x64.log`、`build-x86.log`、`core-unit-*-normalized-env.txt`、`gui-unit-*.txt`、
各契约测试输出、`deployed-*.txt` 及 `deployment-manifest.json`。
`run-core-tests.ps1 -Platform x64` 或 `-Platform Win32` 可在本机复现核心测试环境。
大型运行副本和测试产物不入库。

## 部署与使用

直接打开 `C:\RenderDocCustom\qrenderdoc.exe`，不需 CMD、`.cap` 或系统兼容变量。
在 Launch Application 选择游戏 EXE，保持 **Compatibility Mode = Automatic detection**，
检查摘要后点击 **Launch**。具体选取规则和手动覆盖见
[普通 GUI 使用说明](docs/how/gui_launch_compatibility.rst)。

本次通过现有 `deploy_custom_runtime.ps1` 部署，十个关键文件与构建 SHA256 一致。
每种架构的 `renderdoc.dll` 与 `rdoc.dll` 字节相同；两个文件均须保留。

| 文件 | SHA256 |
| --- | --- |
| x64 qrenderdoc.exe | `9E9F0D8A19B41F6EFEEC610DB6D7EDB9999124E2D0B528726137CCCE7B87D2A6` |
| x64 renderdoc.dll / rdoc.dll | `1905967F5E50ECF4FDFC01FCDA18ADEB626FDF75C588F7EB356EC801FEE5370C` |
| x86 qrenderdoc.exe | `182B18E8D75B280E50815E35997B7BEE88AD26087D5E3CC4A6B2E52F827317D7` |
| x86 renderdoc.dll / rdoc.dll | `F8D58C835656060465A0238042B46A15E914D63D0273C681787B47855EFAA425` |

部署前的完整运行目录备份在
`E:\RenderDocProject\work\upstream-merge-20261104\runtime-before`。
如需恢复，先正常关闭占用运行副本的程序并禁用 Global Hook，再从备份恢复对应文件；
无需重写或回退 Git 历史。

## 用户人工验收（反馈基本通过）

用户在部署后反馈：“人工验证基本通过”，并要求提交推送。
该结论按用户的总体反馈记录；本次未补充逐游戏结果、图像设置细项或新的 RDC／截图。
现有 DLSS 限制保持原记录。后续如需复核，沿用以下清单：

1. 直接打开上述 C: GUI，确认版本为 **1.48 / f49a31c**。
2. 分别选择原神、星穹铁道、绝区零和 Returnal 的 EXE，保持自动识别，检查实际程序与兼容摘要。
3. 登录并进入可操作场景，确认图形 API Active 后截帧并保存 RDC。
4. 正常退出游戏，用独立 RenderDoc 进程重新打开新 RDC，核对场景、人物和 HUD。
5. 星穹铁道和 Returnal 保持关闭 DLSS；异常时记录游戏、设置、RDC 和对应日志。

此前构建的通过记录保留在 `CUSTOM_RUNTIME.md`，其证据仍对应原构建。
合并后版本的人工验收状态来自上述用户反馈；工具未另行执行游戏场景验证。
