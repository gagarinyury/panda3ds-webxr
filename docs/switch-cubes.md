# Switch в браузере из готовых кубиков (2026-10-17)

Вопрос: можно ли на нашей идее (Dynarmic IR → WASM, уже работает для A32/3DS) сделать Switch, не опираясь на Voland/Ballistic.
Метки: ✅ проверено на источнике (мной или агентом, ссылка), 🔍 догадка/не проверено. «Я» = проверено лично 2026-10-17 через API/страницы.

## Короткий ответ

Готового ARM64 → WASM для Switch нет ни у кого ✅ (поиск по Eden/Citron/Ryubing, Voland, qemu-wasm, elfconv, Vita3K-web). Но почти всё остальное есть готовым. Свой код нужен для двух слоёв: `emit_a64` (CPU) и WebGPU-рендерер Maxwell (+ эмуляция geometry/tessellation).

## Слой 1. CPU: ARM64 → WASM

| Кубик | Что даёт | Статус |
|---|---|---|
| **Dynarmic A64-фронтенд** (у нас в форке `frontend/A64`, ~10,7 тыс. строк) | декодер + IR для AArch64, тот же IR, который уже ест наш A32-бэкенд | ✅ локально; соберётся ли под wasm32 — 🔍 |
| **Dynarmic в дереве Eden** (`src/dynarmic/src/dynarmic/backend/`: arm64, x64, riscv64, loongarch64) | живой Dynarmic с A64, правки почти ежедневно (я: последний коммит Eden 2026-10-16). Наш `backend/wasm` встаёт пятым в ту же папку | ✅ я |
| Dynarmic в дереве Citron Neo (`externals/dynarmic`, backends: arm64, loongarch64, riscv64, x64) | то же | ✅ я |
| **Vita3K-web** (codeberg.org/SadsArches/vita3kweb, коммит 2026-06-19, GPLv2) | **самый близкий аналог нашего**: ARMv7-интерпретатор с NEON на WASM SIMD128, сверка бит-в-бит с эталоном, опциональный ARM→WASM JIT (`src/jit/arm_to_wasm.cpp` 49 КБ, `wasm_emitter.h`), WebGPU, **модель памяти «страничная таблица» из-за невозможности 4 ГиБ плоско в wasm32**, потоки на Workers поверх shared Memory | ✅ я (README и листинг `src/jit`); скорость не заявлена |
| **qemu-wasm** (ktock, push 2026-02-06) | aarch64 TCG→wasm: блок = модуль, горячие (~1000 исполнений) в wasm, холодные в TCI. Только system-режим, «experimental» | ✅ агент; дата я |
| **elfconv** (yomaytk, Apache-2.0, push 2026-10-01) | AOT AArch64 ELF → remill → LLVM → wasm, 56–96 % от нативного wasm на трёх бенчмарках; только статические ELF | ✅ дата/лицензия я, цифры агент |
| **remill** (Apache-2.0) | семантика AArch64 в LLVM IR, как справочник | ✅ агент |
| **binarrow** (crutchcorn) | идея: tier-0 интерпретатор + tier-1 блоки в WASM с v128, P-code | 🔍 ссылка 404 у одного агента, второй видел только поисковую выдачу |
| **Ballistic** (pound-emu, GPL-2.0, push 2026-10-13) | 4 из 25 целей, только x86, WASM нет, NEON нет | ✅ я |
| flycast-wasm (GPL-2.0, push 2026-08-01) | приёмы: слияние блоков, детект SMC по страницам, кэш в IndexedDB | ✅ я |

**Вердикт по «лучше Динармика»:** по зрелости A64-фронтенда лучше нет. Альтернативы (Ballistic, remill, elfconv, QEMU) либо сырые, либо AOT, либо не имеют A64-фронтенда такого качества. Dynarmic остаётся, меняем только вывод. Риск: апстрим `lioncash/dynarmic` архивирован (2024-03), живые линии — Eden и Citron Neo (в дереве), azahar (A32/3DS). Брать копию из Eden, не из архива.

### Тесты и эталоны для A64 (бесплатные)
- Dynarmic `tests/A64`, `tests/unicorn_emu`, `fuzz_util`, `test_generator` — дифф-фаззинг против Unicorn (форк azahar, 0BSD) ✅ агент.
- QEMU `tests/tcg/aarch64` (состав ✅, лицензия 🔍).
- **Sail ARM** (`rems-project/sail-arm`, push 2026-10-13, лицензия «Other» — читать) — полная формальная семантика ISA, эталон для NEON/FP ✅ я (путь: rems-project, не ARM-software).
- Готового набора SingleStepTests для ARM64 нет ✅ агент.

## Слой 2. Ядро эмулятора (kernel/HLE/сервисы)

| Кубик | Заметки |
|---|---|
| **Eden** (git.eden-emu.dev, GPL-3.0+, C++, коммит 2026-10-16, Win/Linux/macOS/Android) | полный yuzu-стек. Живёт на собственном Forgejo ✅ я. В Eden и Citron нет ни issue, ни PR про wasm/Emscripten ✅ агент → сборка под Emscripten 🔍 |
| **Citron Neo** (github.com/citron-neo/emulator, GPL-3.0, push 2026-10-15) | **есть `src/ios`**: `jit_icache_callbacks_ios.cpp`, entitlements, App/, emu_window ✅ я; рабочесть 🔍. Только Vulkan. Живёт на GitHub, риск DMCA |
| **Ryubing/Ryujinx** (MIT, C#) | чистая лицензия, но C#; не проверял (хост недоступен) 🔍 |
| **Voland** (GPL-2.0, коммит 2026-09-22) | маленький C11-скелет с нуля: softmmu `vmm.c`, загрузчики расшифрованных NCA/ExeFS/RomFS/NSO, TLS, SVC памяти. Гостя не исполняет. Заголовков лицензий в файлах нет, README просто «GPL-2.0» → считать **GPL-2.0-only**, 🔍; смешивать с GPL-3 кодом нельзя |
| Atmosphère `libmesosphere` (GPLv2) | исходники ядра Horizon — справочник по поведению SVC |
| switchbrew wiki: SVC (~127 ID), NRO, NSO | спецификации ✅ агент (лицензия контента не указана) |
| nstool (MIT), hac-rs (Apache/MIT) | парсеры NRO/NSO/NPDM/PFS0 без ключей 🔍 |
| HWDER (sonsegajp, push 2026-10-14, лицензия в API NOASSERTION) | статическая рекомпиляция одного тайтла в C + свой рантайм Horizon/GPU/аудио, Windows. **Только как чтение**: репо тянет prod.keys и расшифровку, нам такое нельзя (правило Voland №1 разумно копировать) ✅ я |
| Skyline | мёртв: код удалён по требованию Nintendo (2026-08-18) ✅ агент |

### Гостевые тесты (легальные)
- **Atmosphère/tests**: TestSvc, TestFs, TestOsEvents, TestSocket, TestStack (GPLv2) ✅ я (листинг).
- **switch-examples** (switchbrew), **libnx** (ISC), **nx-hbmenu**, образ `devkitPro/docker` (devkitA64) ✅ агент.

## Слой 3. GPU

| Кубик | Заметки |
|---|---|
| `shader_recompiler` в Eden: Maxwell → IR → SPIR-V (через sirit) | отдельная статическая библиотека, но зависит от `common` (GPL) ✅ агент. Лицензия GPL-3+ → весь проект GPL-3 |
| Транслятор шейдеров Ryujinx (MIT, C#) | чистая лицензия, но язык другой ✅ агент |
| **naga** (v30.0.1, MIT/Apache): SPIR-V → WGSL / GLSL ES | Vita3K-web использует именно этот путь, работает ✅ я. Execution model: только Vertex/Fragment/Compute — **geometry и tessellation нет** ✅ агент по исходникам |
| SPIRV-Cross (Apache) | → GLSL ES для WebGL2-запаса, в WGSL не выводит ✅ |
| Tint (Dawn, BSD-3) | SPIR-V reader ограничен, запасной вариант 🔍 |
| Vita3K-web `vita3k-port/webgpu/` | образец WebGPU-рендерера консольного GPU в браузере ✅ я |
| Mesa NVK, deko3d (Zlib) | справочник по Maxwell/NVN, не код для переиспользования ✅ агент |

Ограничения WebGPU (✅ спецификация, агент): 4 bind group по умолчанию (WebKit даёт 11), буфер 256 МиБ, нет geometry/tessellation, нет i64-атомиков. На iOS ASTC/ETC2 есть, BC зависит от устройства 🔍 — перекодировать. Geometry/tessellation Switch придётся эмулировать через compute (самый большой кусок собственного кода).

## Что собирать (предложение)

1. CPU: `emit_a64.cpp` в нашем Dynarmic-форке (база — дерево Eden), iOS-путь отлажен в WKWebView. Тесты: Dynarmic test_generator + Unicorn; NEON/FP сверять с Sail ARM. Tier-0 интерпретатор по схеме Vita3K-web, чтобы не ждать JIT на холодном коде.
2. Память: страничная таблица (Vita3K-web, Voland `vmm`), 39-битное адресное пространство разреженно. Потолок WebContent на iPhone ~1,9 ГБ (наш замер) → часть игр на iPhone не влезет, на десктопном браузере — вероятно да 🔍.
3. Ядро: два пути. (а) Eden как база — максимум готового, но порт на Emscripten с потоками/Vulkan = неизвестный объём 🔍; (б) Voland-скелет + дописывать HLE самим — GPL-2.0-only конфликтует с нашим GPL-3. Склоняюсь к (а), но сначала спайк «собирается ли Eden `core/` без Vulkan под Emscripten».
4. GPU: shader_recompiler → naga → WGSL + WebGPU-рендерер по образцу Vita3K-web; geometry/tessellation через compute.

## Риски
- **Юридический**: за 2026 Nintendo подала DMCA на yuzu/Citron/Eden-форки и зеркала (февраль) и ~300 форков Suyu (август, github/dmca 2026-08-17) ✅ агент. Eden на своём Forgejo, Citron Neo на GitHub. Публиковать только код без ключей/прошивок; решение за вами.
- **Объём**: CPU — самое дешёвое, Horizon-сервисы и Maxwell — на порядок дороже, чем PICA у 3DS.
- Не проверено: компиляция A64-фронтенда под wasm32, сборка Eden core и shader_recompiler под Emscripten, работа iOS-порта Citron Neo, скорость A64→WASM.

## Следующий шаг (дёшево)
Спайк на 1–2 дня: собрать `frontend/A64` под wasm32 (как A32: правки `size_t` и приведения указателей), прогнать маленькое ядро, собранное devkitA64, через `emit_a64` с одними целочисленными опкодами; параллельно проверить, собирается ли `core/` Eden без Vulkan под Emscripten.
