鸣潮：启动兼容性核查
===================

截至 2026-11-04，本分支尚未验证鸣潮正式客户端的截帧或回放成功。
本页记录实际测试结果，不作为已经支持该游戏的声明。

使用的构建
----------

已重新构建提交 ``7a616488fc39469019421a605e50bcccf07c2bdc`` 的
x64/x86 Development，通过仓库 ``deploy_custom_runtime.ps1`` 部署至
``C:\RenderDocCustom`` 及其 ``x86`` 子目录。关键运行文件与构建输出逐一核对 SHA256。
使用以下命令检查部署提交号，不能只凭版本号 ``1.47`` 区分新旧::

    C:\RenderDocCustom\renderdoccmd.exe version
    C:\RenderDocCustom\x86\renderdoccmd.exe version

部署副本通过 48 项 D3D11、6 项 DXGI 合约检查及公开回放初始化检查。
GUI 日志确认 D3D11、D3D12 回放设备均在本机 NVIDIA GPU 上初始化成功。
这些检查不等于游戏进程的图形接口已被捕获。

启动对照结果
------------

* 未运行 RenderDoc、未注入的直接 ``Wuthering Waves.exe`` 启动报错
  ``kuro: Use launcher to start game!``，因此不能将直接启动失败归因于 RenderDoc。
* 官方入口为 ``E:\Program Files\Wuthering Waves\launcher.exe``，本机实际启动器是
  ``3.0.1.0\launcher_main.exe``。从官方启动器正常进入游戏可显示渲染画面和加载进度。
* 关闭上述对照实例及已有启动器后，使用新版 GUI 标准 Launch 启动官方入口，启用
  **Capture Child Processes**，未添加原神兼容环境变量，未启用 Global Hook。
  官方启动器窗口可正常打开，但日志显示对实际 ``launcher_main.exe`` 注入失败，
  未找到捕获 DLL，控制连接被拒绝。这个注入失败的具体原因仍未确认。
* 随后从该官方启动器点击 **进入游戏**，再次出现
  ``警告 (3, 1060, 158004)``，正文为
  ``检测到不兼容程序 RenderDoc，请关闭并重启机器后再试。``
  这次警告在新版部署下实际复现，不是沿用旧版截图。

当前结论与后续验证
------------------

当前证据确认启动链路会明确拒绝 RenderDoc；没有得到游戏内 D3D11/D3D12 捕获结果，
也不能由此断言存在某个着色器、交换链或设备包装缺陷。
警告窗口由系统代呈现，窗口所属进程本身不足以定位发出警告的组件。
未对错误码的内部含义或具体检测机制作推断。

后续需要游戏方允许的图形捕获方式或可调试构建，再验证游戏内设备创建、呈现、截帧和独立回放。
原神的环境变量配置没有在鸣潮上验证，不能直接套用。
本次核查没有修改游戏或保护组件，没有执行重启，也没有通过隐藏工具或规避检测推进。

源码诊断修正
------------

后续源码检查发现，``LaunchAndInjectIntoProcess`` 仅在 replacement 注入成功时更新结果。
replacement 失败时，它会错误地返回已退出 bootstrap 的成功结果及旧端口，
将真正的启动错误掩盖成稍后的连接拒绝。现已修正为传回所选 replacement 的真实结果。

日志补充了目标 PID、DLL 路径、WOW64 状态、加载线程等待结果/耗时/退出状态、
进程存活状态、模块是否找到及 replacement 的返回状态。线程退出 DWORD
不等于目标线程的 Win32 last-error，也不能作为完整的 x64 模块句柄解释。
无法打开目标进程时会立即返回原始 Win32 错误，避免误报为位数查询失败。

可在仓库根目录运行自有目标回归检查::

    powershell -NoProfile -File util/test/win32_launch_contract.ps1

该测试只启动仓库自建程序，检查普通启动、明确拒绝捕获 DLL 的 replacement
及无效 PID。旧部署在 replacement 用例错误返回成功；修正后返回
``InjectionFailed`` 和端口 0。三个用例及新增诊断日志断言均通过，
x64/x86 Development 均构建成功。测试不弱化子进程的加载策略。

上述源码诊断修正已随 2026-11-04 的后续 Returnal 验证构建部署至
``C:\RenderDocCustom``，部署二进制 SHA256 见 ``CUSTOM_RUNTIME.md``。
该后续构建尚未重复鸣潮正式客户端的启动对照。官方启动器此前失败的实际加载原因
仍待新增日志验证；不能由错误上报修正或 Returnal 验证成功推断鸣潮加载问题已解决。
