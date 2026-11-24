# cardputer-zero-tools — контекст проєкту

Відповідай українською. Власник — Sebastian, бекенд-розробник; любить структуровані відповіді з чіткими наступними кроками.

## Що це
Мультирепо нативних застосунків для M5Stack CardputerZero (CM0, Debian 13 trixie arm64, 512 MB RAM, екран 320×170,
46 клавіш, без тачу). C++17 + LVGL 9.5, `.deb` для лаунчера APPLaunch. GitHub: `git@github.com:heartpolik/cardputer-zero-tools.git`,
`upstream` = офіційний `CardputerZero/Template` (від нього `common/`).

## Структура
- `common/` → бібліотека `czkit`: `Runtime` (дисплей fbdev/SDL, тема + `~/.config/<app>/settings.conf`, toast, цикл, `CZ_SCRIPT`, `CZ_ASSETS`),
  `platform/key_decoder` (чистий, тести), `linux_input` (фази Press/Repeat/LongPress/Release, автоповтор наш, kernel repeat ігнорується).
- `apps/<app>/`: `core/` (чистий C++ без LVGL + `tests/`), `src/` (UI), `assets/`, `demo.script` (скріншоти README).
- Збирається один застосунок за раз: `-DCZ_APP=<app>` (CPack на проєкт). `cmake/cz-package.cmake` → `cz_package_app()`.
- `scripts/cz sim|test|shots|deb|push|run`.

## Пристрій
- `pi@192.168.1.215`, SSH по ключу. `sudo` без пароля лише для `apt-get`/`dpkg` (`/etc/sudoers.d/cz-deploy`) → `scripts/cz push` працює неінтерактивно.
- Без встановлення: розпакувати `.deb` у `~/cz-test`, запуск `CZ_ASSETS=~/cz-test/usr/share/cz-<app> ~/cz-test/usr/bin/cz-<app>`.
  При запуску по ssh застосунок малює поверх лаунчера (лаунчер ділить клавіатуру й fb).
- Клавіатура `tca8418c` (`/dev/input/event3`): Sym-символи драйвер шле кодами з vendor-мапи (72 = `+`, 92 = `/`…) — шукати до US-розкладки,
  лише для вбудованої клавіатури. Fn драйвер застосовує сам (стрілки = Fn+F/X/Z/C, F1..F10 = Fn+цифри, Help = Fn+H, PrtSc = Fn+J).
  Shift — звичайний `KEY_LEFTSHIFT` (одноразовий режим: драйвер тримає LSHIFT down навколо наступної клавіші; фіксація — до виходу).
  Сам Fn шле код 389 (скан 66), Sym — 383 (скан 67); обидва декодер ігнорує. Перевірено на залізі 2026-11-24
  (скани: `EVIOCGKEYCODE`, проба `/tmp/keyprobe.py`). Стан модифікаторів також у `m5ioe1` I2C 0x4F, рег. 0xBD/0xBF/0xBE
  (Shift/Sym/Fn; 0=off, 1=одноразовий, 3=утримання) — не потрібен, бо драйвер застосовує шари сам.
  Esc: коротко — app, довго — HOME (лаунчер) / quit (наш Runtime).
- Іконки лаунчера: `/usr/share/APPLaunch/share/images/<name>.png` (~100×100), `.desktop` у `/usr/share/APPLaunch/applications/`.

## Підводні камені
- `lv_delay_ms()` без delay-callback — busy-wait (~90% CPU). У циклі `sleep_for` (вже в `Runtime::run`).
- Mono.framework на Маку підсовує старий `png.h` → `CMAKE_FIND_FRAMEWORK=LAST` (локальний `CMakeUserPresets.json`, пресет `mac`).
- GCC у Docker суворіший за clang (наприклад, `std::max({...})` потребує `<algorithm>`) — перевіряти `scripts/cz deb`.
- ASan на цьому macOS падає на старті → тести з `-fsanitize=undefined`.
- Переглядач скріншотів у Claude кешує за шляхом — для повторного перегляду копіювати під новим іменем.

## CalcPuter (apps/calc, пакет `cz-calc`)
- `core/expr` (лексер/Pratt-парсер/AST/eval, спецформи `der/integ/solve/sum` по `x`), `format` (AUTO/SCI/ENG), `engine` (історія лише успішних,
  `ans`, змінні/функції, 4 слоти графіків, серіалізація в `~/.config/cz-calc/state.txt`), `line_editor` + `prepare_input` (`*2`→`ans*2`, автодужки),
  `plot_math` (viewport, nice ticks, корені, екстремуми з уточненням по f'=0).
- UI: `CalcApp` (статус-бар, Tab між CALC/Y=/GRAPH, F1 help, F2 DEG/RAD, F3 формат, F4 тема, F5 очистити історію),
  `GraphPage` малює в буфер canvas сам (Bresenham), не примітивами LVGL.
