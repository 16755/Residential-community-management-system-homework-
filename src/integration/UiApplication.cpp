// ============================================================================
// 文件：UiApplication.cpp
// 职责：管理原生窗口、DirectX 11 资源及 ImGui 帧循环。
// 业务窗口（登录、导航、数据表格、操作面板）的绘制集中在 ui.cpp。
// ============================================================================
#include "integration/ui.h"

#include <d3d11.h>
#include <shlobj.h>
#include <stdexcept>
#include <system_error>
#include <wrl/client.h>

namespace {
using Microsoft::WRL::ComPtr;

// 编码转换的公共步骤：按指定代码页解码为宽字符串，拒绝无效字节序列。
std::wstring decode(const std::string& value, UINT codePage) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(
        codePage, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (!size) {
        throw std::runtime_error("文字编码无效，请检查输入内容。");
    }
    std::wstring result(size, L'\0');
    MultiByteToWideChar(codePage,
                        MB_ERR_INVALID_CHARS,
                        value.data(),
                        static_cast<int>(value.size()),
                        result.data(),
                        size);
    return result;
}

// 主窗口运行资源：集中保存窗口句柄、待处理尺寸及图形对象，退出时统一释放。
struct WindowRuntime {
    HWND window = nullptr;
    UINT width = 0;
    UINT height = 0;
    bool minimized = false;
    bool occluded = false;
    bool contextCreated = false;
    bool platformReady = false;
    bool rendererReady = false;
    std::string iniPath; // 在 ImGui 上下文销毁前保持有效，异常清理时也可保存布局。
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11RenderTargetView> target;

    // 主窗口渲染目标：绑定交换链后台缓冲，窗口尺寸变化后需重新创建。
    bool createTarget() {
        ComPtr<ID3D11Texture2D> buffer;
        const HRESULT bufferResult = swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer));
        if (FAILED(bufferResult)) {
            return false;
        }

        const HRESULT targetResult = device->CreateRenderTargetView(buffer.Get(), nullptr, &target);
        return SUCCEEDED(targetResult);
    }

    // 初始化 DirectX 11 设备与交换链；硬件不可用时尝试 WARP 软件渲染。
    bool createDevice() {
        DXGI_SWAP_CHAIN_DESC description{};
        description.BufferCount = 2;
        description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.OutputWindow = window;
        description.SampleDesc.Count = 1;
        description.Windowed = TRUE;
        description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
        D3D_FEATURE_LEVEL selected{};
        HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr,
                                                       D3D_DRIVER_TYPE_HARDWARE,
                                                       nullptr,
                                                       0,
                                                       levels,
                                                       2,
                                                       D3D11_SDK_VERSION,
                                                       &description,
                                                       &swapChain,
                                                       &device,
                                                       &selected,
                                                       &context);
        if (FAILED(result)) {
            swapChain.Reset();
            device.Reset();
            context.Reset();
            result = D3D11CreateDeviceAndSwapChain(nullptr,
                                                   D3D_DRIVER_TYPE_WARP,
                                                   nullptr,
                                                   0,
                                                   levels,
                                                   2,
                                                   D3D11_SDK_VERSION,
                                                   &description,
                                                   &swapChain,
                                                   &device,
                                                   &selected,
                                                   &context);
        }
        if (FAILED(result)) {
            return false;
        }
        ComPtr<IDXGIFactory> factory;
        if (SUCCEEDED(swapChain->GetParent(IID_PPV_ARGS(&factory)))) {
            factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
        }
        return createTarget();
    }

    // 资源兜底清理：同时覆盖正常退出和初始化中途失败的情况。
    ~WindowRuntime() {
        if (rendererReady) {
            ImGui_ImplDX11_Shutdown();
        }
        if (platformReady) {
            ImGui_ImplWin32_Shutdown();
        }
        if (contextCreated) {
            ImGui::DestroyContext();
        }
        target.Reset();
        swapChain.Reset();
        context.Reset();
        device.Reset();
        if (window && IsWindow(window)) {
            DestroyWindow(window);
        }
    }
};

} // namespace

// Win32 后端的消息处理函数声明在全局作用域，供下面的窗口过程转发输入。
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
// ============================================================================
// 原生主窗口消息处理：先交给 ImGui 接收输入，再处理尺寸、DPI 和退出事件。
// ============================================================================
LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui::GetCurrentContext() &&
        ::ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) {
        return 1;
    }
    auto* runtime = reinterpret_cast<WindowRuntime*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        // 创建窗口时关联运行资源，后续消息即可通过窗口句柄找到对应状态。
        runtime =
            static_cast<WindowRuntime*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(runtime));
    }
    switch (message) {
        case WM_SIZE:
            // 此处只记录尺寸和最小化状态；交换链调整留到帧循环中执行。
            if (runtime) {
                runtime->minimized = wParam == SIZE_MINIMIZED;
                if (!runtime->minimized) {
                    runtime->width = LOWORD(lParam);
                    runtime->height = HIWORD(lParam);
                }
            }
            return 0;
        case WM_DPICHANGED: {
            // 跨越不同缩放比例的显示器时，应用系统推荐的窗口位置与尺寸。
            const RECT& suggested = *reinterpret_cast<RECT*>(lParam);
            SetWindowPos(window,
                         nullptr,
                         suggested.left,
                         suggested.top,
                         suggested.right - suggested.left,
                         suggested.bottom - suggested.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) {
                return 0;
            }
            break;
        case WM_DESTROY:
            // 主窗口关闭后通知消息循环退出，图形资源由运行对象负责清理。
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

// 全局浅色主题：统一窗口间距、控件、表格表头及停靠背景，并按 DPI 缩放。
void applyStyle(float scale) {
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(20, 18);
    style.FramePadding = ImVec2(10, 8);
    style.ItemSpacing = ImVec2(10, 10);
    style.CellPadding = ImVec2(10, 9);
    style.FrameRounding = 5;
    style.GrabRounding = 5;
    style.PopupRounding = 7;
    style.TabRounding = 5;
    style.WindowRounding = 0;
    style.WindowBorderSize = 1;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(.98f, .985f, .995f, 1);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(.96f, .97f, .985f, 1);
    style.Colors[ImGuiCol_Text] = ImVec4(.13f, .19f, .28f, 1);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(.43f, .49f, .58f, 1);
    style.Colors[ImGuiCol_Header] = ImVec4(.82f, .89f, 1, 1);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(.72f, .83f, 1, 1);
    style.Colors[ImGuiCol_Button] = ImVec4(.82f, .89f, 1, 1);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(.66f, .79f, .98f, 1);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(.47f, .68f, .96f, 1);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(.9f, .93f, .98f, 1);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(.91f, .94f, .98f, 1);
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
}

// 以可执行文件定位路径，避免从不同工作目录启动时加载到不同业务数据。
std::filesystem::path executableDirectory() {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) {
        throw std::runtime_error("无法确定程序所在目录。");
    }
    path.resize(length);
    return std::filesystem::path(path).parent_path();
}

// 布局配置保存到当前用户的本地应用目录，记录停靠位置及窗口分离状态。
std::string layoutFile() {
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) {
        throw std::runtime_error("无法读取用户配置目录。");
    }
    const std::filesystem::path directory = std::filesystem::path(local) / L"XiaoquManagement";
    CoTaskMemFree(local);
    std::filesystem::create_directories(directory);
    return (directory / L"layout.ini").u8string();
}

// 中文字体：优先加载微软雅黑，缺失时使用宋体，两者都不可用则明确报错。
void loadChineseFont() {
    wchar_t windowsDirectory[MAX_PATH]{};
    if (!GetWindowsDirectoryW(windowsDirectory, MAX_PATH)) {
        throw std::runtime_error("无法定位系统字体目录。");
    }
    for (const auto* name : {L"msyh.ttc", L"simsun.ttc"}) {
        const auto path = std::filesystem::path(windowsDirectory) / L"Fonts" / name;
        if (std::filesystem::exists(path) &&
            ImGui::GetIO().Fonts->AddFontFromFileTTF(
                path.u8string().c_str(),
                18.0f,
                nullptr,
                ImGui::GetIO().Fonts->GetGlyphRangesChineseFull())) {
            return;
        }
    }
    throw std::runtime_error("未找到可用中文字体（微软雅黑或宋体）。");
}
} // namespace

namespace community_ui {
// 业务数据进入界面：将现有 GBK 字符串转换为 ImGui 使用的 UTF-8。
std::string toUtf8(const std::string& value) {
    const std::wstring wide = decode(value, 936);
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8,
                        0,
                        wide.data(),
                        static_cast<int>(wide.size()),
                        result.data(),
                        size,
                        nullptr,
                        nullptr);
    return result;
}

// 界面输入提交业务层：严格转换回 GBK，禁止无法表示的字符被替换为问号。
std::string toBusiness(const std::string& value) {
    const std::wstring wide = decode(value, CP_UTF8);
    if (wide.empty()) {
        return {};
    }
    BOOL replaced = FALSE;
    const int size = WideCharToMultiByte(936,
                                         WC_NO_BEST_FIT_CHARS,
                                         wide.data(),
                                         static_cast<int>(wide.size()),
                                         nullptr,
                                         0,
                                         nullptr,
                                         &replaced);
    if (!size || replaced) {
        throw std::runtime_error("输入包含当前数据格式不支持的字符，请使用常用中文、字母或数字。");
    }
    std::string result(size, '\0');
    WideCharToMultiByte(936,
                        WC_NO_BEST_FIT_CHARS,
                        wide.data(),
                        static_cast<int>(wide.size()),
                        result.data(),
                        size,
                        nullptr,
                        &replaced);
    if (replaced) {
        throw std::runtime_error("文字无法完整保存，请检查输入内容。");
    }
    return result;
}

// 从程序目录逐级向上查找已有 data；均未找到时使用程序旁的 data。
std::filesystem::path findDataDirectory(const std::filesystem::path& exeDirectory) {
    for (auto directory = exeDirectory; !directory.empty();) {
        if (std::filesystem::is_directory(directory / L"data")) {
            return directory / L"data";
        }
        const auto parent = directory.parent_path();
        if (parent == directory) {
            break;
        }
        directory = parent;
    }
    return exeDirectory / L"data";
}

// ============================================================================
// GUI 应用入口：创建主窗口 → 初始化界面 → 处理消息及绘制 → 保存布局并退出。
// ============================================================================
int runApplication() {
    // 注册原生主窗口类型，所有业务面板由窗口内的 ImGui 停靠空间承载。
    const wchar_t* className = L"CommunityManagementImGui";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = className;
    try {
        ImGui_ImplWin32_EnableDpiAwareness();
        if (!RegisterClassExW(&windowClass)) {
            throw std::runtime_error("注册窗口失败。");
        }
        {
            WindowRuntime runtime;
            // 以 1440×900 为基准，按主显示器 DPI 计算初始客户区尺寸。
            const float scale = ImGui_ImplWin32_GetDpiScaleForMonitor(
                MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));
            RECT rectangle{0, 0, static_cast<LONG>(1440 * scale), static_cast<LONG>(900 * scale)};
            AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);
            runtime.window = CreateWindowExW(0,
                                             className,
                                             L"小区管理系统",
                                             WS_OVERLAPPEDWINDOW,
                                             CW_USEDEFAULT,
                                             CW_USEDEFAULT,
                                             rectangle.right - rectangle.left,
                                             rectangle.bottom - rectangle.top,
                                             nullptr,
                                             nullptr,
                                             windowClass.hInstance,
                                             &runtime);
            if (!runtime.window || !runtime.createDevice()) {
                throw std::runtime_error("创建窗口或 DirectX 11 设备失败。");
            }

            // 创建 ImGui 上下文，开启键盘导航、停靠和可拖出主窗口的多视口。
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            runtime.contextCreated = true;
            auto& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable |
                              ImGuiConfigFlags_ViewportsEnable;
            io.ConfigDpiScaleFonts = true;
            io.ConfigDpiScaleViewports = true;
            // 将布局文件路径交给 ImGui，并初始化主题、中文字体及平台/渲染后端。
            runtime.iniPath = layoutFile();
            io.IniFilename = runtime.iniPath.c_str();
            applyStyle(scale);
            loadChineseFont();
            runtime.platformReady = ImGui_ImplWin32_Init(runtime.window);
            runtime.rendererReady =
                ImGui_ImplDX11_Init(runtime.device.Get(), runtime.context.Get());
            if (!runtime.platformReady || !runtime.rendererReady) {
                throw std::runtime_error("初始化界面后端失败。");
            }
            // GUI 与原控制台共用业务系统；界面只管理显示与交互状态。
            const auto programDirectory = executableDirectory();
            const auto dataDirectory = findDataDirectory(programDirectory);
            IntegrationSystem system(dataDirectory.string());
            CommunityUI ui(system);
            ShowWindow(runtime.window, SW_SHOWDEFAULT);
            UpdateWindow(runtime.window);
            bool done = false;
            while (!done) {
                // 每帧先处理原生消息，使键盘、鼠标、窗口拖动和关闭及时响应。
                MSG message{};
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                    if (message.message == WM_QUIT) {
                        done = true;
                    }
                }
                if (done) {
                    break;
                }
                // 最小化或窗口被遮挡时暂停绘制，保留消息处理以便恢复显示。
                if (runtime.minimized) {
                    Sleep(20);
                    continue;
                }

                if (runtime.occluded) {
                    const HRESULT visibilityResult =
                        runtime.swapChain->Present(0, DXGI_PRESENT_TEST);
                    if (visibilityResult == DXGI_STATUS_OCCLUDED) {
                        Sleep(20);
                        continue;
                    }
                }
                runtime.occluded = false;

                if (runtime.width && runtime.height) {
                    // 消费尺寸变化：释放旧目标、调整后台缓冲，再建立新渲染目标。
                    runtime.target.Reset();
                    const HRESULT resizeResult = runtime.swapChain->ResizeBuffers(
                        0, runtime.width, runtime.height, DXGI_FORMAT_UNKNOWN, 0);
                    if (FAILED(resizeResult)) {
                        throw std::runtime_error("调整窗口尺寸失败。");
                    }
                    if (!runtime.createTarget()) {
                        throw std::runtime_error("调整窗口尺寸失败。");
                    }

                    runtime.width = 0;
                    runtime.height = 0;
                }
                // 开始界面帧：由 CommunityUI 绘制菜单栏、登录或三个业务窗口。
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();
                ui.render();
                ImGui::Render();
                // 主窗口绘制：清空背景，再将 ImGui 生成的绘制数据提交给 DX11。
                auto* target = runtime.target.Get();
                runtime.context->OMSetRenderTargets(1, &target, nullptr);
                const float clear[] = {.91f, .94f, .98f, 1};
                runtime.context->ClearRenderTargetView(target, clear);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                // 分离窗口绘制：更新拖出主窗口的视口，并逐一完成渲染。
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                // 垂直同步提交主窗口画面，同时记录遮挡状态并检查设备错误。
                const HRESULT result = runtime.swapChain->Present(1, 0);
                runtime.occluded = result == DXGI_STATUS_OCCLUDED;
                if (FAILED(result)) {
                    throw std::runtime_error("图形设备已断开，请重新启动程序。");
                }
            }
            // 显式关闭后端与上下文，保证保存布局时 io.IniFilename 仍然有效。
            ImGui_ImplDX11_Shutdown();
            runtime.rendererReady = false;
            ImGui_ImplWin32_Shutdown();
            runtime.platformReady = false;
            ImGui::DestroyContext();
            runtime.contextCreated = false;
        }
        UnregisterClassW(className, windowClass.hInstance);
        return 0;
    } catch (const std::exception& error) {
        // 初始化或运行失败时通过原生消息框报告，GUI 无需依赖控制台输出。
        std::wstring message;
        try {
            message = decode(error.what(), CP_UTF8);
        } catch (...) {
            message = L"初始化或运行界面失败，请检查数据与运行环境。";
        }
        MessageBoxW(nullptr, message.c_str(), L"小区管理系统", MB_OK | MB_ICONERROR);
        UnregisterClassW(className, windowClass.hInstance);
        return 1;
    }
}
} // namespace community_ui
