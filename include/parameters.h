#ifndef PARAMETERS_H
#define PARAMETERS_H

class TempParameters
{
private:
    float tempH, tempHH, tempL, tempLL;

public:
    TempParameters();
    void setTempH(float);
    void setTempHH(float);
    void setTempL(float);
    void setTempLL(float);
    float getTempH(void);
    float getTempHH(void);
    float getTempL(void);
    float getTempLL(void);
};

#endif /* !PARAMETERS_H */