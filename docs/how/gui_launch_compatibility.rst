普通 GUI 启动兼容模式
====================

直接启动
--------

1. 打开部署目录中的 ``C:\RenderDocCustom\qrenderdoc.exe``。
   不需要从 CMD 启动，不需要加载 ``.cap``，也不需要设置系统环境变量。
2. 在 **Launch Application** 中选择游戏 EXE，保持 **Compatibility Mode** 为
   **Automatic detection**。工作目录和命令行可留空。
3. 阅读下方只读摘要，确认识别的游戏、实际启动程序和补充设置，再点击 **Launch**。
4. 用户自行完成协议确认和登录。游戏进入可操作场景后，确认捕获会话显示
   **D3D11 (Active)** 或 **D3D12 (Active)**，点击 **Capture Frame(s) Immediately**，
   或在游戏中按 F12。会在失焦时暂停的游戏，可在启动前用 **Queue Capture** 指定足够晚的
   场景帧，保持游戏前台等待捕获。保存 RDC，再用独立 RenderDoc 进程加载，检查场景、人物和 HUD。

请先正常退出同一游戏的已有实例。自动策略只用于本地 Windows 的 Launch 新进程；
Inject 已有进程和远程捕获仍使用手动设置。

支持的选择
----------

* 原神：选择 ``Genshin Impact Game\YuanShen.exe``，同目录须有 ``YuanShen_Data``。
* 星穹铁道：选择 ``Star Rail Game\StarRail.exe``，同目录须有 ``StarRail_Data``。
* 绝区零：选择 ``ZenlessZoneZero Game\ZenlessZoneZero.exe``，同目录须有
  ``ZenlessZoneZero_Data``。
* Returnal：选择安装根目录的 ``Returnal.exe``，或
  ``Returnal\Binaries\Win64\Returnal-Win64-Shipping.exe``。
  两处 EXE 都必须存在。保持 Steam 正常运行。

识别不固定盘符，EXE 名称按完整名称匹配且不区分大小写，不接受相似文件名。
三个米哈游游戏共享既有原神 D3D11 策略：仅向目标进程补充
``RENDERDOC_DXGI_HOOK_FACTORY0=0`` 和 ``RENDERDOC_D3D11_CAPTURE_OPTOUT=1``。
不强制添加图形 API 参数。选项原理见 :doc:`genshin_capture`。

Returnal 自动使用真实 Shipping EXE、以安装根目录为默认工作目录，补齐开头的
``Returnal`` 参数并启用子进程捕获。已有的 ``Returnal`` 或 ``"Returnal"`` 前缀不会重复，
其余参数和非空的手动工作目录保留。目录结构不完整时拒绝启动并显示错误。

**Returnal 完整场景回放仍需关闭 DLSS**，详见 :doc:`returnal_capture`。
摘要只提醒这一限制，不会修改游戏设置或存档。

手动覆盖与配置
--------------

**Standard / manual settings only** 关闭自动处理，保留用户原有配置。
**MiHoYo D3D11** 和 **Returnal D3D12** 可显式选择对应策略，但仍检查安装结构。
其他程序在自动模式下保持标准行为。

Environment Variables 只显示并保存手动项。自动项仅存在于本次有效启动设置，
切换游戏不会残留。Windows 下变量名不区分大小写；同名手动项优先，保留其 Set、Append
或 Prepend 操作和值。若手动值使捕获失效，请按摘要检查该项。

``CaptureSettings.compatibilityProfile`` 是 GUI/Python 共用的字符串字段，取值为
``auto``、``standard``、``mihoyo-d3d11``、``returnal-d3d12``。
保存 ``.cap`` 时记录此选择和原始手动设置，不记录自动补充项。
旧配置缺少该字段时使用 ``auto``；未知值显示提示并按手动模式运行。
旧 CMD 和 ``.cap`` 仍可用，但不再是普通启动的前提。

Python 可沿现有 CaptureDialog 入口配置；预览、Launch 和 autoStart 使用同一解析器：

.. code-block:: python

   settings = qrenderdoc.CaptureSettings()
   settings.executable = r"E:\Program Files\miHoYo Launcher\games\Star Rail Game\StarRail.exe"
   settings.compatibilityProfile = "auto"
   dialog = pyrenderdoc.GetCaptureDialog()
   dialog.SetSettings(settings)
   dialog.Launch()

实现和回归
----------

解析器位于 ``qrenderdoc/Code/LaunchCompatibility.cpp``，输入原始设置，输出有效副本、
摘要和错误。核心驱动默认策略、公共 RenderDoc C API 和 RDC 格式未改变。
本次借鉴本机参考分支 ``d5725b9`` 的自动选择/显式关闭设计，未整批合入其
D3D11 工厂桥接、NVAPI、目标标记或工具改名改造。

x64/x86 Development 构建已通过。策略/配置测试命令为：

.. code-block:: text

   qrenderdoc.exe --unittest log=launch-compatibility.txt [launch-compatibility]

测试覆盖四个目标、大小写、空格路径、相似名/错误目录、手动覆盖、作用域隔离、参数去重、
切换无残留及新旧配置序列化。D3D11、DXGI、启动结果、EOS 解析器与公开回放初始化
合约也须通过。它们验证代码契约，不能代替每个客户端的场景截帧和独立回放。
本机本轮实测范围、部署哈希见仓库根目录 ``CUSTOM_RUNTIME.md``。

2026-11-04 本轮 GUI 验收
-----------------------

* 原神、星穹铁道、绝区零：分别通过直接 GUI Launch 连接 **D3D11 (Active)**。
  登录后场景截帧、人物和 HUD 的独立回放均仍待逐游戏验收。
* Returnal：根目录 EXE 自动解析、**D3D12 (Active)**、第 5000 帧捕获及独立进程回放通过。
  最终输出已检查场景、人物、生命值、任务提示和小地图。
  此次使用保留用户原配置的隔离配置目录，关闭 DLSS；不代表 DLSS 开启时回放已修复。
