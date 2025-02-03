#include "parameters.h"

TempParameters::TempParameters()
{
    tempH  = 22.0;
    tempHH = 23.0;
    tempL  = 18.0;
    tempLL = 17.0;
}

void TempParameters::setTempH(float t)
{
    tempH = t;
}

void TempParameters::setTempHH(float t)
{
    tempHH = t;
}

void TempParameters::setTempL(float t)
{
    TempParameters::tempL = t;
}

void TempParameters::setTempLL(float t)
{
    tempLL = t;
}

float TempParameters::getTempH(void)
{
    return tempH;
}

float TempParameters::getTempHH(void)
{
    return tempHH;
}

float TempParameters::getTempL(void)
{
    return tempL;
}

float TempParameters::getTempLL(void)
{
    return tempLL;
}