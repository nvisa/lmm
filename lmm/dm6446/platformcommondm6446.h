#ifndef PLATFORMCOMMONDM6446_H
#define PLATFORMCOMMONDM6446_H

class PlatformCommonDM6446 : public PlatformCommon
{
public:
	PlatformCommonDM6446();
	virtual void platformInit();
	virtual void platformCleanUp();
	static int showDecodeInfo(QGraphicsView *view, BaseLmmPlayer *dec);
};

#endif // PLATFORMCOMMONDM6446_H
