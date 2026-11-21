#pragma once
#include "alerts/engine.hpp"
#include "alerts/store.hpp"

alerts::HttpResult alerts_http_get(const std::string& url);
void alerts_network_start(alerts::StateStore* store);
void alerts_check_ota(alerts::StateStore* store);
