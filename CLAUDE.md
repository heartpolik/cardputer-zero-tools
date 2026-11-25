# cardputer-zero-tools — контекст проєкту

Відповідай українською. Власник — Sebastian, бекенд-розробник; любить структуровані відповіді з чіткими наступними кроками.

## Що це
Мультирепо нативних застосунків для M5Stack CardputerZero (CM0, Debian 13 trixie arm64, 512 MB RAM, екран 320×170,
46 клавіш, без тачу). C++17 + LVGL 9.5, `.deb` для лаунчера APPLaunch. GitHub: `git@github.com:heartpolik/cardputer-zero-tools.git`,
`upstream` = офіційний `CardputerZero/Template` (від нього `common/`).

## Структура
- `common/` → бібліотека `czkit`: `Runtime` (дисплей fbdev/SDL, тема + `~/.config/<app>/settings.conf`, toast, цикл, `CZ_SCRIPT`, `CZ_ASSETS`),
  `platform/key_decoder` (чистий, тести), `linux_input` (фази Press/Repeat/LongPress/Release, автоповтор наш, kernel repeat ігнорується).
- `common/core/` → `czcore` (чистий C++: `cz::LineEditor`, `cz::prepare_input`, `bytes.h`: `parse_bytes` + CRC-специфікації — перенесено з BitPuter, там лишились using-аліаси в `checksum.h`), лінкується в ядра застосунків.
  `common/src/view/ui_style` → `cz::Style` (палітра dark/light + JetBrains Mono з `common/assets/fonts`, ставиться в кожен пакет).
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

## Клавіші: домовленості CardputerZero (з офіційних launcher і Store)
- F/X/Z/C без Fn = стрілки вгору/вниз/вліво/вправо (launcher docs, «F/X/Z/C Direction-Key Convention»), але лише там,
  де немає введення тексту (у калькуляторах і терміналі — лише Fn+F/X/Z/C).
- Клавіші 4–8 стоять під дисплеєм: `cz::ShortcutBar` (`common/src/view/shortcut_bar`) малює підписи над ними як Store
  (`appstore_shortcut_bar.cpp`): центри x = 48/104/160/216/272, кольори 4 жовтий, 5 синій, 6 зелений, 7 фіолетовий,
  8 помаранчевий; 4 — «назад», 8 — головна дія; недоступне сіре. Не для екранів, де цифри — введення (калькулятори, BANDS, MODBUS).
  Зараз: SerialPuter PORT (4 ports, 6 log, 8 open/close), DiscPuter.
- Шари: `platform::ModifierWatch` (у Runtime, опитування кожні 80 мс) читає стан Shift/Fn/Sym з M5IOE1 (I²C 0x4F, рег. 0xBD/0xBE/0xBF,
  0 off / 1 once / 3 lock; `I2C_SLAVE_FORCE` + лише SMBus byte reads, працює від `pi` у групі i2c). `Runtime::set_layer_handler` →
  `ShortcutBar::show_layer`. Підписи на шар: `set(key, text, enabled, KeyLayer::Fn)`; без підпису показується, що клавіша друкує
  (Shift і Sym обидва `$ % ^ & *`: Sym шле коди 40/41/43/51/52, які `builtin_symbol` перетворює на ті самі символи;
  тож розрізнити можна лише 3 набори: цифри, `$%^&*`, `F4..F8`). `set_auto_hide(true)` — рядок лише поки шар активний
  (калькулятори, SerialPuter). Десктоп: Shift/Ctrl(=Fn)/Alt(=Sym) з SDL; скрипти: `{layer:fn}`, `{layer:sym!}` (зафіксований), `{layer:base}`.
- Grove 5 V і режим Grove керуються LED-класом: `/sys/class/leds/grove_5v_out` (за замовч. 0 = вимкнено!), `grove_fun`,
  `ext_5v_out`, `ext_usb_gpio_fun` (група gpio може писати).

## Підводні камені
- `lv_delay_ms()` без delay-callback — busy-wait (~90% CPU). У циклі `sleep_for` (вже в `Runtime::run`).
- Mono.framework на Маку підсовує старий `png.h` → `CMAKE_FIND_FRAMEWORK=LAST` (локальний `CMakeUserPresets.json`, пресет `mac`).
- GCC у Docker суворіший за clang (наприклад, `std::max({...})` потребує `<algorithm>`) — перевіряти `scripts/cz deb`.
- ASan на цьому macOS падає на старті → тести з `-fsanitize=undefined`.
- Переглядач скріншотів у Claude кешує за шляхом — для повторного перегляду копіювати під новим іменем.

## CalcPuter (apps/calcputer, пакет `cz-calcputer`)
- `core/expr` (лексер/Pratt-парсер/AST/eval, спецформи `der/integ/solve/sum` по `x`), `format` (AUTO/SCI/ENG), `engine` (історія лише успішних,
  `ans`, змінні/функції, 4 слоти графіків, серіалізація в `~/.config/cz-calcputer/state.txt`), `line_editor` + `prepare_input` (`*2`→`ans*2`, автодужки),
  `plot_math` (viewport, nice ticks, корені, екстремуми з уточненням по f'=0). Редактор рядка — з `czcore`.
- UI: `CalcApp` (статус-бар, Tab між CALC/Y=/GRAPH, F1 help, F2 DEG/RAD, F3 формат, F4 тема, F5 очистити історію),
  `GraphPage` малює в буфер canvas сам (Bresenham), не примітивами LVGL.

## BitPuter (apps/bitputer, пакет `cz-bitputer`)
- `core/word` (ширина 8/16/32/64, signed, формати HEX/DEC/OCT/BIN, `literal()` для вводу), `int_expr` (рекурсивний спуск, C-пріоритети,
  усе обгортається до слова; у HEX-режимі слово з hex-цифр = число, `0b` там — hex; `0n` = десяткове), `decode` (CHR/FLT/UTC),
  `checksum` (CRC за параметрами reveng, перевірено check-значеннями "123456789"), `session` (`~/.config/cz-bitputer/state.txt`).
- UI: BITS (ввід, 4 рядки значень, бітова сітка: Fn+X з вводу) і CRC. F2 база, F3 ширина, F4 signed, F5 декодування, F6 тема,
  F7 сітка бітів ↔ таблиця порядків байтів (`reorder`: ABCD/DCBA/BADC/CDAB; функції `bswap16`, `wswap`).

## OhmPuter (apps/ohmputer, пакет `cz-ohmputer`)
- `core/si` (`4k7`, `2u2`, `4R7`, юніти ігноруються; m = мілі, M = мега), `eseries` (E6–E96, nearest, пара для подільника),
  `marking` (кольорові смуги 3–6, IEC 60062; коди: 3/4-значні, EIA-96, RKM, конденсатори з напругою/допуском, танталові,
  невеликий список SMD-кодів діодів/транзисторів — може бути неповним), `tools` (`Form`: введені поля + порядок редагування,
  зайве найстаріше поле очищається; Ohm/DIV/LED/RC/LC/BATT), `session`.
- UI: MARK / BANDS / TOOLS; F2 E-ряд, F6 тема. Кольори вводяться цифрами (0 чорний … 9 білий, G золото, S срібло).

## SerialPuter (apps/serialputer, пакет `cz-serialputer`)
- `core/serial_format` (бод, 8N1…, час символу, Modbus-пауза 3,5 символу / 1750 мкс > 19200), `capture` (`Framer`: кадри за паузою
  з оцінкою часу першого байта чанку або за LF; `Capture` з лічильниками; вигляди HEX/TXT/DUMP, мітки часу, рядок логу),
  `modbus` (encode/decode FC 1–6/15/16, `Sniffer` з пам'яттю запитів за адресою, `split_frames` за CRC, `Master` з таймаутом,
  ігнором власного ехо і склеюванням розірваної відповіді, `Scanner`, `format_value` U16…F32 ABCD/CDAB), `sim_bus` (порт `SIM`:
  BMS #1, інвертор #16, AT-модем — для симулятора і скриншотів), `session` (`prepare_send`, налаштування).
- `src/serial_port` (termios, потік читання з `poll()` і мітками часу, `flock`; після `tcsetattr` формат читається назад),
  `engine` (порт + кадри + Modbus + лог, `tick()` з LVGL-таймера 2 мс), сторінки TERM / MODBUS / PORT. Лише читання по Modbus.
- Пристрій (перевірено 2026-11-25): UART заголовка = GPIO14 TX / GPIO15 RX = `/dev/serial0` → `ttyS0` (**mini UART**:
  лише 8N1/7N1, парність → EINVAL; PL011 зайнятий Bluetooth). Виведено на Grove (жовтий TX, білий RX, режим UART за замовч.,
  перемикач `G4` low = UART) і EXT пін 14 (TX) / 12 (RX). Консолі та getty на ньому немає, `pi` у `dialout`.
  Логіка 3,3 В; 5 В на Grove вмикає `PYG4_GROVE_EN` (M5IOE1). Вбудований ІЧ: `gpio-ir` GPIO13, `gpio-ir-tx` GPIO12.
