// shared_data.h
#pragma once
typedef struct {
    float temperature;
    float humidity;
    float pressure;
    char ip_address[16];
} shared_data_t;

extern shared_data_t shared_data;
