# vnes.pro — Qt project file для Nokia C7 (Symbian^3)
# Собирать через Nokia Qt SDK 1.1 / Qt Creator

QT += core gui multimedia

TARGET   = vnes
TEMPLATE = app

# Исходники — ядро эмулятора (не менялось)
SOURCES += \
    src/Globals.cpp      \
    src/NESMemory.cpp    \
    src/NESROM.cpp       \
    src/NESMapper.cpp    \
    src/NESCPU.cpp       \
    src/NESPPU.cpp       \
    src/NESPAPU.cpp      \
    src/NESSystem.cpp    \
    src/NESWidget.cpp    \
    src/main.cpp

HEADERS += \
    inc/NESTypes.h    \
    inc/NESMemory.h   \
    inc/NESROM.h      \
    inc/NESMapper.h   \
    inc/NESCPU.h      \
    inc/NESPPU.h      \
    inc/NESPAPU.h     \
    inc/NESSystem.h   \
    inc/NESWidget.h

INCLUDEPATH += inc

# -----------------------------------------------------------------------
# Symbian^3 (Nokia C7) — специфичные настройки
# -----------------------------------------------------------------------
symbian {
    TARGET.UID3      = 0xE0000002
    TARGET.CAPABILITY= ReadUserData WriteUserData UserEnvironment
    TARGET.EPOCHEAPSIZE = 0x100000 0x2000000   # 1 MB min, 32 MB max

    # Библиотеки Symbian (ядро эмулятора использует e32base и т.п.)
    LIBS += -leuser -lefsrv -lestor

    # Multimedia для QAudioOutput
    LIBS += -lmediaclientaudiostream

    # Иконка приложения
    # icon.sources  = data/vnes.svg
    # icon.path     = .
    # DEPLOYMENT   += icon

    # Установочный файл .sis
    vendor_info = \
        "%{\"Vendor\"}" \
        ":\"Vendor\""
    my_deployment.pkg_prerules = vendor_info
    DEPLOYMENT += my_deployment

    # Папка для ROM на телефоне (создаётся при установке)
    roms_folder.sources =
    roms_folder.path    = /Data/NES
    DEPLOYMENT += roms_folder
}

# -----------------------------------------------------------------------
# Desktop (Windows/Linux/Mac) — для отладки на ПК
# -----------------------------------------------------------------------
!symbian {
    DEFINES += QT_NO_SYMBIAN
    # На десктопе Symbian-типы эмулируются через typedef
}
