<div align="center">

# 🗂️ Lilolify

### AI-Powered Intelligent Digital File Organization System

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg?style=flat-square&logo=cplusplus)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C.svg?style=flat-square&logo=cmake)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg?style=flat-square&logo=windows)](https://www.microsoft.com/windows)

**Lilolify doesn't just detect objects in images — it reasons about their *purpose* and organizes them like an intelligent human.**

</div>

---

## 🧠 How It Thinks

Traditional file organizers classify images by content: *"This image contains a card."*

Lilolify asks deeper questions:

| Traditional | Lilolify |
|---|---|
| "What is in this image?" | "**Why** does this file exist?" |
| "Detected: card, text" | "**Purpose**: Identity verification" |
| → `Images/Cards/` | → `Documents/Government IDs/Aadhaar/` |

Every file goes through an AI reasoning pipeline that determines its **purpose**, **category**, and **optimal folder location** — just like a meticulous human organizer would.

---

## 🏗️ Architecture

Lilolify uses **Clean Architecture** with strict layer separation:

```
┌─────────────────────────────────────────┐
│     Presentation (Dear ImGui / GLFW)    │   ← Lightweight 60 FPS Desktop UI
├─────────────────────────────────────────┤
│          Application (Use Cases)        │   ← Orchestration & Job Coordinator
├─────────────────────────────────────────┤
│        Domain (Entities & Rules)        │   ← Pure C++20 business logic
├─────────────────────────────────────────┤
│  Infrastructure     │   AI Service      │   ← Database, File Systems,
│  (SQLite, FS)       │   (OpenAI, OCR)   │     and REST Adapters
└─────────────────────────────────────────┘
```

Dependencies always point **inward**. The domain layer has zero external dependencies, making it 100% testable and decoupled.

---

## 🛠️ Software Engineering & Design Patterns

Lilolify is built with a focus on modern C++ engineering best practices and architectural design patterns that recruiters look for:

- 🧱 **Dependency Injection (DI)**: Injected abstract interfaces (`IOcrEngine`, `IAiProvider`, `IFileOrganizer`) allow the system to switch backends (e.g. mock engines for unit testing and real Curl/Tesseract implementations in production) without rewriting core logic.
- 🏭 **Factory Pattern**: The `AiProviderFactory` creates specific provider adapters (OpenAI, Gemini, Claude, or Custom Gateways) dynamically at runtime based on the user's settings.
- 📂 **Port-Adapter / Hexagonal concepts**: Business rules communicate with external resources (the network, database, and filesystem) via abstract interfaces, ensuring the core remains decoupled from concrete libraries like `SQLite3`, `libcurl`, or `Tesseract`.
- 🔄 **Command & Ledger Transactions**: Operations are logged in a LIFO transaction database table. Users can undo a full folder reorganization session safely with a single click, restoring the filesystem to its exact original state.
- ⚡ **Multimodal Vision Integration**: Supports binary file streams and Base64 visual payload extraction, enabling native AI vision analysis on local screenshots and camera photos.

---

## 🔌 AI Provider Abstraction

Lilolify supports multiple AI vision providers through a clean abstraction:

- ✅ **OpenAI** GPT-4 Vision
- ✅ **Google Gemini** Vision
- ✅ **Anthropic Claude** Vision
- 🔜 **Local AI** models (future)

Switching providers requires changing **one configuration value** — no code changes.

---

## ⚙️ Technology Stack

| Component | Technology |
|---|---|
| Language | C++20 (RAII, Smart Pointers, Concepts) |
| Build | CMake 3.25+ with Presets |
| Dependencies | vcpkg (manifest mode) |
| UI | Dear ImGui (GLFW + OpenGL3) |
| Database | SQLite |
| HTTP | libcurl |
| JSON | nlohmann/json |
| OCR | Tesseract |
| Logging | spdlog |
| Testing | GoogleTest + GoogleMock |

---

## 🚀 Building from Source

### Prerequisites

- C++20 compatible compiler (MSVC 19.30+, GCC 12+, or Clang 15+)
- CMake 3.25+
- Ninja (recommended) or Visual Studio
- vcpkg (optional — FetchContent fallback available)

### Build

```bash
# Configure (Debug)
cmake --preset debug

# Build
cmake --build --preset debug

# Run tests
ctest --preset debug
```

---

## 📁 Project Structure

```
lilolify/
├── cmake/           # CMake modules (warnings, sanitizers)
├── src/
│   ├── core/        # Domain Layer (zero external deps)
│   ├── app/         # Application Layer (use cases)
│   ├── infra/       # Infrastructure (SQLite, filesystem)
│   ├── ai/          # AI Service Layer (providers, OCR)
│   └── ui/          # Presentation (Dear ImGui)
├── tests/
│   ├── unit/        # Unit tests per layer
│   ├── integration/ # Cross-layer tests
│   └── performance/ # Benchmarks
└── docs/            # Architecture docs & diagrams
```

---

## 📋 Development Phases

| Phase | Component | Status |
|---|---|---|
| 1 | Project Architecture | ✅ Complete |
| 2 | Folder Scanner | ✅ Complete |
| 3 | Metadata Engine | ✅ Complete |
| 4 | OCR Integration | ✅ Complete |
| 5 | AI Provider Abstraction | ✅ Complete |
| 6 | Reasoning Engine | ✅ Complete |
| 7 | Decision Engine | ✅ Complete |
| 8 | Folder Manager | ✅ Complete |
| 9 | Undo System | ✅ Complete |
| 10 | Database | ✅ Complete |
| 11 | Search | ✅ Complete |
| 12 | Live Folder Monitor | ✅ Complete |
| 13 | Desktop UI | ✅ Complete |
| 14 | Optimization | ✅ Complete |
| 15 | Packaging | ✅ Complete |

---

## 🔒 Privacy

Privacy is a core design principle:

- 🚫 **No files are uploaded** without explicit user permission
- 🏠 **Local AI** option for fully offline operation (future)
- 🔔 **Clear indicators** when files are sent to cloud AI providers
- 💾 **All data stays local** in SQLite database

---

## 📄 License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
