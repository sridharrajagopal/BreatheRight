
#ifndef _MY_EDGE_IMPULSE_H_
#define _MY_EDGE_IMPULSE_H_

#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct EI_DATA {
    uint16_t coughs;
    uint16_t sneezes;
} EI_DATA;

#define EDGEIMPULSE_TAB_NAME "EDGE-IMPULSE"
void edge_impulse_start();

#ifdef __cplusplus
}
#endif


#endif