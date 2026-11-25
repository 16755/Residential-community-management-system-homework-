# GUI verification

Build with Visual Studio 2022 (C++ desktop workload, Windows SDK) using C++17.
The solution's 32-bit platform is named `x86`; the project maps it to `Win32`.

```powershell
./tests/Run-UiTests.ps1
./tests/Smoke-UiWindow.ps1
```

`Run-UiTests.ps1` builds Debug x64, links a standalone test executable against
the application's objects, and runs all workflows in a fresh temporary data
directory. It covers four-role permissions, owner isolation, CRUD workflows,
GBK/UTF-8 round trips, invalid input, persistence, ImGui frames, undocking and
layout restore. Test datasets are removed on success; failed data is retained
for diagnosis. Compiler output stays in a temporary build directory.

`Smoke-UiWindow.ps1` copies the executable and existing data into a temporary
directory, starts the real Win32/DX11 window, logs in using keyboard events,
captures PNGs, minimizes/restores/resizes the window, and requests a normal
close. It briefly shows the test window and saves captures in the printed
temporary directory. It never opens the project's CSV files for writing.

The application searches for `data` beside its executable, then in ancestor
directories, so Visual Studio output builds use the project's existing data.
For deployment, place `data` alongside the executable. Window layout lives in
`%LOCALAPPDATA%/XiaoquManagement/layout.ini`; the top menu restores the default
three-panel layout. GUI input uses UTF-8 and is converted to the existing GBK
business strings; unsupported characters are rejected rather than replaced.

The original `IntegrationSystem::run()` console workflow remains compiled.
`main()` now calls `community_ui::runApplication()` and the executable uses the
Windows subsystem, so normal GUI startup creates no console window.
