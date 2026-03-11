# Qt Multi Player

基于 Qt Widgets、Qt Multimedia 和 CMake 的多屏视频播放器。

主要能力：
- 按 `config.json` 或外部配置文件创建多屏窗口
- 每个窗口内按网格同时播放多个视频
- 支持视频片段循环播放
- 支持单个播放器“全窗口”放大
- 支持窗口“全屏”切换
- 支持悬浮控制条、进度拖动、音量弹窗

## 环境

- Qt: `C:\Qt\6.6.3\msvc2019_64`
- CMake: `>= 3.21`
- 编译器: Visual Studio 2019 MSVC

## 配置文件

默认运行配置使用：

```text
C:\Users\groot\Documents\project\mutilplayer\config_local.json
```

配置格式示例：

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

## CMake Presets

已提供两个 preset：

- `vs2019-debug`
- `vs2019-release`

## Debug 构建与运行

```powershell
cmake --preset vs2019-debug
cmake --build --preset vs2019-debug
ctest --preset vs2019-debug
```

可执行文件：

```text
build/vs2019-debug/Debug/QtMultiPlayer.exe
```

## Release 构建与打包

```powershell
cmake --preset vs2019-release
cmake --build --preset vs2019-release
cmake --build build/vs2019-release --config Release --target package_release
```

输出目录：

- Release 可执行文件：`build/vs2019-release/Release`
- 发布目录：`dist/release`

`package_release` 会自动：
- 安装 `QtMultiPlayer.exe`
- 拷贝 `README.md`
- 拷贝 `config.json`
- 调用 `windeployqt` 收集 Qt 运行时依赖

## 编译开关

### 平滑窗口拖动

```powershell
-DQT_MULTIPLAYER_SMOOTH_WINDOW_DRAG=ON
```

- `ON`: 标题栏手动拖动，拖动窗口时画面更平稳
- `OFF`: 使用原生 `HTCAPTION` 拖动

示例：

```powershell
cmake --preset vs2019-release -DQT_MULTIPLAYER_SMOOTH_WINDOW_DRAG=OFF
```

## VS Code

推荐扩展：

- `ms-vscode.cmake-tools`
- `ms-vscode.cpptools`

仓库已包含：

- `CMake Tools` 状态栏 `Debug` 配置
- `Debug` / `Release` 启动配置
- `Debug` / `Release` 构建任务
- `Package Release` 打包任务

常用方式：

- `Ctrl+Shift+B`: 默认执行 Debug 构建
- 运行 `CMake: build release`: 执行 Release 构建
- 运行 `Package Release`: 生成发布目录
- 运行 `CTest` / `CTest Release`: 执行测试
- 在调试面板选择 `Debug QtMultiPlayer` 或 `Release QtMultiPlayer` 直接启动

## 测试

当前包含两类 CTest：

- `config_test`: 验证配置文件可被正确加载
- `app_smoke_test`: 以 `offscreen` 模式短暂启动程序，验证构建产物可运行

## 说明

- 当前默认媒体后端为 `windows`
- 当前默认音频输出为静音，可通过控制条音量按钮调整
- 多路同时播放时，性能主要受视频编码格式、分辨率、并发数量和 Qt Multimedia 后端影响
