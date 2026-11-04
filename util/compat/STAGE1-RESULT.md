# 第一阶段结果：通过拒绝提示检查

本轮基于 `Dev` 的 `4b870f6d0457eeab9c7a8f4708ae693bb62a209b`（1.48），
应用 `stage1-identity.patch` 构建。源码提交与兼容补丁是两个独立标识，不能把源提交
直接当成产物哈希。第二、三阶段尚未验收。

## 构建与回归

- `Development|x64`、`Development|x86`：通过。
- 两架构核心：各 74 个测试通过；GUI 单元测试与 Python 绑定一致性检查：通过。
- D3D11 opt-out、DXGI 工厂选项、覆盖层解析器、Windows 启动契约：通过。
- 公开头文件／Python 接口未改，48 个内部着色器函数及对应查询检查通过。
- 部署版独立回放已有原神 D3D11 RDC：986 actions，461 textures，最终画面可见场景、人物、HUD。
- 部署版独立回放已有 Returnal D3D12 RDC：2422 actions，779 textures，成功导出最终 backbuffer。
- `C:\RenderDocCustom` 两架构 GUI／DLL 哈希在本轮部署前后相同。

修复了迁移中发现的两组依赖：Vulkan x86 stdcall 导出别名须与定义／查询同步；
DLL 产品名改变后仍须接受公开 `renderdoc__replay__marker`，不能改坏公共回放接口。

部署到 `C:\RenderDocCompat`，88 个部署文件逐项核对 SHA256。

| 内容 | SHA256 |
|---|---|
| 身份补丁 | `ad2ba840a61d051208b0bf9fcd539664b7a864a4898c49631ab0c78f45779590` |
| x64 DLL | `d6a0b8c7b126bdbb1285ce73400deb84d47fa3cd2d72c249ec8be92cfb8f098d` |
| x64 GUI | `0cbd68a1359be6dceec00e083636182f106ea2e4ec21f3eb4aba19aa16a2e8c9` |
| x86 DLL | `c42f17b945fc06b9ff94e31588c7fde19204ee4b99276d80068ccc89241edd5a` |
| x86 GUI | `2265fd1136324c3b4745e64faded8c81f2666f9a336f5efff59a16b6e1d5a25c` |

## 鸣潮实际启动

2026-11-05 本机测试（启动记录 UTC `2026-11-04T20:09:44Z`）：

- 使用新部署的 `rendertestcmd.exe`，清除该子进程继承的专用 `RENDERDOC_*`／`RENDERTEST_*` 环境变量。
- 启动根目录 `Wuthering Waves.exe`，安装根目录作工作目录，开启子进程捕获。
- 本阶段没有加入第二阶段入口修复，也没有预设 `-dx11 -krqlv=uhd` 参数。
- 根进程 PID **59860**、实际 `Client-Win64-Shipping.exe` PID **63708**，
  均通过进程模块枚举确认加载 **`C:\RenderDocCompat\rendertest.dll`**。
- 官方 `launcher_main.exe` 已运行，但仍未满足游戏要求的启动上下文。
- 最终窗口标题：`The Game has crashed and will close`。
- 完整错误正文：

```text
Fatal error: [File:Unknown] [Line: 54]
kuro: Use launcher to start game!
```

当前轮次未观察到 RenderDoc 拒绝，启动上下文仍未满足。
**按约定第一阶段通过，启动器问题转入第二阶段。** 不推导为正常启动、登录成功、
图形接口 Active 或角色场景截帧回放成功。

本地完整证据：`E:\RenderDocProject\work\compat-stage1`，包括构建日志、
`stage1-launch.json` 模块样本、CLI 输出、单元／契约检查结果、独立回放 JSON／PNG。
部署清单位于 `C:\RenderDocCompat\compat-build.json`。
