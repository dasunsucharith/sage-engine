# Contributing to SAGE Engine

Thank you for helping build **SAGE** (**S**criptable **A**I **G**ame **E**ngine)! This is a long-term, open-source learning and engineering project — beginners, veterans, and AI-assisted developers are all welcome.

## How to Contribute

1. **Fork** the repository on GitHub and create a feature branch:
   ```bash
   git checkout -b feat/your-feature-name
   ```
2. Read [`AGENTS.md`](AGENTS.md) and [`docs/architecture.md`](docs/architecture.md) to match project conventions.
3. Keep pull requests focused on **one clear feature or fix** (e.g., *"Add SDL3 window creation"* or *"Add `world.spawnModel` Lua binding"*).
4. If you add a new Lua API or engine subsystem, update [`docs/lua_api.md`](docs/lua_api.md) or [`docs/knowledge-base/`](docs/knowledge-base/).
5. Open a **Pull Request** explaining what you built and how to test it.

## Coding Style

* **C++20** with clear, readable names (`PascalCase` for types/classes, `camelCase` for functions/variables, `m_` prefix for private class members).
* Prefer clarity over clever tricks — this codebase doubles as a learning resource for aspiring engine developers!
* **Lua scripts** follow clean FiveM-style event callbacks (`onStart`, `onTick`, `onShutdown`).
