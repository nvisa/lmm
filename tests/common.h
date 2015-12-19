#ifndef COMMON_H
#define COMMON_H

#include <QString>

#include <vector>

class BaseTestParameters
{
public:
	std::vector<BaseTestParameters> getTestPars(const QString &group, int parset) = 0;
};

#endif // COMMON_H
