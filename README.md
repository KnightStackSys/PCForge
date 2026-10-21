<div align="center">

# 🖥️ PCForge

**A native C++ desktop application for designing, validating, importing, and generating PC builds.**

C++20 · Qt 6 Widgets · CMake · Offline-first · Optional Codex CLI

</div>

## ✨ Features

- 🧩 **Parts library:** browse, search, add, edit, and delete components.
- 🧰 **Build editor:** select one component per category, calculate total price, and save/load a JSON build.
- 🛡️ **Compatibility checks:** CPU-to-motherboard socket, motherboard RAM type, motherboard/case form factor, cooler socket, GPU length, cooler height, and estimated PSU headroom.
- 📋 **PCPartPicker import:** paste a copied text/Markdown part list to import names and listed prices, then edit missing specifications.
- ⚙️ **Offline builder:** enter a budget and a prompt (gaming, workstation, quiet); generate the highest-scoring compatible configuration from your local catalog.
- ✨ **Optional Codex:** use a locally installed Codex CLI to select catalog item IDs, then validate the result with PCForge before accepting it.
- 💾 **Local-first:** catalog and last build are stored in your platform application-data directory.

> [!IMPORTANT]
> The bundled parts are **fictional examples with illustrative prices and relative performance scores**, not retailer listings. They do not represent measured benchmarks or verified stock. The app does **not** scrape PCPartPicker or query real-time prices. Imported hardware can have unknown specifications, which means compatibility is not guaranteed. Always verify BIOS versions, memory QVL, physical clearance, connectors, cooling, electrical requirements, and manufacturer data before purchasing.

## 🚀 Build locally

**Prerequisites:** C++20 compiler, CMake 3.21+, and **Qt 6.4+** with the Widgets development module.

On Windows, install Qt 6 with the MSVC kit matching your Visual Studio installation, then use the Qt-enabled Developer PowerShell:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" -DPCFORGE_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\PCForge.exe
```

Adjust the Qt path/version and executable output path for your environment. You may also open `CMakeLists.txt` directly in Qt Creator or Visual Studio with Qt installed.

Ubuntu/Debian (with a Qt 6.4+ package):

```bash
sudo apt-get install cmake g++ qt6-base-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/PCForge
```

For a distributable Windows application, run Qt's `windeployqt` on the executable and comply with Qt's relevant license requirements.

## 🧱 Build workflow

1. Select components from the **Hardware Library**, or add custom entries with a name, USD price, performance score (0–100) and a JSON specifications object.
2. Double-click a component or click **Use selected part in build**.
3. Review the live compatibility report. An incomplete report is **not** a certificate of physical compatibility.
4. Enter a natural-language description and budget; use **Generate Offline** to get a proposed configuration.
5. Save or open build files as JSON. The most recent build is also stored locally.

### Sample component specifications

The custom-part dialog accepts a JSON object. Use only fields you know; unknown values should be omitted rather than guessed.

```json
{
  "socket": "AM5",
  "ram_type": "DDR5",
  "form_factor": "ATX",
  "form_factors": "ATX,mATX,Mini-ITX",
  "sockets": "AM5,LGA1700",
  "power_w": 105,
  "capacity_w": 850,
  "gpu_length_mm": 310,
  "max_gpu_length_mm": 360,
  "cooler_height_mm": 155,
  "max_cooler_height_mm": 170
}
```

Fields irrelevant to a component can be omitted. PSU headroom is approximated as **(CPU power + GPU power + 75W) × 1.25**. This is an estimate, not an electrical certification.

### 📋 Import from PCPartPicker

On PCPartPicker, copy the **text/Markdown part list**, then paste it into **Import PCPartPicker List**. The importer recognizes the categories CPU, CPU Cooler, Motherboard, Memory, Storage, Video Card, Case, and Power Supply. It extracts names and visible dollar amounts, creates custom catalog entries for unfamiliar items, and selects them in the current build.

Example:

```text
CPU | [Example Processor](https://example.com/cpu) | $299.99
Motherboard | [Example Board](https://example.com/board) | $179.99
```

Automatic ingestion from arbitrary PCPartPicker URLs and bulk catalog scraping are deliberately not implemented. Consider authorized data access or a licensed parts feed before adding those features.

### ✨ Optional Codex CLI

The **Generate with Codex CLI** button requires the `codex` command to be installed and authenticated separately. PCForge starts `codex exec` through `QProcess`, using **read-only sandboxing**. It sends the user's prompt, budget, and local catalog and asks Codex to return JSON mapping each part category to an existing catalog ID.

PCForge checks every returned ID, category, the current compatibility rules, and the budget **before** replacing the user's build. Invalid suggestions are rejected. Codex is optional: local generation works without an account, internet access, or API key.

The CLI is a separate executable; this project does not bundle Codex, provide subscription access, or store any API keys. The catalog and prompt are shared with the chosen CLI/model when Codex mode is used.

## 🧪 Tests and CI

```bash
cmake -S . -B build -DPCFORGE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Qt Test covers catalog persistence, invalid-file protection, incompatible sockets, insufficient PSU capacity, case clearance, budget handling, optimization, and importing. A GitHub Actions workflow runs the Linux build and core tests on pull requests.

## 🗺️ Future roadmap

- Verified retailer feeds with region-aware currencies and price timestamps.
- Better hardware metadata (BIOS, memory speeds/QVL, connector counts, PCIe, cooler/radiator mounts and chassis constraints).
- More robust compatibility graph and performance benchmarks.
- Multi-part slots, multiple GPUs/drives, build comparisons and upgrade paths.
- Dedicated Codex app-server integration with session control.
- Installers, signed binaries and automated Windows builds.

## 📄 License

Apache-2.0. See [LICENSE](LICENSE).
