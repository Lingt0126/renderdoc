Returnal Windows D3D12 捕获
=========================

启动方法
--------

本机游戏目录为 ``E:\SteamLibrary\steamapps\common\Returnal``，Steam AppID 为
``1649240``。先让 Steam 正常运行，再关闭已有的 Returnal 实例。

重新构建 x64/x86 Development 后执行仓库的 ``deploy_custom_runtime.ps1``，
更新 ``C:\RenderDocCustom``。关闭占用该目录的 RenderDoc 和捕获目标后才能部署。
2026-11-04 普通 GUI 兼容版已部署。版本字符串仍是构建时的基础提交号；
请同时核对 ``CUSTOM_RUNTIME.md`` 顶部的 GUI 和 DLL SHA256。

直接打开 ``C:\RenderDocCustom\qrenderdoc.exe``，在 Launch Application 中选择安装根目录
的 ``Returnal.exe``，保持 Compatibility Mode 为 Automatic detection，点击 Launch。
不需要 CMD 或预先载入 ``Returnal.cap``。工作目录和参数可留空；摘要会显示真实游戏程序、
默认安装根目录、补充的参数以及 DLSS 提醒。
详见 :doc:`gui_launch_compatibility`。

原有 ``launch_returnal_capture.cmd`` 和 ``Returnal.cap`` 保留兼容用途。
其手动配置也可继续使用：

* Executable: ``E:\SteamLibrary\steamapps\common\Returnal\Returnal\Binaries\Win64\Returnal-Win64-Shipping.exe``
* Working directory: ``E:\SteamLibrary\steamapps\common\Returnal``
* Command line: ``Returnal``

标准启动代码从 Steam manifest 补充运行环境。不需要 Global Hook，也不需要修改游戏文件。
普通 Steam 启动链为 ``Launcher.exe -> Returnal.exe -> Returnal-Win64-Shipping.exe``；
直接运行根目录 EXE 且缺少 Steam 环境时，会重新向 Steam 提交带参数的请求并可能停在确认框。

等待着色器预编译结束、进入可渲染场景后，按 F12 或使用 RenderDoc 捕获按钮。
保留原生 D3D12；无需为本次启动修复强制切换 D3D11。
截帧前在游戏“设置 -> 图像 -> 屏幕优化”中选择关闭 DLSS，并确认设置已应用。
不要仅修改 ``GameUserSettings.ini``：游戏可能从存档中的图形选项恢复 DLSS。

回放限制与对照
--------------

2026-11-04 首次实测保留原有 DLSS Quality、1440p 和光追设置，成功捕获 D3D12
文件并完成独立回放初始化，但最终图像只有 HUD，缺少场景，不能当作完整回放成功。
随后使用独立的 ``-UserDir`` 测试配置关闭 DLSS，保留 2560×1440、TAA 和光追设置，
标准 Launch 成功进入场景。公开 Target Control 报告 D3D12 正在呈现且支持捕获，
生成 ``Returnal_native_off_frame3953.rdc``（2,433,082,839 字节）。
独立 qrenderdoc 进程成功加载，并从 Present 事件 16034 导出完整人物、场景、光照及 HUD。
该捕获包含 2,531 个动作、1,886 个 draw/dispatch、781 个纹理和 534 个缓冲。

因此当前已验证的使用范围是关闭 DLSS 的 D3D12 捕获和回放；并非全部图形设置均已支持。
本修复没有实现 DLSS 重建执行的序列化或回放。
测试使用复制到独立目录的配置和存档，原用户配置仍保留 DLSS Quality；
日常使用上述 ``Returnal.cap`` 时，需要在游戏菜单自行关闭 DLSS。

本机已验证捕获的 SHA256 为
``B2771E8A9937EC896CD60827AE566C5C33391C5B8268CC4FB85FCE69906E7051``。
最终回放图位于
``E:\RenderDocProject\work\returnal-20261104\native-off-replay-final.png``。

源码原因与回归
--------------

启动失败的明确原因是 EOS 叠加层与 RenderDoc 的函数解析钩子递归。
调试器捕获到 ``c00000fd`` 栈溢出，堆栈交替经过
``Hooked_GetProcAddress`` 和 ``EOSOVH-Win64-Shipping.dll`` 的转发调用。
RenderDoc 修改叠加层导入表后，叠加层保存的原始解析器变成 RenderDoc 钩子，形成环。

``win32_hook.cpp`` 对 EOSOVH 叠加层采用与 Steam/Fraps 相同的导入表排除原则，
继续截获游戏和普通模块的图形调用；没有排除整个 EOSSDK。

可运行以下独立回归检查（需要 Visual Studio C++ 工具和 Windows SDK）：

.. code-block:: powershell

   ./util/test/win32_overlay_resolver_contract.ps1 -RenderDocDll C:/RenderDocCustom/renderdoc.dll
   ./util/test/win32_launch_contract.ps1 -RenderDocDll C:/RenderDocCustom/renderdoc.dll
   ./util/test/d3d11_optout_contract.ps1 -RenderDocDll C:/RenderDocCustom/renderdoc.dll

叠加层检查验证 EOS 导入仍调用系统解析器、普通应用和模块仍获得 RenderDoc 钩子，
以及不存在的导出仍返回空。相同探针在修复前的 DLL 上失败，在修复后通过。

本机证据保存在 ``E:\RenderDocProject\work\returnal-20261104``，
捕获文件保存在 ``E:\RenderDocProject\captures``，不随源码提交。
