#include "Driver.h"

//
// Periodically sends the DS3 enable control request until successful.
// 
VOID Ds3EnableEvtTimerFunc(
    _In_ WDFTIMER Timer
)
{
    UNREFERENCED_PARAMETER(Timer);
}

//
// Sends output report updates to the DS3 via control endpoint.
// 
VOID Ds3OutputEvtTimerFunc(
    _In_ WDFTIMER Timer
)
{
    UNREFERENCED_PARAMETER(Timer);
}