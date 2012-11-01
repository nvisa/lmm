#ifndef VIDEOUTILS_H
#define VIDEOUTILS_H

class VideoUtils
{
public:
	VideoUtils();
	static int getLineLength(int pixFmt, int width);
	static int getFrameSize(int pixFmt, int width, int height);
};

#endif // VIDEOUTILS_H
