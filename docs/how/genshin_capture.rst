原神：Windows D3D11 捕获
=======================

这份说明适用于本分支的自定义 RenderDoc。2026-10-31 已在本机当前正式客户端
验证普通 Launch、游戏内截帧及独立回放。它不保证其他游戏、后续客户端版本或所有渲染路径。

准备运行时
----------

使用本分支编译出的 x64 Development 运行时。构建前关闭使用该构建目录的 RenderDoc
及其捕获目标，避免 DLL 被占用。按照仓库 Windows 构建说明准备 Visual Studio、Windows SDK
和依赖，在开发者命令提示符的仓库根目录执行::

    msbuild renderdoc.sln /t:Build /p:Configuration=Development /p:Platform=x64 /m

2026-10-31 的原神捕获验证使用 ``x64/Development/qrenderdoc.exe``。
2026-11-04 已重新构建提交 ``7a61648`` 并更新 ``C:\RenderDocCustom`` 的 x64/x86 运行时；
部署副本通过 D3D11、DXGI 和公开回放初始化自测，本次未重复原神游戏内截帧。
可用 ``C:\RenderDocCustom\renderdoccmd.exe version`` 核对提交号，避免误开其他安装版本。
保留构建输出的 ``renderdoc.dll`` 和同字节的 ``rdoc.dll``；标准 Launch 会按本项目
现有规则选择目标运行时。不需要向游戏目录复制 DLL，也不需要启用 Global Hook。

启动和截帧
----------

1. 双击仓库根目录的 ``launch_genshin_capture.cmd``。它打开本仓库的 Development GUI，
   并载入 ``Genshin.cap``，不会自动启动游戏。
   也可以自行打开该 GUI，在 Launch Application 中点击 **Load Settings** 选择此配置。
2. 检查 **Executable Path** 和 **Working Directory**。
   示例默认路径为 ``E:\Program Files\miHoYo Launcher\games\Genshin Impact Game\YuanShen.exe``；
   如安装目录不同，改为实际 EXE 及其所在目录，并用 **Save Settings** 保存副本。
3. 确认 Environment Variables 中有以下两项 Set 设置，关闭其他原神实例，再点击 **Launch**。
4. 正常进入游戏后按 **F12**，或者使用捕获会话的 **Capture Frame(s) Immediately**。
5. 截帧出现在 RenderDoc 捕获列表后，双击打开回放，并用 **Save** 保存到固定位置。
   不要仅凭生成文件就认定成功：还应检查事件列表、纹理和最终输出画面。

请从 Launch 创建新进程。本配置没有验证“先启动游戏，再 Inject”的工作流；
DXGI 入口策略在 DLL 注册挂钩时确定，之后修改环境变量不能追溯改变已安装的挂钩。

兼容选项及原理
--------------

这两个变量在配置文件中仅作用于被启动的进程，无需修改系统环境变量：

``RENDERDOC_DXGI_HOOK_FACTORY0=0``
    禁用旧版 ``CreateDXGIFactory`` 的导入/动态查询和导出补丁拦截。
    保留 Factory1/2、调试接口和其他图形 API 挂钩。当前原神的呈现工厂使用 Factory1。
    依赖旧版工厂创建呈现对象的程序不应直接套用这一配置。
    未设置或值不为 ``0`` 时保持原有拦截。

``RENDERDOC_D3D11_CAPTURE_OPTOUT=1``
    显式允许捕获带 ``D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY``
    标志的设备。保留原始 ``0x80`` 和应用指定的 DEBUG 标志，不修改应用选择的调试层。
    因此这类设备不会因 **Enable API Validation** 而改变调试层选择。
    未设置或值不为 ``1`` 时，仍按默认 opt-out 策略返回原生设备和上下文。

故障隔离表明：仅转发原生对象的旧版工厂拦截仍可触发退出，而 Factory1 包装可正常运行。
所以只修正 COM 对象包装不足以解决退出。另一个独立问题是设备创建标志 ``0x81``
导致 RenderDoc 主动放弃捕获；上述显式选项把捕获选择与原始设备标志分开。
游戏内部为什么响应旧版入口拦截仍未确认，不能将最终异常地址直接当作完整根因。

代码同时修复了默认 opt-out 的 immediate context 输出，以及不请求 device 输出时的
临时设备生命周期。DXGI 工厂导出保留原先前导模式，并支持本机 Windows 的五字节
``mov [rsp+8], rbx`` 前导，避免截断后续指令。没有加入原神进程名分支或修改游戏文件。

验证与排查
----------

在仓库根目录运行（需要 Visual Studio C++ 工具及 Windows SDK）::

    powershell -NoProfile -File util/test/d3d11_optout_contract.ps1
    powershell -NoProfile -File util/test/dxgi_factory_option_contract.ps1

两个脚本均可用 ``-RenderDocDll`` 指定构建 DLL，用 ``-OutputDirectory`` 指定结果目录。
已通过 48 项 D3D11 原生/默认/显式覆盖合约检查、6 项 DXGI 默认/兼容入口检查，
以及公开回放初始化检查。原神普通 CLI 启动截帧的独立回放含 1003 次绘制和 98 次计算调度，
角色、场景及 UI 可见；GUI 启动亦收到多次截帧并成功回放。大型 RDC、图片和本机日志不入库。

若仍不能捕获，先核对运行时版本及两个环境变量。日志应有
``CreateDXGIFactory interception disabled`` 和 ``D3D11 capture opt-out override enabled``，
捕获会话应显示 D3D11 正在呈现且可捕获。旧运行时不识别新选项。
Vulkan 注册提示不影响这条 D3D11 路径；Vulkan 本身也不只用于移动端。
