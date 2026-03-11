# Qt Multi Player

这是一个基于 Qt Widgets 和 CMake 的 C++ 多屏视频播放器工程，按 `config.json` 配置为不同屏幕创建窗口，并在每个窗口中以网格方式播放多个视频。

## 构建

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.6.3/msvc2019_64
cmake --build build --config Release
```

## 运行

```powershell
.\build\Release\QtMultiPlayer.exe -f C:/Users/groot/Documents/project/mutilplayer/config_local.json
```

当前工程默认调试配置也直接使用：

```text
C:\Users\groot\Documents\project\mutilplayer\config_local.json
```

## 测试

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

当前提供两类测试：

- `config_test`：验证 `config.json` 解析结果是否符合预期
- `app_smoke_test`：以 `offscreen` 模式启动程序并自动退出，验证构建产物可运行

## VS Code

仓库已包含 `.vscode` 配置，推荐安装：

- `ms-vscode.cmake-tools`
- `ms-vscode.cpptools`

常用方式：

- `Ctrl+Shift+B`：执行 `CMake: build`
- 运行任务 `CTest`：执行全部测试
- 直接点击 VS Code 底部状态栏的 `Debug`：由 CMake Tools 直接调试当前 launch target

为保证底部状态栏的 `Debug` 可以直接工作，工程现在做了这几件事：

- 强制 `CMake Tools` 使用 `CMakePresets.json`
- 通过 `cmake.debugConfig` 固定调试参数、工作目录和 Qt 运行时环境变量
- 测试不再单独生成额外的可执行文件，避免状态栏需要切换 launch target

## 配置格式

配置格式沿用 Python 项目：

```json
{
  "hwaccel": "d3d11va",
  "screens": {
    "0": {
      "cols": 2,
      "video": [
        {
          "path": "demo.mp4",
          "play_sections": [
            { "start_time": 1, "duration": 4 },
            { "start_time": 6, "duration": -1 }
          ]
        }
      ]
    }
  }
}
```
