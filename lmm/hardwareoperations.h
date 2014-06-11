/*
 * Copyright (C) 2010 Yusuf Caglar Akyuz
 *
 * Her hakki Bilkon Ltd. Sti.'ye  aittir.
 */

#ifndef HARDWAREOPERATIONS_H
#define HARDWAREOPERATIONS_H

#include <QObject>

class HardwareOperations : public QObject
{
    Q_OBJECT
public:
    explicit HardwareOperations(QObject *parent = 0);
	int map(unsigned int addr);
	int unmap();
	int write(unsigned int off, unsigned int value);
	uint read(unsigned int off);
    static bool SetOsdTransparency(unsigned char trans, unsigned int x,
                    unsigned int y, unsigned int width, unsigned int height);
    static bool SetOsdTransparency(unsigned char trans);
    static bool fillFramebuffer(const char *fbfile, unsigned int value);
    static bool blendOSD(bool blend, unsigned short colorKey = 0xffff);

    static bool writeRegister(unsigned int addr, unsigned int value);

signals:

public slots:
private:
	unsigned int *mmapBase;
	void *realBase;
};

#endif // HARDWAREOPERATIONS_H
