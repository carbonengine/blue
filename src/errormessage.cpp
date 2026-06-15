// Copyright © 2021 CCP ehf.

#include "StdAfx.h"

#include "IBlueOS.h"
#include "errormessage.h"
#include <algorithm>
#include <cctype>

#if _WIN32
#include "WinNls.h"
#endif

static const std::wstring languageArgName = L"language";

// recognized language tags.
static const std::string de = "de";
static const std::string en = "en";
static const std::string es = "es";
static const std::string fr = "fr";
static const std::string ja = "ja";
static const std::string ko = "ko";
static const std::string ru = "ru";
static const std::string zh = "zh";

static const std::vector<std::string> allSupportedLanguageTags{
	de,
	en,
	es,
	fr,
	ja,
	ko,
	ru,
	zh
};

using StringTable = std::map<unsigned, std::string>;

static const StringTable stringTableDE{
	{ IDS_VERIFYFAIL_M, "Ihr EVE-Client enth\xe4lt wom\xf6glich modifizierte oder besch\xe4""digte Dateien. Sie sollten das Spiel neu installieren, um dieses Problem zu l\xf6sen. Wenn Sie den Kundendienst kontaktieren wollen, erw\xe4hnen Sie bitte alle unten angezeigten Fehlermeldungen." },
	{ IDS_VERIFYFAIL_M1, "Ihr EVE-Client enth\xe4lt zus\xe4tzliche Dateien. Wenn Sie k\xfcrzlich einen Patch heruntergeladen haben, m\xfcssen Sie Ihren Rechner eventuell neu starten. Wenn das Problem bestehen bleibt, sollten Sie das Spiel neu installieren. Wenn Sie den Kundendienst kontaktieren wollen, erw\xe4hnen Sie bitte diese Fehlermeldung." },
	{ IDS_VERIFYFAIL_C, "\xdc""berpr\xfc""fung fehlgeschlagen" },
	{ IDS_INVALIDWINDOWS, "Windows\xa0""10 oder h\xf6her ben\xf6tigt" },
	{ IDS_VERIFYFAIL_NOTFOUND, "File not found:" },
	{ IDS_VERIFYFAIL_UNKNOWNFOUND, "Unbekannte Dateien entdeckt:" },
	{ IDS_VERIFYFAIL_INCORRECTCRC, "\xdc""berpr\xfc""fung fehlgeschlagen:  \'%s\', crc:%x, expected:%x" },
	{ IDS_INVALIDMACOS, "macOS 10.15 oder h\xf6""her erforderlich"},
};

static const StringTable stringTableEN{
	{ IDS_VERIFYFAIL_M, "Your EVE client installation may have modified, damaged or corrupt files. You should reinstall the game to resolve this issue. If you need to contact support, please include any error message listed below." },
	{ IDS_VERIFYFAIL_M1, "There are extra files in your EVE client installation. If you\'ve recently applied a patch you may need to restart your computer. If the problem persists, you should reinstall the game. If you need to contact support, please include this error message." },
	{ IDS_VERIFYFAIL_C, "Verification Failure" },
	{ IDS_INVALIDWINDOWS, "Windows 10 or higher required" },
	{ IDS_VERIFYFAIL_NOTFOUND, "File not found:" },
	{ IDS_VERIFYFAIL_UNKNOWNFOUND, "Unknown files found:" },
	{ IDS_VERIFYFAIL_INCORRECTCRC, "Verification failed: \'%s\', crc:%x, expected:%x." },
	{ IDS_INVALIDMACOS, "macOS 10.15 or higher required"},
};

static const StringTable stringTableES{
	{ IDS_VERIFYFAIL_M, "La instalaci\xf3n de tu cliente de EVE podr\xed""a tener archivos modificados, da\xf1""ados o corruptos. Debes reinstalar el juego para resolver este problema. Si necesitas ponerte en contacto con el equipo de asistencia, incluye cualquier mensaje de error que aparece a continuaci\xf3n." },
	{ IDS_VERIFYFAIL_M1, "Hay archivos adicionales en la instalaci\xf3n de tu cliente de EVE. Si has aplicado un parche recientemente, es posible que tengas que reiniciar el ordenador. Si el problema persiste, deber\xed""as reinstalar el juego. Si necesitas contactar con el equipo de asistencia, incluye este mensaje de error." },
	{ IDS_VERIFYFAIL_C, "Fallo de verificaci\xf3n" },
	{ IDS_INVALIDWINDOWS, "Se requiere Windows 10 o superior" },
	{ IDS_VERIFYFAIL_NOTFOUND, "Archivo no encontrado:" },
	{ IDS_VERIFYFAIL_UNKNOWNFOUND, "Archivos desconocidos encontrados:" },
	{ IDS_VERIFYFAIL_INCORRECTCRC, "Error de verificaci\xf3n: \'%s\', crc:%x, esperado:%x." },
	{ IDS_INVALIDMACOS, "Se requiere macOS 10.15 o superior"},
};

static const StringTable stringTableFR{
	{ IDS_VERIFYFAIL_M, "Votre installation du client EVE comporte peut-\xeatre des fichiers modifi\xe9s, endommag\xe9s ou corrompus. Vous devriez r\xe9installer le jeu pour r\xe9soudre ce probl\xe8me. Si vous avez besoin de contacter l\'assistance, veuillez inclure tout message d\'erreur rencontr\xe9 ci-dessous." },
	{ IDS_VERIFYFAIL_M1, "L\'installation de votre client EVE comporte des fichiers suppl\xe9mentaires. Si vous avez appliqu\xe9 un patch r\xe9""cemment, un red\xe9marrage de votre ordinateur pourrait \xeatre n\xe9""cessaire. Si le probl\xe8me persiste, vous devriez r\xe9installer le jeu. Si vous avez besoin de contacter l\'assistance, veuillez inclure ce message d\'erreur." },
	{ IDS_VERIFYFAIL_C, "\xc9""chec de v\xe9rification" },
	{ IDS_INVALIDWINDOWS, "Windows\xa0""10 ou sup\xe9rieur requis" },
	{ IDS_VERIFYFAIL_NOTFOUND, "File not found:" },
	{ IDS_VERIFYFAIL_UNKNOWNFOUND, "Fichiers inconnus trouv\xe9s\xa0:" },
	{ IDS_VERIFYFAIL_INCORRECTCRC, "\xc9""chec de v\xe9rification\xa0: \'%s\', crc:%x, expected:%x." },
	{ IDS_INVALIDMACOS, "macOS 10.15 ou sup\xe9""rieur requis"},
};

static const StringTable stringTableJA{
	{ IDS_VERIFYFAIL_M, "EVE\x83N\x83\x89\x83""C\x83""A\x83\x93\x83g\x82\xcc\x83""C\x83\x93\x83X\x83g\x81[\x83\x8b\x82\xc9\x81""A\x95\xcf\x8dX\x81""E\x91\xb9\x8f\x9d\x81""E\x94j\x91\xb9\x82\xb5\x82\xbd\x83t\x83@\x83""C\x83\x8b\x82\xaa\x8a\xdc\x82\xdc\x82\xea\x82\xc4\x82\xa2\x82\xe9\x89\xc2\x94\\\x90\xab\x82\xaa\x82\xa0\x82\xe8\x82\xdc\x82\xb7\x81""B\x82\xb1\x82\xcc\x96\xe2\x91\xe8\x82\xf0\x89\xf0\x8c\x88\x82\xb7\x82\xe9\x82\xbd\x82\xdf\x82\xc9\x82\xcd\x83Q\x81[\x83\x80\x82\xf0\x8d\xc4\x83""C\x83\x93\x83X\x83g\x81[\x83\x8b\x82\xb5\x82\xc4\x82\xad\x82\xbe\x82\xb3\x82\xa2\x81""B\x83T\x83|\x81[\x83g\x82\xc9\x82\xa8\x96\xe2\x82\xa2\x8d\x87\x82\xed\x82\xb9\x82\xcc\x8d\xdb\x82\xc9\x82\xcd\x81""A\x89\xba\x8bL\x82\xcc\x83G\x83\x89\x81[\x83\x81\x83""b\x83Z\x81[\x83W\x82\xe0\x82\xb7\x82\xd7\x82\xc4\x82\xa8\x92m\x82\xe7\x82\xb9\x82\xad\x82\xbe\x82\xb3\x82\xa2\x81""B" },
	{ IDS_VERIFYFAIL_M1, "EVE\x83N\x83\x89\x83""C\x83""A\x83\x93\x83g\x82\xcc\x83""C\x83\x93\x83X\x83g\x81[\x83\x8b\x82\xc9\x92\xc7\x89\xc1\x82\xcc\x83t\x83@\x83""C\x83\x8b\x82\xaa\x93\xfc\x82\xc1\x82\xc4\x82\xa2\x82\xdc\x82\xb7\x81""B\x8d\xc5\x8b\xdf\x83p\x83""b\x83`\x82\xf0\x93K\x97p\x82\xb5\x82\xbd\x82\xce\x82\xa9\x82\xe8\x82\xc8\x82\xe7\x81""A\x83R\x83\x93\x83s\x83\x85\x81[\x83^\x82\xf0\x8d\xc4\x8bN\x93\xae\x82\xb7\x82\xe9\x95K\x97v\x82\xaa\x82\xa0\x82\xe9\x82\xa9\x82\xe0\x82\xb5\x82\xea\x82\xdc\x82\xb9\x82\xf1\x81""B\x82\xbb\x82\xea\x82\xc5\x82\xe0\x96\xe2\x91\xe8\x82\xaa\x89\xf0\x8c\x88\x82\xb5\x82\xc8\x82\xaf\x82\xea\x82\xce\x83Q\x81[\x83\x80\x82\xf0\x8d\xc4\x83""C\x83\x93\x83X\x83g\x81[\x83\x8b\x82\xb5\x82\xc4\x82\xad\x82\xbe\x82\xb3\x82\xa2\x81""B\x83T\x83|\x81[\x83g\x82\xc9\x82\xa8\x96\xe2\x82\xa2\x8d\x87\x82\xed\x82\xb9\x82\xcc\x8d\xdb\x82\xc9\x82\xcd\x81""A\x82\xb1\x82\xcc\x83G\x83\x89\x81[\x83\x81\x83""b\x83Z\x81[\x83W\x82\xe0\x82\xa8\x92m\x82\xe7\x82\xb9\x82\xad\x82\xbe\x82\xb3\x82\xa2\x81""B" },
	{ IDS_VERIFYFAIL_C, "\x94""F\x8f\xd8\x8e\xb8\x94s" },
	{ IDS_INVALIDWINDOWS, "Windows 10\x88\xc8\x8d~\x82\xaa\x95K\x97v\x82\xc5\x82\xb7" },
	{ IDS_VERIFYFAIL_NOTFOUND, "\x83t\x83@\x83""C\x83\x8b\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82\xb9\x82\xf1\x81""F" },
	{ IDS_VERIFYFAIL_UNKNOWNFOUND, "\x95s\x96\xbe\x82\xc8\x83t\x83@\x83""C\x83\x8b\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82\xb5\x82\xbd\x81""F" },
	{ IDS_VERIFYFAIL_INCORRECTCRC, "\x94""F\x8f\xd8\x82\xc9\x8e\xb8\x94s\x82\xb5\x82\xdc\x82\xb5\x82\xbd\x81""F\'%s\'\x81""Acrc\x81""F%x\x81""A\x97\\\x91z\x81""F%x\x81""B" },
	{ IDS_INVALIDMACOS, "macOS 10.15\x88\xc8\x8f\xe3\x82\xaa\x95K\x97v\x82\xc5\x82\xb7"},
};

static const StringTable stringTableKO{
	{ IDS_VERIFYFAIL_M, "\xc0\xcc\xba\xea \xc5\xac\xb6\xf3\xc0\xcc\xbe\xf0\xc6\xae\xbf\xa1 \xbc\xd5\xbb\xf3\xb5\xc7\xb0\xc5\xb3\xaa \xba\xaf\xb0\xe6\xb5\xc8 \xc6\xc4\xc0\xcf\xc0\xcc \xc1\xb8\xc0\xe7\xc7\xd5\xb4\xcf\xb4\xd9. \xb9\xae\xc1\xa6\xb8\xa6 \xc7\xd8\xb0\xe1\xc7\xcf\xb7\xc1\xb8\xe9 \xb0\xd4\xc0\xd3\xc0\xbb \xc0\xe7\xbc\xb3\xc4\xa1\xc7\xd8 \xc1\xd6\xbd\xca\xbd\xc3\xbf\xc0. \xb0\xed\xb0\xb4\xc1\xf6\xbf\xf8\xc0\xcc \xc7\xca\xbf\xe4\xc7\xd2 \xb0\xe6\xbf\xec \xbe\xc6\xb7\xa1\xbf\xa1 \xc7\xa5\xbd\xc3\xb5\xc8 \xbf\xc0\xb7\xf9 \xb8\xde\xbd\xc3\xc1\xf6\xb8\xa6 \xc6\xf7\xc7\xd4\xc7\xd8 \xc1\xd6\xbd\xc3\xb1\xe2 \xb9\xd9\xb6\xf8\xb4\xcf\xb4\xd9." },
	{ IDS_VERIFYFAIL_M1, "\xc0\xcc\xba\xea \xc5\xac\xb6\xf3\xc0\xcc\xbe\xf0\xc6\xae\xbf\xa1 \xc3\xdf\xb0\xa1 \xc6\xc4\xc0\xcf\xc0\xcc \xc1\xb8\xc0\xe7\xc7\xd5\xb4\xcf\xb4\xd9. \xc3\xd6\xb1\xd9\xbf\xa1 \xbe\xf7\xb5\xa5\xc0\xcc\xc6\xae\xb8\xa6 \xc1\xf8\xc7\xe0\xc7\xdf\xb4\xd9\xb8\xe9 \xc4\xc4\xc7\xbb\xc5\xcd\xb8\xa6 \xc0\xe7\xbd\xc3\xc0\xdb\xc7\xcf\xbd\xc3\xb1\xe2 \xb9\xd9\xb6\xf8\xb4\xcf\xb4\xd9. \xb9\xae\xc1\xa6\xb0\xa1 \xc1\xf6\xbc\xd3\xc0\xfb\xc0\xb8\xb7\xce \xb9\xdf\xbb\xfd\xc7\xd2 \xb0\xe6\xbf\xec \xb0\xd4\xc0\xd3\xc0\xbb \xc0\xe7\xbc\xb3\xc4\xa1\xc7\xcf\xbf\xa9 \xc1\xd6\xbd\xc3\xb1\xe2 \xb9\xd9\xb6\xf8\xb4\xcf\xb4\xd9. \xb0\xed\xb0\xb4\xc1\xf6\xbf\xf8\xc0\xcc \xc7\xca\xbf\xe4\xc7\xd2 \xb0\xe6\xbf\xec \xbe\xc6\xb7\xa1\xbf\xa1 \xc7\xa5\xbd\xc3\xb5\xc8 \xbf\xc0\xb7\xf9 \xb8\xde\xbd\xc3\xc1\xf6\xb8\xa6 \xc6\xf7\xc7\xd4\xc7\xd8 \xc1\xd6\xbd\xc3\xb1\xe2 \xb9\xd9\xb6\xf8\xb4\xcf\xb4\xd9." },
	{ IDS_VERIFYFAIL_C, "\xc0\xce\xc1\xf5 \xbd\xc7\xc6\xd0" },
	{ IDS_INVALIDWINDOWS, "\xc0\xa9\xb5\xb5\xbf\xec 10 \xc0\xcc\xbb\xf3\xc0\xc7 \xbf\xee\xbf\xb5\xc3\xbc\xc1\xa6\xb0\xa1 \xbf\xe4\xb1\xb8\xb5\xcb\xb4\xcf\xb4\xd9" },
	{ IDS_VERIFYFAIL_NOTFOUND, "\xc6\xc4\xc0\xcf\xc0\xbb \xc3\xa3\xc0\xbb \xbc\xf6 \xbe\xf8\xbd\xc0\xb4\xcf\xb4\xd9:" },
	{ IDS_VERIFYFAIL_UNKNOWNFOUND, "\xbe\xcb \xbc\xf6 \xbe\xf8\xb4\xc2 \xc6\xc4\xc0\xcf:" },
	{ IDS_VERIFYFAIL_INCORRECTCRC, "\xc0\xce\xc1\xf5 \xbd\xc7\xc6\xd0: \'%s\', crc:%x, \xbf\xb9\xbb\xf3:%x." },
	{ IDS_INVALIDMACOS, "macOS 10.15 \xc0\xcc\xbb\xf3 \xb9\xf6\xc0\xfc\xc0\xcc \xc7\xca\xbf\xe4\xc7\xd5\xb4\xcf\xb4\xd9."},
};

static const StringTable stringTableRU{
	{ IDS_VERIFYFAIL_M, "\x93\xe1\xe2\xa0\xad\xae\xa2\xab\xa5\xad\xad\xeb\xa9 \xe3 \xa2\xa0\xe1 \xaa\xab\xa8\xa5\xad\xe2 EVE \xac\xae\xa6\xa5\xe2 \xe1\xae\xa4\xa5\xe0\xa6\xa0\xe2\xec \xa2\xa8\xa4\xae\xa8\xa7\xac\xa5\xad\xf1\xad\xad\xeb\xa5 \xa8\xab\xa8 \xaf\xae\xa2\xe0\xa5\xa6\xa4\xf1\xad\xad\xeb\xa5 \xe4\xa0\xa9\xab\xeb. \x97\xe2\xae\xa1\xeb \xe0\xa5\xe8\xa8\xe2\xec \xed\xe2\xe3 \xaf\xe0\xae\xa1\xab\xa5\xac\xe3, \xa2\xa0\xac \xe1\xab\xa5\xa4\xe3\xa5\xe2 \xaf\xa5\xe0\xa5\xe3\xe1\xe2\xa0\xad\xae\xa2\xa8\xe2\xec \xa8\xa3\xe0\xe3. \x85\xe1\xab\xa8 \xe5\xae\xe2\xa8\xe2\xa5 \xe1\xa2\xef\xa7\xa0\xe2\xec\xe1\xef \xe1\xae \xe1\xab\xe3\xa6\xa1\xae\xa9 \xaf\xae\xa4\xa4\xa5\xe0\xa6\xaa\xa8, \xad\xa5 \xa7\xa0\xa1\xe3\xa4\xec\xe2\xa5 \xe3\xaa\xa0\xa7\xa0\xe2\xec \xa2\xe1\xa5 \xaf\xe0\xa8\xa2\xa5\xa4\xf1\xad\xad\xeb\xa5 \xad\xa8\xa6\xa5 \xe1\xae\xae\xa1\xe9\xa5\xad\xa8\xef \xae\xa1 \xae\xe8\xa8\xa1\xaa\xa0\xe5." },
	{ IDS_VERIFYFAIL_M1, "\x93\xe1\xe2\xa0\xad\xae\xa2\xab\xa5\xad\xad\xeb\xa9 \xe3 \xa2\xa0\xe1 \xaa\xab\xa8\xa5\xad\xe2 EVE \xe1\xae\xa4\xa5\xe0\xa6\xa8\xe2 \xab\xa8\xe8\xad\xa8\xa5 \xe4\xa0\xa9\xab\xeb. \x85\xe1\xab\xa8 \xa2\xeb \xad\xa5\xa4\xa0\xa2\xad\xae \xe3\xe1\xe2\xa0\xad\xae\xa2\xa8\xab\xa8 \xae\xa1\xad\xae\xa2\xab\xa5\xad\xa8\xa5, \xa2\xa0\xac \xac\xae\xa6\xa5\xe2 \xaf\xae\xe2\xe0\xa5\xa1\xae\xa2\xa0\xe2\xec\xe1\xef \xaf\xa5\xe0\xa5\xa7\xa0\xa3\xe0\xe3\xa7\xa8\xe2\xec \xaa\xae\xac\xaf\xec\xee\xe2\xa5\xe0. \x85\xe1\xab\xa8 \xaf\xe0\xae\xa1\xab\xa5\xac\xa0 \xad\xa5 \xa8\xe1\xe7\xa5\xa7\xad\xa5\xe2, \xaf\xa5\xe0\xa5\xe3\xe1\xe2\xa0\xad\xae\xa2\xa8\xe2\xa5 \xa8\xa3\xe0\xe3. \x85\xe1\xab\xa8 \xe5\xae\xe2\xa8\xe2\xa5 \xe1\xa2\xef\xa7\xa0\xe2\xec\xe1\xef \xe1\xae \xe1\xab\xe3\xa6\xa1\xae\xa9 \xaf\xae\xa4\xa4\xa5\xe0\xa6\xaa\xa8, \xad\xa5 \xa7\xa0\xa1\xe3\xa4\xec\xe2\xa5 \xe3\xaa\xa0\xa7\xa0\xe2\xec \xa2\xe1\xa5 \xaf\xe0\xa8\xa2\xa5\xa4\xf1\xad\xad\xeb\xa5 \xad\xa8\xa6\xa5 \xe1\xae\xae\xa1\xe9\xa5\xad\xa8\xef \xae\xa1 \xae\xe8\xa8\xa1\xaa\xa0\xe5." },
	{ IDS_VERIFYFAIL_C, "\x8e\xe8\xa8\xa1\xaa\xa0 \xaf\xae\xa4\xe2\xa2\xa5\xe0\xa6\xa4\xa5\xad\xa8\xef" },
	{ IDS_INVALIDWINDOWS, "\x92\xe0\xa5\xa1\xe3\xa5\xe2\xe1\xef Windows 10 \xa8\xab\xa8 \xa2\xeb\xe8\xa5" },
	{ IDS_VERIFYFAIL_NOTFOUND, "File not found:" },
	{ IDS_VERIFYFAIL_UNKNOWNFOUND, "\x8e\xa1\xad\xa0\xe0\xe3\xa6\xa5\xad\xeb \xad\xa5\xa8\xa7\xa2\xa5\xe1\xe2\xad\xeb\xa5 \xe4\xa0\xa9\xab\xeb:" },
	{ IDS_VERIFYFAIL_INCORRECTCRC, "\x8d\xa5 \xe3\xa4\xa0\xab\xae\xe1\xec \xa2\xeb\xaf\xae\xab\xad\xa8\xe2\xec \xaf\xe0\xae\xa2\xa5\xe0\xaa\xe3:  \'%s\',  crc:%x,  expected:%x." },
	{ IDS_INVALIDMACOS, "\x92\xe0\xa5\xa1\xe3\xa5\xe2\xe1\xef macOS \xa2\xa5\xe0\xe1\xa8\xa8 10.15 \xa8\xab\xa8 \xad\xae\xa2\xa5\xa5"},
};

static const StringTable stringTableZH{
	{ IDS_VERIFYFAIL_M, "\xc4\xe3\xb5\xc4\xbf\xcd\xbb\xa7\xb6\xcb\xb0\xb2\xd7\xb0\xbf\xc9\xc4\xdc\xd2\xd1\xb1\xbb\xd0\xde\xb8\xc4\xbb\xf2\xd2\xd1\xcb\xf0\xbb\xb5\xa3\xac\xc7\xeb\xd6\xd8\xd0\xc2\xb0\xb2\xd7\xb0\xd3\xce\xcf\xb7\xa1\xa3\xc8\xe7\xd0\xe8\xc1\xaa\xcf\xb5\xbf\xcd\xbb\xa7\xd6\xa7\xb3\xd6\xcd\xc5\xb6\xd3\xd1\xb0\xc7\xf3\xb0\xef\xd6\xfa\xa3\xac\xc7\xeb\xb8\xbd\xc9\xcf\xcf\xc2\xc1\xd0\xb4\xed\xce\xf3\xd0\xc5\xcf\xa2\xa1\xa3" },
	{ IDS_VERIFYFAIL_M1, "\xd5\xe2\xd0\xa9\xca\xc7\xbf\xcd\xbb\xa7\xb6\xcb\xb0\xb2\xd7\xb0\xba\xf3\xb2\xfa\xc9\xfa\xb5\xc4\xb6\xe0\xd3\xe0\xce\xc4\xbc\xfe\xa3\xac\xc8\xe7\xb9\xfb\xc4\xe3\xd7\xee\xbd\xfc\xd4\xf8\xb0\xb2\xd7\xb0\xb9\xfd\xb2\xb9\xb6\xa1\xa3\xac\xc7\xeb\xd6\xd8\xd0\xc2\xc6\xf4\xb6\xaf\xbc\xc6\xcb\xe3\xbb\xfa\xa1\xa3\xc8\xe7\xb9\xfb\xce\xca\xcc\xe2\xc8\xd4\xd4\xda\xa3\xac\xc7\xeb\xd6\xd8\xd0\xc2\xb0\xb2\xd7\xb0\xd3\xce\xcf\xb7\xbf\xcd\xbb\xa7\xb6\xcb\xa1\xa3\xc8\xe7\xd0\xe8\xc1\xaa\xcf\xb5\xbf\xcd\xbb\xa7\xd6\xa7\xb3\xd6\xcd\xc5\xb6\xd3\xd1\xb0\xc7\xf3\xb0\xef\xd6\xfa\xa3\xac\xc7\xeb\xb8\xbd\xc9\xcf\xb4\xcb\xcc\xf5\xb4\xed\xce\xf3\xd0\xc5\xcf\xa2\xa1\xa3" },
	{ IDS_VERIFYFAIL_C, "\xd1\xe9\xd6\xa4\xca\xa7\xb0\xdc" },
	{ IDS_INVALIDWINDOWS, "\xd0\xe8\xd2\xaaWindows 10\xbb\xf2\xb8\xfc\xb8\xdf\xb0\xe6\xb1\xbe" },
	{ IDS_VERIFYFAIL_NOTFOUND, "\xce\xb4\xd5\xd2\xb5\xbd\xce\xc4\xbc\xfe\xa3\xba" },
	{ IDS_VERIFYFAIL_UNKNOWNFOUND, "\xd5\xd2\xb5\xbd\xce\xb4\xd6\xaa\xce\xc4\xbc\xfe\xa3\xba" },
	{ IDS_VERIFYFAIL_INCORRECTCRC, "\xd1\xe9\xd6\xa4\xca\xa7\xb0\xdc\xa3\xba\'%s\', crc:%x, expected:%x." },
	{ IDS_INVALIDMACOS, "\xd0\xe8\xd2\xaa""macOS\xb2\xd9\xd7\xf7\xcf\xb5\xc0\xd6""10.15\xbb\xf2\xb8\xfc\xb8\xdf\xb0\xe6\xb1\xbe"},
};

static const std::map<std::string, StringTable> messageTable{
	{ de, stringTableDE },
	{ en, stringTableEN },
	{ es, stringTableES },
	{ fr, stringTableFR },
	{ ja, stringTableJA },
	{ ko, stringTableKO },
	{ ru, stringTableRU },
	{ zh, stringTableZH }
};

#if _WIN32
static const std::map<std::string, unsigned int> codePageTable{
	{ de, 1252 },
	{ en, 1252 },
	{ es, 1252 },
	{ fr, 1252 },
	{ ja, 932 },
	{ ko, 949 },
	{ ru, 866 },
	{ zh, 936 }
};

/*
	Get the codepage for the given language code.
*/
unsigned int GetCodePageForLanguageCode( const std::string& languageCode )
{
	auto iter = codePageTable.find( languageCode );
	if( iter != codePageTable.end() )
	{
		return iter->second;
	}
	else
	{
		return 1252; // Default
	}
}
#elif __APPLE__
static const std::map<std::string, CFStringEncoding> encodingTable{
	{ de, kCFStringEncodingWindowsLatin1 },
	{ en, kCFStringEncodingWindowsLatin1 },
	{ es, kCFStringEncodingWindowsLatin1 },
	{ fr, kCFStringEncodingWindowsLatin1 },
	{ ja, kCFStringEncodingDOSJapanese },
	{ ko, kCFStringEncodingDOSKorean },
	{ ru, kCFStringEncodingDOSRussian },
	{ zh, kCFStringEncodingDOSChineseSimplif }
};

/*
	Get the macos encoding appropriate for use with the given language code.
*/
CFStringEncoding GetEncodingForLanguageCode( const std::string& languageCode )
{
	auto iter = encodingTable.find( languageCode );
	if( iter != encodingTable.end() )
	{
		return iter->second;
	}
	else
	{
		return kCFStringEncodingWindowsLatin1; // Default
	}
}

/*
	Convert a string to a CFStringRef.
	The returned value must be freed.
*/
CFStringRef CopyStringToCFString( const char* value, const CFStringEncoding& encoding )
{
	return CFStringCreateWithBytes( nullptr, reinterpret_cast<const UInt8*>( value ), strlen( value ) * sizeof( char ), encoding, false );
}
/*
	Convert a WString to a CFStringRef.
	The returned value must be freed.
*/
CFStringRef CopyWStringToCFString( const wchar_t* value )
{
	CFStringEncoding encoding = ( CFByteOrderLittleEndian == CFByteOrderGetCurrent() ) ? kCFStringEncodingUTF32LE : kCFStringEncodingUTF32BE;
	return CFStringCreateWithBytes( nullptr, reinterpret_cast<const UInt8*>( value ), wcslen( value ) * sizeof( wchar_t ), encoding, false );
}
#endif

/*
	Uses OS functions to attempt to get an appropriate language code to use.
*/
std::string GetOSLanguageCode()
{
#if _WIN32
	LANGID lcid = GetUserDefaultUILanguage();

	switch( PRIMARYLANGID( lcid ) )
	{
	case LANG_GERMAN:
		return de;
	case LANG_ENGLISH:
		return en;
	case LANG_SPANISH:
		return es;
	case LANG_FRENCH:
		return fr;
	case LANG_JAPANESE:
		return ja;
	case LANG_KOREAN:
		return ko;
	case LANG_RUSSIAN:
		return ru;
	case LANG_CHINESE:
	case LANG_CHINESE_TRADITIONAL:
		return zh;
	default:
		return en;
	}
#elif __APPLE__
	// The CFLocale interface, documented here; https://developer.apple.com/documentation/corefoundation/cflocale-rsj
	// complies with ISO 639-x/IETF BCP 47. So we can use this to get a standardised language code we can relate back to
	// our internal language tags. The language code should conform to "primaryLanguage-..."
	CFArrayRef langs = CFLocaleCopyPreferredLanguages();
	CFStringRef langCode = (CFStringRef)CFArrayGetValueAtIndex( langs, 0 );

	const std::string* result = &en;
	for( auto iter = allSupportedLanguageTags.begin(); iter != allSupportedLanguageTags.end(); ++iter )
	{
		auto tag = CopyStringToCFString( iter->c_str(), kCFStringEncodingWindowsLatin2 );
		if( kCFCompareEqualTo != CFStringHasPrefix( langCode, tag ) )
		{
			result = &( *iter );
			CFRelease( tag );
			break;
		}
		else
		{
			CFRelease( tag );
		}
	}

	CFRelease( langs );

	return *result;
#else
#error Unsupported platform!
#endif
}

/*
	Tries to get the language code as provided as a command line argument. Failing
	that we try to get the code from the user's OS settings.
*/
std::string GetLanguageCode()
{
	if( BeOS->HasStartupArg( languageArgName ) )
	{
		auto code = std::string( CW2A( BeOS->GetStartupArgValue( languageArgName ).c_str() ) );
		std::transform( code.begin(), code.end(), code.begin(), []( char a ){ return std::tolower(a); } );
		return code;
	}
	else
	{
		return GetOSLanguageCode();
	}
}

/*
	Obtains an appropriate table of error message strings. Defaults to english.
*/
const StringTable& GetLocalizedStringTable()
{
	std::string name = GetLanguageCode();

	auto iter = messageTable.find( name );
	if( iter != messageTable.end() )
	{
		return iter->second;
	}
	else
	{
		return stringTableEN;
	}
}

/*
	Attempts to get a translated error message based on the supplied messageId. On failure, the
	origin string is returned instead.
*/
std::string GetByID( const std::string& original, unsigned messageId, const StringTable& table )
{
	auto iter = table.find( messageId );
	if( iter != table.end() )
	{
		return iter->second;
	}
	else
	{
		return original;
	}
}

void BlueShowInvalidOSVersionError()
{
#if _WIN32
	std::string localizedMessage = TranslateErrorMessage( "Windows 10 or higher required", IDS_INVALIDWINDOWS );
#elif __APPLE__
	std::string localizedMessage = TranslateErrorMessage( "Invalid macOS version", IDS_INVALIDMACOS );
#else
	#error Unsupported platform!
#endif
	std::string localizedTitle = TranslateErrorMessage( "Verification Failure", IDS_VERIFYFAIL_C );
	DisplayErrorMessageBox(localizedTitle.c_str(), localizedMessage.c_str());
}

std::string TranslateErrorMessage( const std::string& original, unsigned messageId )
{
	return GetByID( original, messageId, GetLocalizedStringTable() );
}

void DisplayErrorMessageBox( const char* title, const char* message )
{
#if _WIN32
	auto codePage = GetCodePageForLanguageCode( GetLanguageCode() );
	MessageBoxW( nullptr, CA2WEX( message, codePage ), CA2WEX( title, codePage ), MB_ICONSTOP );
#elif __APPLE__
	auto encoding = GetEncodingForLanguageCode( GetLanguageCode() );

	CFStringRef titleRef = CopyStringToCFString( title, encoding );
	CFStringRef messageRef = CopyStringToCFString( message, encoding );

	CCP_LOG( "Message %s", message );

	CFOptionFlags result; // result code from the message box
	CFUserNotificationDisplayAlert( 0, kCFUserNotificationStopAlertLevel, nullptr, nullptr, nullptr, titleRef, messageRef, nullptr, nullptr, nullptr, &result );

	if( titleRef )
	{
		CFRelease( titleRef );
	}
	if( messageRef )
	{
		CFRelease( messageRef );
	}
#else
#error Unsupported platform!
#endif
}
