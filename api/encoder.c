#include "encoder.h"
#include "hallscan_config.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#if ENCODER_ENABLE

// quadrature state
static uint8_t last_state = 0;
// switch debounce
static uint32_t last_sw_ts = 0;
static bool last_sw = false;

void encoder_init(void)
{
    gpio_init(ENC_A_PIN);
    gpio_set_dir(ENC_A_PIN, GPIO_IN);
    gpio_pull_up(ENC_A_PIN);

    gpio_init(ENC_B_PIN);
    gpio_set_dir(ENC_B_PIN, GPIO_IN);
    gpio_pull_up(ENC_B_PIN);

    gpio_init(ENC_SW_PIN);
    gpio_set_dir(ENC_SW_PIN, GPIO_IN);
    gpio_pull_up(ENC_SW_PIN);

    // Read initial state
    uint8_t a = gpio_get(ENC_A_PIN) ? 1 : 0;
    uint8_t b = gpio_get(ENC_B_PIN) ? 1 : 0;
    last_state = (a << 1) | b;

    last_sw = !gpio_get(ENC_SW_PIN);
    last_sw_ts = to_ms_since_boot(get_absolute_time());
}

// Encoder state table for quadrature decoding
// Indexed as: (last_state << 2) | current_state
static const int8_t encoder_table[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

int encoder_poll(void)
{
    // Read current state
    uint8_t a = gpio_get(ENC_A_PIN) ? 1 : 0;
    uint8_t b = gpio_get(ENC_B_PIN) ? 1 : 0;
    uint8_t cur = (a << 1) | b;

    // Look up direction using table
    int8_t index = (last_state << 2) | cur;
    int8_t d = encoder_table[index];
    last_state = cur;

#if ENCODER_INVERT
    return (int)-d;
#else
    return (int)d;
#endif
}

bool encoder_switch_pressed(void)
{
    bool cur = !gpio_get(ENC_SW_PIN); // active-low
    uint32_t now = to_ms_since_boot(get_absolute_time());
    const uint32_t ENCODER_SW_DEBOUNCE_MS = 50;
    if (cur != last_sw && (now - last_sw_ts) > ENCODER_SW_DEBOUNCE_MS) {
        last_sw = cur;
        last_sw_ts = now;
        if (cur) return true;
    }
    return false;
}

#else // !ENCODER_ENABLE — stubs

void encoder_init(void) {}
int encoder_poll(void) { return 0; }
bool encoder_switch_pressed(void) { return false; }

#endif // ENCODER_ENABLE
