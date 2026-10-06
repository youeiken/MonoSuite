MonoLauncher/
├── platformio.ini
├── README.md
├── wifi.txt
├── wifi.txt.example
│
├── include/
│   └── (empty, or put truly global project-wide headers here)
│
├── lib/
│   ├── appmode/
│   │   ├── appmode.cpp
│   │   └── appmode.h
│   ├── appstate/
│   │   ├── appstate.cpp
│   │   └── appstate.h
│   ├── display/
│   │   ├── display.cpp
│   │   └── display.h
│   ├── ebook_decode/
│   │   ├── ebook_decode.cpp
│   │   ├── ebook_decode.h
│   │   ├── ebook_epub.cpp
│   │   ├── ebook_epub.h
│   │   ├── ebook_txt.cpp
│   │   └── ebook_txt.h
│   ├── fontconfig/
│   │   ├── fontconfig.cpp
│   │   └── fontconfig.h
│   ├── game2048/
│   │   ├── game2048.cpp
│   │   └── game2048.h
│   ├── launcher/
│   │   ├── launcher.cpp
│   │   └── launcher.h
│   ├── minesweeper/
│   │   ├── minesweeper.cpp
│   │   └── minesweeper.h
│   ├── miniz/
│   │   ├── library.json
│   │   ├── miniz_epub.h
│   │   ├── miniz.c
│   │   └── miniz.h
│   ├── ofr_sd_preset/
│   │   └── ofr_sd_preset.h
│   ├── reader/
│   │   ├── reader_font.cpp
│   │   ├── reader_font.h
│   │   ├── reader.cpp
│   │   └── reader.h
│   ├── reversi/
│   │   ├── reversi.cpp
│   │   └── reversi.h
│   ├── savegame/
│   │   ├── savegame.cpp
│   │   └── savegame.h
│   ├── sokoban/
│   │   ├── sokoban.cpp
│   │   └── sokoban.h
│   ├── storage/
│   │   ├── storage.cpp
│   │   └── storage.h
│   ├── time_sync/
│   │   ├── time_sync.cpp
│   │   └── time_sync.h
│   ├── touch/
│   │   ├── touch.cpp
│   │   └── touch.h
│   └── wifi_setup/
│       ├── wifi_setup.cpp
│       └── wifi_setup.h
│ 
├── scripts/
│   ├── convert_hzk24h.py (not use in the programme)
│   ├── send_time.py
│   └── subset_font.py (not use in the programme)
│
└── src/
    └── main.cpp
