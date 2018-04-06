#pragma once
namespace OIV
{
//DELETE is a macro defined in winnt.h
// disable it for this file.
   
#pragma push_macro("DELETE")
#undef DELETE
    enum class KeyCode : uint16_t
    {
        UNASSIGNED = 0x00,
        ESCAPE = 0x01,
        KEY_1 = 0x02,
        KEY_2 = 0x03,
        KEY_3 = 0x04,
        KEY_4 = 0x05,
        KEY_5 = 0x06,
        KEY_6 = 0x07,
        KEY_7 = 0x08,
        KEY_8 = 0x09,
        KEY_9 = 0x0A,
        KEY_0 = 0x0B,
        MINUS = 0x0C,    // - on main keyboard
        EQUALS = 0x0D,
        BACK = 0x0E,    // backspace
        TAB = 0x0F,
        Q = 0x10,
        W = 0x11,
        E = 0x12,
        R = 0x13,
        T = 0x14,
        Y = 0x15,
        U = 0x16,
        I = 0x17,
        O = 0x18,
        P = 0x19,
        LBRACKET = 0x1A,
        RBRACKET = 0x1B,
        ENTER = 0x1C,    // Enter on main keyboard
        LCONTROL = 0x1D,
        A = 0x1E,
        S = 0x1F,
        D = 0x20,
        F = 0x21,
        G = 0x22,
        H = 0x23,
        J = 0x24,
        K = 0x25,
        L = 0x26,
        SEMICOLON = 0x27,
        APOSTROPHE = 0x28,
        GRAVE = 0x29,    // accent
        LSHIFT = 0x2A,
        BACKSLASH = 0x2B,
        Z = 0x2C,
        X = 0x2D,
        C = 0x2E,
        V = 0x2F,
        B = 0x30,
        N = 0x31,
        M = 0x32,
        COMMA = 0x33,
        PERIOD = 0x34,    // . on main keyboard
        SLASH = 0x35,    // / on main keyboard
        RSHIFT = 0x36,
        MULTIPLY = 0x37,    // * on numeric keypad
        LALT = 0x38,    // left Alt
        SPACE = 0x39,
        CAPITAL = 0x3A,
        F1 = 0x3B,
        F2 = 0x3C,
        F3 = 0x3D,
        F4 = 0x3E,
        F5 = 0x3F,
        F6 = 0x40,
        F7 = 0x41,
        F8 = 0x42,
        F9 = 0x43,
        F10 = 0x44,
        NUMLOCK = 0x45,
        SCROLL = 0x46,    // Scroll Lock
        NUMPAD7 = 0x47,
        NUMPAD8 = 0x48,
        NUMPAD9 = 0x49,
        SUBTRACT = 0x4A,    // - on numeric keypad
        NUMPAD4 = 0x4B,
        NUMPAD5 = 0x4C,
        NUMPAD6 = 0x4D,
        ADD = 0x4E,    // + on numeric keypad
        NUMPAD1 = 0x4F,
        NUMPAD2 = 0x50,
        NUMPAD3 = 0x51,
        NUMPAD0 = 0x52,
        DECIMAL = 0x53,    // . on numeric keypad
        OEM_102 = 0x56,    // < > | on UK/Germany keyboards
        F11 = 0x57,
        F12 = 0x58,
        F13 = 0x64,    //                     (NEC PC98)
        F14 = 0x65,    //                     (NEC PC98)
        F15 = 0x66,    //                     (NEC PC98)
        KANA = 0x70,    // (Japanese keyboard)
        ABNT_C1 = 0x73,    // / ? on Portugese (Brazilian) keyboards
        CONVERT = 0x79,    // (Japanese keyboard)
        NOCONVERT = 0x7B,    // (Japanese keyboard)
        YEN = 0x7D,    // (Japanese keyboard)
        ABNT_C2 = 0x7E,    // Numpad . on Portugese (Brazilian) keyboards
        NUMPADEQUALS = 0x8D,    // = on numeric keypad (NEC PC98)
        PREVTRACK = 0x90,    // Previous Track (KC_CIRCUMFLEX on Japanese keyboard)
        AT = 0x91,    //                     (NEC PC98)
        COLON = 0x92,    //                     (NEC PC98)
        UNDERLINE = 0x93,    //                     (NEC PC98)
        KANJI = 0x94,    // (Japanese keyboard)
        STOP = 0x95,    //                     (NEC PC98)
        AX = 0x96,    //                     (Japan AX)
        UNLABELED = 0x97,    //                        (J3100)
        NEXTTRACK = 0x99,    // Next Track
        NUMPADENTER = 0x9C,    // Enter on numeric keypad
        RCONTROL = 0x9D,
        MUTE = 0xA0,    // Mute
        CALCULATOR = 0xA1,    // Calculator
        PLAYPAUSE = 0xA2,    // Play / Pause
        MEDIASTOP = 0xA4,    // Media Stop
        VOLUMEDOWN = 0xAE,    // Volume -
        VOLUMEUP = 0xB0,    // Volume +
        WEBHOME = 0xB2,    // Web home
        NUMPADCOMMA = 0xB3,    // , on numeric keypad (NEC PC98)
        DIVIDE = 0xB5,    // / on numeric keypad
        SYSRQ = 0xB7,
        RALT = 0xB8,    // right Alt
        PAUSE = 0xC5,    // Pause
        HOME = 0xC7,    // Home on arrow keypad
        UP = 0xC8,    // UpArrow on arrow keypad
        PGUP = 0xC9,    // PgUp on arrow keypad
        LEFT = 0xCB,    // LeftArrow on arrow keypad
        RIGHT = 0xCD,    // RightArrow on arrow keypad
        END = 0xCF,    // End on arrow keypad
        DOWN = 0xD0,    // DownArrow on arrow keypad
        PGDOWN = 0xD1,    // PgDn on arrow keypad
        INSERT = 0xD2,    // Insert on arrow keypad
        DELETE = 0xD3,    // Delete on arrow keypad
        LWIN = 0xDB,    // Left Windows key
        RWIN = 0xDC,    // Right Windows key
        APPS = 0xDD,    // AppMenu key
        POWER = 0xDE,    // System Power
        SLEEP = 0xDF,    // System Sleep
        WAKE = 0xE3,    // System Wake
        WEBSEARCH = 0xE5,    // Web Search
        WEBFAVORITES = 0xE6,    // Web Favorites
        WEBREFRESH = 0xE7,    // Web Refresh
        WEBSTOP = 0xE8,    // Web Stop
        WEBFORWARD = 0xE9,    // Web Forward
        WEBBACK = 0xEA,    // Web Back
        MYCOMPUTER = 0xEB,    // My Computer
        MAIL = 0xEC,    // Mail
        MEDIASELECT = 0xED     // Media Select

        //Extended scan codes
        , KEYPADENTER = 0XE01C
        , RCONTROL2          = 0XE01D //  1d(LCtrl)
        , FAKELSHIFT         = 0XE02A // 	2a(LShift)
        , KEYPADDIVIDE       = 0XE035 // 	35 (/ ? )
        , FAKERSHIFT         = 0XE036 // 	36 (RShift)
        , CONTROLPRINTSCREEN = 0XE037 // )		37 (*/ PrtScn)
        , RIGHTALT           = 0XE038 // )
        , CONTROL2           = 0XE046 // 		46 (ScrollLock)
        , GREYHOME           = 0XE047 // 	47 (Keypad - 7 / Home)
        , GREYUP             = 0XE048 // 48 (Keypad - 8 / UpArrow)
        , GREYPGUP           = 0XE049 // 	49 (Keypad - 9 / PgUp)
        , GREYLEFT           = 0XE04B // 	4b(Keypad - 4 / Left)
        , GREYRIGHT          = 0XE04D // 	4d(Keypad - 6 / Right)
        , GREYEND            = 0XE04F // 	4f(Keypad - 1 / End)
        , GREYDOWN           = 0XE050 // 	50 (Keypad - 2 / DownArrow)
        , GREYPGDN           = 0XE051 // 	51 (Keypad - 3 / PgDn)
        , GREYINSERT         = 0XE052 // 	52 (Keypad - 0 / Ins)
        , GREYDELETE         = 0XE053 // 	53 (Keypad - . / Del)
        , LEFTWINDOW         = 0XE05B //(LeftWindow)
        , RIGHTWINDOW        = 0XE05C // (RightWindow)
        , MENU               = 0XE05D // (Menu)


    };

    struct KeyCodeKeyStringPair
    {
        KeyCode keyCode;
        char* keyName;
    };

    const std::vector<KeyCodeKeyStringPair> KeyCodeString =
    {
         {KeyCode::UNASSIGNED ,"UNASSIGNED"}
        ,{KeyCode::ESCAPE ,"ESCAPE"}
        ,{KeyCode::KEY_1 ,"1"}
        ,{KeyCode::KEY_2 ,                         "2" }
        ,{KeyCode::KEY_3 ,                         "3" }
        ,{KeyCode::KEY_4 ,                         "4" }
        ,{KeyCode::KEY_5 ,                         "5" }
        ,{KeyCode::KEY_6 ,                         "6" }
        ,{KeyCode::KEY_7 ,                         "7" }
        ,{KeyCode::KEY_8 ,                         "8" }
        ,{KeyCode::KEY_9 ,                         "9" }
        ,{KeyCode::KEY_0 ,                         "0" }
        ,{KeyCode::MINUS ,                  "MINUS"}    // - on main keyboard
        ,{KeyCode::EQUALS ,               "EQUALS" }
        ,{KeyCode::BACK ,                  "BACK"  }    // backspace
        ,{KeyCode::TAB ,                     "TAB" }
        ,{KeyCode::Q ,                         "Q" }
        ,{KeyCode::W ,                         "W" }
        ,{KeyCode::E ,                         "E" }
        ,{KeyCode::R ,                         "R" }
        ,{KeyCode::T ,                         "T" }
        ,{KeyCode::Y ,                         "Y" }
        ,{KeyCode::U ,                         "U" }
        ,{KeyCode::I ,                         "I" }
        ,{KeyCode::O ,                         "O" }
        ,{KeyCode::P ,                         "P" }
        ,{KeyCode::LBRACKET ,           "LBRACKET" }
        ,{KeyCode::RBRACKET ,           "RBRACKET" }
        ,{KeyCode::ENTER,               "ENTER" }    // Enter on main keyboard
        ,{KeyCode::LCONTROL ,           "LCONTROL" }
        ,{KeyCode::A ,                         "A" }
        ,{KeyCode::S ,                         "S" }
        ,{KeyCode::D ,                         "D" }
        ,{KeyCode::F ,                         "F" }
        ,{KeyCode::G ,                         "G" }
        ,{KeyCode::H ,                         "H" }
        ,{KeyCode::J ,                         "J" }
        ,{KeyCode::K ,                         "K" }
        ,{KeyCode::L ,                         "L" }
        ,{KeyCode::SEMICOLON ,         "SEMICOLON" }
        ,{KeyCode::APOSTROPHE ,       "APOSTROPHE" }
        ,{KeyCode::GRAVE ,                 "GRAVE" }    // accent
        ,{KeyCode::LSHIFT ,               "LSHIFT" }
        ,{KeyCode::BACKSLASH ,         "BACKSLASH" }
        ,{KeyCode::Z ,                         "Z" }
        ,{KeyCode::X ,                         "X" }
        ,{KeyCode::C ,                         "C" }
        ,{KeyCode::V ,                         "V" }
        ,{KeyCode::B ,                         "B" }
        ,{KeyCode::N ,                         "N" }
        ,{KeyCode::M ,                         "M" }
        ,{KeyCode::COMMA ,                 "COMMA" }
        ,{KeyCode::PERIOD ,                 "PERIOD" }    // . on main keyboard
        ,{KeyCode::SLASH ,                   "SLASH" }    // / on main keyboard
        ,{KeyCode::RSHIFT ,                 "RSHIFT" }
        ,{KeyCode::MULTIPLY ,             "MULTIPLY" }    // * on numeric keypad
        ,{KeyCode::LALT ,                   "LALT" }    // left Alt
        ,{KeyCode::SPACE ,                   "SPACE" }
        ,{KeyCode::CAPITAL ,               "CAPITAL" }
        ,{KeyCode::F1 ,                         "F1" }
        ,{KeyCode::F2 ,                         "F2" }
        ,{KeyCode::F3 ,                         "F3" }
        ,{KeyCode::F4 ,                         "F4" }
        ,{KeyCode::F5 ,                         "F5" }
        ,{KeyCode::F6 ,                         "F6" }
        ,{KeyCode::F7 ,                         "F7" }
        ,{KeyCode::F8 ,                         "F8" }
        ,{KeyCode::F9 ,                         "F9" }
        ,{KeyCode::F10 ,                       "F10" }
        ,{KeyCode::NUMLOCK ,                         "NUMLOCK" }
        ,{KeyCode::SCROLL ,                           "SCROLL" }    // Scroll Lock
        ,{KeyCode::NUMPAD7 ,                         "NUMPAD7" }
        ,{KeyCode::NUMPAD8 ,                         "NUMPAD8" }
        ,{KeyCode::NUMPAD9 ,                         "NUMPAD9" }
        ,{KeyCode::SUBTRACT ,                       "SUBTRACT" }    // - on numeric keypad
        ,{KeyCode::NUMPAD4 ,                         "NUMPAD4" }
        ,{KeyCode::NUMPAD5 ,                         "NUMPAD5" }
        ,{KeyCode::NUMPAD6 ,                         "NUMPAD6" }
        ,{KeyCode::ADD ,                                 "ADD" }    // + on numeric keypad
        ,{KeyCode::NUMPAD1 ,                         "NUMPAD1" }
        ,{KeyCode::NUMPAD2 ,                         "NUMPAD2" }
        ,{KeyCode::NUMPAD3 ,                         "NUMPAD3" }
        ,{KeyCode::NUMPAD0 ,                         "NUMPAD0" }
        ,{KeyCode::DECIMAL ,                         "DECIMAL" }    // . on numeric keypad
        ,{KeyCode::OEM_102 ,                         "OEM_102" }    // < > | on UK/Germany keyboards
        ,{KeyCode::F11 ,                                 "F11" }
        ,{KeyCode::F12 ,                                 "F12" }
        ,{KeyCode::F13 ,                                  "F13" }    //                     (NEC PC98)
        ,{KeyCode::F14 ,                                  "F14" }    //                     (NEC PC98)
        ,{KeyCode::F15 ,                                  "F15" }    //                     (NEC PC98)
        ,{KeyCode::KANA ,                                "KANA" }    // (Japanese keyboard)
        ,{KeyCode::ABNT_C1 ,                          "ABNT_C1" }    // / ? on Portugese (Brazilian) keyboards
        ,{KeyCode::CONVERT ,                          "CONVERT" }    // (Japanese keyboard)
        ,{KeyCode::NOCONVERT ,                      "NOCONVERT" }    // (Japanese keyboard)
        ,{KeyCode::YEN ,                                  "YEN" }    // (Japanese keyboard)
        ,{KeyCode::ABNT_C2 ,                          "ABNT_C2" }    // Numpad . on Portugese (Brazilian) keyboards
        ,{KeyCode::NUMPADEQUALS ,                "NUMPADEQUALS" }    //" ,    // Previous Track (CIRCUMFLEX on Japanese keyboard)
        ,{KeyCode::PREVTRACK,                       "PREVTRACK"}
        ,{KeyCode::AT ,                                    "AT" }    //                     (NEC PC98)
        ,{KeyCode::COLON ,                              "COLON" }    //                     (NEC PC98)
        ,{KeyCode::UNDERLINE ,                      "UNDERLINE" }    //                     (NEC PC98)
        ,{KeyCode::KANJI ,                              "KANJI" }    // (Japanese keyboard)
        ,{KeyCode::STOP ,                                "STOP" }    //                     (NEC PC98)
        ,{KeyCode::AX ,                                    "AX" }    //                     (Japan AX)
        ,{KeyCode::UNLABELED ,                      "UNLABELED" }    //                        (J3100)
        ,{KeyCode::NEXTTRACK ,                      "NEXTTRACK" }    // Next Track
        ,{KeyCode::NUMPADENTER ,                  "NUMPADENTER" }    // Enter on numeric keypad
        ,{KeyCode::RCONTROL ,                        "RCONTROL" }
        ,{KeyCode::MUTE ,                                "MUTE" }    // Mute
        ,{KeyCode::CALCULATOR ,                    "CALCULATOR" }    // Calculator
        ,{KeyCode::PLAYPAUSE ,                      "PLAYPAUSE" }    // Play / Pause
        ,{KeyCode::MEDIASTOP ,                      "MEDIASTOP" }    // Media Stop
        ,{KeyCode::VOLUMEDOWN ,                    "VOLUMEDOWN" }    // Volume -
        ,{KeyCode::VOLUMEUP ,                        "VOLUMEUP" }    // Volume +
        ,{KeyCode::WEBHOME ,                          "WEBHOME" }    // Web home
        ,{KeyCode::NUMPADCOMMA ,                  "NUMPADCOMMA" }    //" , on numeric keypad (NEC PC98)
        ,{KeyCode::DIVIDE ,                            "DIVIDE" }    // / on numeric keypad
        ,{KeyCode::SYSRQ ,                              "SYSRQ" }
        ,{KeyCode::RALT ,                              "RALT" }    // right Alt
        ,{KeyCode::PAUSE ,                              "PAUSE" }    // Pause
        ,{KeyCode::HOME ,                                "HOME" }    // Home on arrow keypad
        ,{KeyCode::UP ,                                    "UP" }    // UpArrow on arrow keypad
        ,{KeyCode::PGUP ,                                "PGUP" }    // PgUp on arrow keypad
        ,{KeyCode::LEFT ,                                "LEFT" }    // LeftArrow on arrow keypad
        ,{KeyCode::RIGHT ,                              "RIGHT" }    // RightArrow on arrow keypad
        ,{KeyCode::END ,                                  "END" }    // End on arrow keypad
        ,{KeyCode::DOWN ,                                "DOWN" }    // DownArrow on arrow keypad
        ,{KeyCode::PGDOWN ,                            "PGDOWN" }    // PgDn on arrow keypad
        ,{KeyCode::INSERT ,                            "INSERT" }    // Insert on arrow keypad
        ,{KeyCode::DELETE ,                            "DELETE" }    // Delete on arrow keypad
        ,{KeyCode::LWIN ,                                "LWIN" }    // Left Windows key
        ,{KeyCode::RWIN ,                                "RWIN" }    // Right Windows key
        ,{KeyCode::APPS ,                                "APPS" }    // AppMenu key
        ,{KeyCode::POWER ,                              "POWER" }    // System Power
        ,{KeyCode::SLEEP ,                              "SLEEP" }    // System Sleep
        ,{KeyCode::WAKE ,                                "WAKE" }    // System Wake
        ,{KeyCode::WEBSEARCH ,                      "WEBSEARCH" }    // Web Search
        ,{KeyCode::WEBFAVORITES ,                "WEBFAVORITES" }    // Web Favorites
        ,{KeyCode::WEBREFRESH ,                    "WEBREFRESH" }    // Web Refresh
        ,{KeyCode::WEBSTOP ,                          "WEBSTOP" }    // Web Stop
        ,{KeyCode::WEBFORWARD ,                    "WEBFORWARD" }    // Web Forward
        ,{KeyCode::WEBBACK ,                          "WEBBACK" }    // Web Back
        ,{KeyCode::MYCOMPUTER ,                    "MYCOMPUTER" }    // My Computer
        ,{KeyCode::MAIL ,                                "MAIL" }    // Mail
        ,{KeyCode::MEDIASELECT,                    "MEDIASELECT"}
            
        //Extended scan codes
        ,{KeyCode::KEYPADENTER,                   "KEYPADENTER" }
        ,{KeyCode::RCONTROL2,                     "RCONTROL2"   }
        ,{KeyCode::FAKELSHIFT,                    "FAKELSHIFT"  }
        ,{KeyCode::KEYPADDIVIDE,                  "KEYPADDIVIDE"}
        ,{KeyCode::FAKERSHIFT,                    "FAKERSHIFT"  }
        ,{KeyCode::CONTROLPRINTSCREEN,            "CONTROLPRINTSCREEN" }
        ,{KeyCode::RIGHTALT,                      "RALT"        }
        ,{KeyCode::CONTROL2,                      "CONTROL2"    }
        ,{KeyCode::GREYHOME,                      "GREYHOME"    }                 
        ,{KeyCode::GREYUP,                        "GREYUP"      }       
        ,{KeyCode::GREYPGUP,                      "GREYPGUP"    }
        ,{KeyCode::GREYLEFT,                      "GREYLEFT"    }         
        ,{KeyCode::GREYRIGHT,                     "GREYRIGHT"   }         
        ,{KeyCode::GREYEND,                       "GREYEND"     }         
        ,{KeyCode::GREYDOWN,                      "GREYDOWN"    }         
        ,{KeyCode::GREYPGDN,                      "GREYPGDN"    }         
        ,{KeyCode::GREYINSERT,                    "GREYINSERT"  }         
        ,{KeyCode::GREYDELETE,                    "GREYDELETE"  } 
        ,{ KeyCode::LEFTWINDOW,                   "LEFTWINDOW"  }
        ,{ KeyCode::RIGHTWINDOW,                  "RIGHTWINDOW" }
        ,{ KeyCode::MENU,                         "MENU"        }
    };
#pragma pop_macro("DELETE")
}
