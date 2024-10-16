# 🚀 Windows Terminal: The Ultimate Command-Line Experience

![Windows Terminal Logo](https://github.com/microsoft/terminal/assets/91625426/333ddc76-8ab2-4eb4-a8c0-4d7b953b1179)

[![Build Status](https://dev.azure.com/shine-oss/terminal/_apis/build/status%2FTerminal%20CI?branchName=main)](https://dev.azure.com/shine-oss/terminal/_build/latest?definitionId=1&branchName=main)

Welcome to the future of command-line interfaces! Windows Terminal is a modern, fast, and customizable terminal application built for command-line users and developers alike.

## 🌟 Features

- 📑 Multi-tab support
- 🎨 Themes and customization
- 🌐 Unicode and UTF-8 character support
- 😄 Emoji support
- ⚡ GPU-accelerated text rendering
- 🔌 Extensibility and profiles

## 🚀 Quick Start

### Requirements

- Windows 10 2004 (build 19041) or later

### Installation Options

1. **Microsoft Store (Recommended)**
   - [Download Windows Terminal](https://aka.ms/terminal)
   - Automatic updates included!

2. **Manual Installation**
   - Download from [Releases](https://github.com/microsoft/terminal/releases)
   - Double-click the `.msixbundle` file

3. **Package Managers**
   - Winget: `winget install Microsoft.WindowsTerminal`
   - Chocolatey: `choco install microsoft-windows-terminal`
   - Scoop: `scoop install windows-terminal`

## 🛠️ For Developers

### Prerequisites

- Windows 10 2004 or later
- Visual Studio 2022
- Windows 11 SDK (10.0.22621.0)
- PowerShell 7+

### Building from Source

1. Clone the repository:
   ```
   git clone https://github.com/microsoft/terminal.git
   cd terminal
   git submodule update --init --recursive
   ```

2. Open `OpenConsole.sln` in Visual Studio 2022

3. Build the `CascadiaPackage` project

For detailed instructions, check out our [Contributor's Guide](./CONTRIBUTING.md).

## 📚 Documentation

Visit [aka.ms/terminal-docs](https://aka.ms/terminal-docs) for comprehensive documentation.

## 🤝 Contributing

We welcome contributions! Please read our [Contributor's Guide](./CONTRIBUTING.md) before getting started.

## 📣 Stay Connected

- 📝 [Command-Line Blog](https://devblogs.microsoft.com/commandline)
- 🐦 Twitter: [@nguyen_dows](https://twitter.com/nguyen_dows), [@dhowett](https://twitter.com/DHowett), [@cazamor_msft](https://twitter.com/cazamor_msft)

## 📜 Code of Conduct

This project follows the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).

## 🙏 Acknowledgements

A big thank you to all our contributors and the open-source community!

---

Ready to revolutionize your command-line experience? Install Windows Terminal today and join us in shaping the future of terminal applications! 🚀🖥️
