// ============================================================================
// MARSVLT API — MUX Channel to Sensor Mapping
// ============================================================================
// This file maps each MUX channel to the sensor (key) connected to it.
//
// HOW IT WORKS:
// - Each HC4067 MUX has 16 channels (0-15), selected by address pins S0-S3.
// - Each channel connects to one Hall effect sensor under a key.
// - This mapping tells the firmware which sensor is on which MUX channel.
//
// HOW TO FILL THIS IN:
// 1. Look at your PCB schematic or wiring diagram.
// 2. For each MUX chip, note which key's sensor is wired to each channel.
// 3. Fill in the arrays below using S_<KEYNAME> from config.h, or raw numbers.
// 4. Use { 0 } for channels with no sensor connected.
//
// You can use either enum names or raw sensor numbers:
//     [3] = { S_E },     // Using enum name (recommended)
//     [3] = { 18 },      // Using raw number (S_E = 18 in 60% layout)
// ============================================================================

#include "hallscan_config.h"

// MUX 1 — Connected to MUX1_ADC_PIN
const mux16_ref_t mux1_channels[16] = {
    [0]  = { S_ESC  },
    [1]  = { S_1    },
    [2]  = { S_2    },
    [3]  = { S_3    },
    [4]  = { S_4    },
    [5]  = { S_5    },
    [6]  = { S_6    },
    [7]  = { S_7    },
    [8]  = { S_8    },
    [9]  = { S_9    },
    [10] = { S_0    },
    [11] = { S_MINS },
    [12] = { S_EQLS },
    [13] = { S_BSPC },
    [14] = { S_TAB  },
    [15] = { S_Q    },
};

// MUX 2 — Connected to MUX2_ADC_PIN
const mux16_ref_t mux2_channels[16] = {
    [0]  = { S_W    },
    [1]  = { S_E    },
    [2]  = { S_R    },
    [3]  = { S_T    },
    [4]  = { S_Y    },
    [5]  = { S_U    },
    [6]  = { S_I    },
    [7]  = { S_O    },
    [8]  = { S_P    },
    [9]  = { S_LBRC },
    [10] = { S_RBRC },
    [11] = { S_BSLS },
    [12] = { S_CAPS },
    [13] = { S_A    },
    [14] = { S_S    },
    [15] = { S_D    },
};

// MUX 3 — Connected to MUX3_ADC_PIN
const mux16_ref_t mux3_channels[16] = {
    [0]  = { S_F    },
    [1]  = { S_G    },
    [2]  = { S_H    },
    [3]  = { S_J    },
    [4]  = { S_K    },
    [5]  = { S_L    },
    [6]  = { S_SCLN },
    [7]  = { S_QUOT },
    [8]  = { S_ENT  },
    [9]  = { S_LSFT },
    [10] = { S_Z    },
    [11] = { S_X    },
    [12] = { S_C    },
    [13] = { S_V    },
    [14] = { S_B    },
    [15] = { S_N    },
};

// MUX 4 — Connected to MUX4_ADC_PIN
const mux16_ref_t mux4_channels[16] = {
    [0]  = { S_M    },
    [1]  = { S_COMM },
    [2]  = { S_DOT  },
    [3]  = { S_SLSH },
    [4]  = { S_RSFT },
    [5]  = { S_LCTL },
    [6]  = { S_WIN  },
    [7]  = { S_LALT },
    [8]  = { S_SPC  },
    [9]  = { S_RALT },
    [10] = { S_FN   },
    [11] = { S_LEFT },
    [12] = { S_DOWN },
    [13] = { S_RGHT },
    [14] = { S_MENU },
    [15] = { S_RCTL },
};
