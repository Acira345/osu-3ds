#include "Mods.hpp"
#include "UiKit.hpp"

const ModDef modTable[15] = {
    // Fila 1: Reducción y Dificultad Clásica
    { "EZ", "Facil",     "Easy",         "Circulos mas grandes y velocidad reducida.",               MOD_EZ,   Ui::rgba(60, 185, 105) },
    { "NF", "NoFail",    "No Fail",      "No puedes perder aunque se agote la barra de vida.",       MOD_NF,   Ui::rgba(50, 165, 120) },
    { "HT", "Lento",     "Half Time",    "Velocidad de musica y notas reducida al 75% (0.75x).",     MOD_HT,   Ui::rgba(45, 150, 140) },
    { "HR", "Dificil",   "Hard Rock",    "Circulos mas pequenos, AR/OD alto y mapa invertido.",      MOD_HR,   Ui::rgba(240, 70, 70)  },
    { "SD", "Muerte",    "Sudden Death", "Falla una sola nota y perderas la partida al instante.",   MOD_SD,   Ui::rgba(255, 90, 50)  },

    // Fila 2: Velocidad y Desafío Extremo
    { "PF", "Perfect",   "Perfect",      "Solo calificaciones 300 permitidas. Un 100 y mueres.",     MOD_PF,   Ui::rgba(255, 180, 40) },
    { "DT", "DobleVel",  "Double Time",  "Velocidad de musica y notas aumentada al 150% (1.5x).",    MOD_DT,   Ui::rgba(180, 70, 240) },
    { "NC", "Nightcore", "Nightcore",    "Velocidad 1.5x con ritmo dance en los compases.",          MOD_NC,   Ui::rgba(220, 80, 255) },
    { "HD", "Oculto",    "Hidden",       "Las notas se desvanecen antes de tocar y no hay anillos.", MOD_HD,   Ui::rgba(255, 102, 171)},
    { "FL", "Linterna",  "Flashlight",   "Toda la pantalla a oscuras salvo un foco sobre el stylus.",MOD_FL,   Ui::rgba(255, 215, 60) },

    // Fila 3: Automatización, Estilo y Asistencia
    { "RX", "Relax",     "Relax",        "Solo apunta el stylus; el juego toca las notas por ti.",   MOD_RX,   Ui::rgba(0, 216, 240)  },
    { "AP", "AutoPilot", "Auto Pilot",   "Solo sigue el ritmo; el cursor apunta automaticamente.",   MOD_AP,   Ui::rgba(80, 180, 255) },
    { "SO", "SinSpin",   "Spun Out",     "Los spinners se giran automaticamente para no cansar.",    MOD_SO,   Ui::rgba(130, 220, 110)},
    { "MR", "Espejo",    "Mirror",       "Refleja todas las notas y sliders horizontalmente (X).",   MOD_MR,   Ui::rgba(255, 145, 185)},
    { "AT", "AutoBot",   "Autoplay",     "Demostracion perfecta por inteligencia artificial.",       MOD_AUTO, Ui::rgba(100, 110, 255)}
};
