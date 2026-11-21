#pragma once
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t alerts_board_start(void);
esp_err_t alerts_board_beep(void);
#ifdef __cplusplus
}
#endif
