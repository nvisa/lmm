#include "mpegdashserver.h"

#include <lmm/debug.h>

#include <QStringList>

#define ruint(_i) ((data[_i] << 24) + (data[_i + 1] << 16) + (data[_i + 2] << 8) + (data[_i + 3] << 0))
#define rushort(_i) ((data[_i + 0] << 8) + (data[_i + 1] << 0))
#define ruint24(_i) ((data[_i + 0] << 16) + (data[_i + 1] << 8) + (data[_i + 2] << 0))
#define ruint64(_i) (((quint64)data[_i] << 56) + ((quint64)data[_i + 1] << 48) + \
	((quint64)data[_i + 2] << 40) + ((quint64)data[_i + 3] << 32) + \
	((quint64)data[_i + 4] << 24) + ((quint64)data[_i + 5] << 16) + \
	((quint64)data[_i + 6] << 8) + ((quint64)data[_i + 7] << 0))
#define readstr(_i) QString::fromUtf8((const char *)&data[pos], _i)
#define rfloat(_i) (rushort(_i) + ((float)rushort(_i + 2) / 0x10000))

class IsoBox;
static IsoBox * fromData(const QByteArray &ba);

class IsoBox
{
public:
	virtual int parseBox(const QByteArray &data) = 0;
	int size() const { return boxSize; }
	QString type() const { return boxType; }
	const QList<IsoBox *> getChildren() const { return children; }
	virtual const QStringList getDebugInfo() const { return QStringList(); }
	virtual const QByteArray data() const { return boxData; }
	virtual bool isSupported() { return true; }

	void addChild(IsoBox *box) { children << box; }

	void serialize(QDataStream *out)
	{
		if (!isSupported())
			return;
		qint64 posi = out->device()->pos();

		*out << (qint32)0; //placeholder for size
		out->writeRawData(qPrintable(boxType), 4);
		write(out);

		/* serialize children */
		for (int i = 0; i < children.size(); i++)
			children[i]->serialize(out);

		qint32 size = out->device()->pos() - posi;
		out->device()->seek(posi);
		*out << (quint32)size;
		out->device()->seek(posi + size);
	}

	~IsoBox()
	{
		qDeleteAll(children);
	}

	uint boxSize;
	QString boxType;
	QByteArray boxData;
protected:
	QList<IsoBox *> children;
	int parseChildren(QByteArray sub)
	{
		int size = sub.size();
		while (sub.size()) {
			IsoBox *box = fromData(sub);
			int pos = box->parseBox(sub.left(box->size()));
			if (pos != box->size())
				qDebug() << "incomplete parse for" << box->type() << pos << box->size();
			sub = sub.mid(box->size());
			children << box;
		}
		return size;
	}
	virtual void write(QDataStream *) {}
};

class FtypBox : public IsoBox
{
public:
	FtypBox() : IsoBox() { boxType = "ftyp"; }

	QString majorBrand;
	QString compatiple;
	uint brandVersion;
protected:
	void write(QDataStream *out)
	{
		out->writeRawData(qPrintable(majorBrand), majorBrand.size());
		*out << (quint32)brandVersion;
		out->writeRawData(qPrintable(compatiple), compatiple.size());
	}
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		majorBrand = readstr(4);
		pos += 4;
		brandVersion = ruint(pos);
		pos += 4;
		compatiple = readstr(-1);
		pos += compatiple.size();

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("brand: %1").arg(majorBrand);
		lines << QString("brand version: %1").arg(brandVersion);
		lines << QString("compatiple brands: %1").arg(compatiple);
		return lines;
	}
};

class MoovBox : public IsoBox
{
public:
	MoovBox() : IsoBox() { boxType = "moov"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		/* moov doesn't expose anything, we are interested in its children */
		return parseChildren(ba.mid(8)) + 8;
	}
};

class MoofBox : public IsoBox
{
public:
	MoofBox() : IsoBox() { boxType = "moof"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		/* likewise moov, only children expose info */
		return parseChildren(ba.mid(8)) + 8;
	}
};

class MfhdBox : public IsoBox
{
public:
	MfhdBox() : IsoBox() { boxType = "mfhd"; }
	int seqn;
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;
		seqn = ruint(pos); pos += 4;

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("sequence no: %1").arg(seqn);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)0; //version and flags
		*out << (quint32)seqn;
	}
	int version;
	int flags;
};

class TrafBox : public IsoBox
{
public:
	TrafBox() : IsoBox() { boxType = "traf"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		/* likewise moov, only children expose info */
		return parseChildren(ba.mid(8)) + 8;
	}
};

class TfhdBox : public IsoBox
{
public:
	TfhdBox() : IsoBox() { boxType = "tfhd"; }
	uint trackId;
	int version;
	int flags;
	uint defaultDuration;
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;
		trackId = ruint(pos); pos += 4;

		if (flags & 0x08) {
			defaultDuration = ruint(pos);
			pos += 4;
		} else
			defaultDuration = 0;

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("track id: %1").arg(trackId);
		lines << QString("default duration: %1").arg(defaultDuration);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)(flags); //version and flags
		*out << (quint32)trackId;
		if (flags & 0x08)
			*out << (quint32)defaultDuration;
	}
};

class TfdtBox : public IsoBox
{
public:
	uint trackEnd;
	TfdtBox() : IsoBox() { boxType = "tfdt"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;

		trackEnd = ruint(pos); pos += 4;

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("track end: %1").arg(trackEnd);
		return lines;
	}

	void write(QDataStream *out)
	{
		*out << (quint32)0; //version and flags
		*out << (quint32)trackEnd;
	}

	int version;
	int flags;
};

class TrunBox : public IsoBox
{
public:
	TrunBox() : IsoBox() { boxType = "trun"; }
	int flags;
	int sampleCount;
	int dataOffset;
	QByteArray trunTable;
	QList<int> segmentSizes;
	QList<int> segmentDurations;
	QList<int> segmentFlags;
	QList<int> segmentSctos;

	void analyze(QList<IsoBox *>)
	{
		qDebug() << "===========================";
		for (int i = 0; i < segmentSizes.size(); i++) {
			qDebug() << segmentSizes[i] << segmentFlags[i] << sampleCount << dataOffset << flags << segmentDurations[i];
		}
	}
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;
		sampleCount = ruint(pos); pos += 4;

		if (flags & 0x01) {
			dataOffset = ruint(pos);
			pos += 4;
		} else
			dataOffset = 0;

		int entrySize = 0;

		if (flags & 0x100) {
			sampleDuration = 1;
			entrySize += 4;
		} else
			sampleDuration = 0;

		if (flags & 0x200) {
			sampleSize = 1;
			entrySize += 4;
		} else
			sampleSize = 0;

		if (flags & 0x400) {
			sampleFlags = 1;
			entrySize += 4;
		} else
			sampleFlags = 0;

		if (flags & 0x800) {
			sampleCTO = 1;
			entrySize += 4;
		} else
			sampleCTO = 0;

		trunTable = ba.mid(pos, sampleCount * entrySize);
		parseTable();
		pos += sampleCount * entrySize; //trun table

		return pos;
	}
	void parseTable()
	{
		const uchar *data = (const uchar *)trunTable.constData();
		int pos = 0;
		int total = 0;
		while (pos < trunTable.size()) {
			int sduration = -1, ssize = -1, sflags = -1, scto = -1;
			if (flags & 0x100) { sduration = ruint(pos); pos += 4; }
			if (flags & 0x200) { ssize = ruint(pos); pos += 4; }
			if (flags & 0x400) { sflags = ruint(pos); pos += 4; }
			if (flags & 0x800) { scto = ruint(pos); pos += 4; }
			segmentSizes << ssize;
			segmentDurations << sduration;
			segmentFlags << sflags;
			segmentSctos << scto;
			total += ssize;
		}
		//qDebug() << "trun size" << total;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("sample count: %1").arg(sampleCount);
		lines << QString("sample size: %1").arg(sampleSize);
		lines << QString("sample flags: %1").arg(sampleFlags);
		lines << QString("sample duration: %1").arg(sampleDuration);
		lines << QString("sample cto: %1").arg(sampleCTO);
		lines << QString("data offset: %1").arg(dataOffset);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)flags; //version and flags
		*out << (quint32)sampleCount;

		if (flags & 0x01)
			*out << (quint32)dataOffset;

		if (trunTable.size()) {
			/* if we a have trun table use it */
			out->writeRawData(trunTable.constData(), trunTable.size());
			return;
		}
		for (int i = 0; i < sampleCount; i++) {
			if (flags & 0x100)
				*out << (quint32)segmentDurations[i]; //sampleDuration
			if (flags & 0x200)
				*out << (quint32)segmentSizes[i]; //sampleSize
			if (flags & 0x400)
				*out << (quint32)segmentFlags[i]; //sampleFlags
			if (flags & 0x800)
				*out << (quint32)segmentSctos[i]; //sampleCTO
		}
	}

	int version;
	uint sampleCTO;
	uint sampleFlags;
	uint sampleSize;
	uint sampleDuration;
};

class MdatBox : public IsoBox
{
public:
	MdatBox() : IsoBox() { boxType = "mdat"; }

	void analyze(QList<IsoBox *> boxes)
	{
		int ind = boxes.indexOf(this);
		MoofBox *moof = (MoofBox *)boxes[ind - 1];
		const QList<IsoBox *> moofs = moof->getChildren();
		TrunBox *trun = (TrunBox *)moof->getChildren()[1]->getChildren()[2];
		QList<int> segmentSizes = trun->segmentSizes;
		QList<int> segmentFlags = trun->segmentFlags;
		const uchar *data = (const uchar *)boxData.constData();
		int pos = 0;
		for (int i = 0; i < segmentSizes.size(); i++) {
			uint nalLength = ruint(pos); pos += 4;
			uint nalType = data[pos] & 0x1f;
			qDebug() << pos - 4 << nalLength << segmentSizes[i] << nalType << segmentFlags[i];
			pos += segmentSizes[i] - 4;
		}
#if 0
		const uchar *data = (const uchar *)boxData.constData();
		int pos = 0;
		QList<uint> nals;
		QList<int> sample;
		while (pos < boxData.size()) {
			uint nalLength = ruint(pos); pos += 4;
			uint nalType = data[pos] & 0x1f;
			pos += nalLength;
			qDebug() << "mdat" << nalLength;
			if (nalType == 9)
				nals << nalType;
		}
		qDebug() << nals.size();
		exit(0);
#endif
	}
protected:
	int parseBox(const QByteArray &ba)
	{
		boxData = ba.mid(8);
		return ba.size();
	}
	void write(QDataStream *out)
	{
		out->writeRawData(boxData.constData(), boxData.size());
	}
};

class MvhdBox : public IsoBox
{
public:
	uint trackId;
	MvhdBox() : IsoBox() { boxType = "mvhd"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		if (version) {
			fDebug("un-supported version");
			return pos;
		}
		flags = ruint24(pos + 1);
		pos += 4;

		cTime = ruint(pos); pos += 4; //creation time
		mTime = ruint(pos); pos += 4; //modification time
		tScale = ruint(pos); pos += 4; //time scale
		duration = ruint(pos); pos += 4; //duration
		speed = ruint(pos); pos += 4; //playback speed
		volume = rushort(pos); pos += 2; //playback volume
		#if 1
		pos += 10; //reserved
		pos += 36; //geometry
		pos += 8; //preview
		pos += 4; //poster
		pos += 8; //selection time
		pos += 4; //current time
		trackId = ruint(pos); pos += 4; //track id
		#else
		boxData = ba.mid(pos);
		pos += boxData.size();
		#endif

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("creation time: %1").arg(cTime);
		lines << QString("modification time: %1").arg(mTime);
		lines << QString("time scale: %1").arg(tScale);
		lines << QString("duration : %1").arg(duration);
		lines << QString("playback speed: %1").arg(speed);
		lines << QString("playback volume: %1").arg(volume);
		lines << QString("track id: %1").arg(trackId);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)0; //version and flags
		*out << (quint32)3428874050ul; //creation time
		*out << (quint32)3428874050ul; //modification time
		*out << (quint32)90000; //time scale
		*out << (quint32)0; //duration
		*out << (quint32)65536; //playback speed
		*out << (quint16)256; //playback volume
		#if 1
		char reserved[70]; memset(reserved, 0, 70);
		out->writeRawData(reserved, 70);
		*out << (quint32)trackId;
		#else
		out->writeRawData(boxData.constData(), boxData.size());
		#endif
	}

	int version;
	int flags;
	uint cTime;
	uint mTime;
	uint tScale;
	uint duration;
	uint speed;
	uint volume;
};

class MvexBox : public IsoBox
{
public:
	MvexBox() : IsoBox() { boxType = "mvex"; }
protected:
	int parseBox(const QByteArray &data)
	{
		/* likewise moov, only children expose info */
		return parseChildren(data.mid(8)) + 8;
	}
};

class TrakBox : public IsoBox
{
public:
	TrakBox() : IsoBox() { boxType = "trak"; }
protected:
	int parseBox(const QByteArray &data)
	{
		/* likewise moov, only children expose info */
		return parseChildren(data.mid(8)) + 8;
	}
};

class TkhdBox : public IsoBox
{
public:
	uint trackId;
	float width; //fixed
	float height; //fixed
	QByteArray dispMatrix;
	TkhdBox() : IsoBox() { boxType = "tkhd"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;

		cTime = ruint(pos); pos += 4;
		mTime = ruint(pos); pos += 4;
		trackId = ruint(pos); pos += 4;
		pos += 4; //reserved
		duration = ruint(pos); pos += 4;
		pos += 8; //reserved
		layer = rushort(pos); pos += 2;
		aGroup = rushort(pos); pos += 2;
		pos += 2; //volume
		pos += 2; //reserved
		dispMatrix = ba.mid(pos, 36); pos += 36;
		width = rfloat(pos); pos += 4;
		height = rfloat(pos); pos += 4;

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("duration: %1").arg(duration);
		lines << QString("track id: %1").arg(trackId);
		lines << QString("layer: %1").arg(layer);
		lines << QString("access group: %1").arg(aGroup);
		lines << QString("width: %1").arg(width);
		lines << QString("height: %1").arg(height);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)(3); //version and flags
		*out << (quint32)3428874050ul; //creation time
		*out << (quint32)3428874050ul; //modification time
		*out << (quint32)trackId; //time scale
		*out << (quint32)0; //reserved
		*out << (quint32)0; //duration
		*out << (quint32)0; //reserved
		*out << (quint32)0; //reserved
		*out << (quint16)0; //layer
		*out << (quint16)0; //agroup
		*out << (quint16)0; //volume
		*out << (quint16)0; //reserved
		while (dispMatrix.size() < 36)
			dispMatrix.append((char)0);
		out->writeRawData(dispMatrix.constData(), 36);
		*out << (quint16)width;
		*out << (quint16)((width - (quint16)width) * 0x10000);
		*out << (quint16)height;
		*out << (quint16)((height - (quint16)height) * 0x10000);
	}

	int version;
	int flags;
	uint cTime;
	uint mTime;
	uint duration;
	ushort layer;
	ushort aGroup;
};

class MdiaBox : public IsoBox
{
public:
	MdiaBox() : IsoBox() { boxType = "mdia"; }
protected:
	int parseBox(const QByteArray &data)
	{
		/* likewise moov, only children expose info */
		return parseChildren(data.mid(8)) + 8;
	}
};

class MdhdBox : public IsoBox
{
public:
	MdhdBox() : IsoBox() { boxType = "mdhd"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;

		cTime = ruint(pos); pos += 4;
		mTime = ruint(pos); pos += 4;
		tScale = ruint(pos); pos += 4;
		duration = ruint(pos); pos += 4;
		pos += 4; //language is not parsed

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("time scale: %1").arg(tScale);
		lines << QString("duration: %1").arg(duration);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)(0); //version and flags
		*out << (quint32)3428874050ul; //creation time
		*out << (quint32)3428874050ul; //modification time
		*out << (quint32)90000; //time scale
		*out << (quint32)0; //duration
		*out << (quint16)0x55c4; //language
		*out << (quint16)0; //quality
	}

	int version;
	int flags;
	uint cTime;
	uint mTime;
	uint tScale;
	uint duration;
};

class HdlrBox : public IsoBox
{
public:
	QString handlerType;
	QString name;
	HdlrBox() : IsoBox() { boxType = "hdlr"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;

		pos += 4; //reserved
		handlerType = readstr(4); pos += 4;
		pos += 12; //reserved
		name = readstr(-1);
		pos += name.size() + 1;

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("handler type: %1").arg(handlerType);
		lines << QString("name: %1").arg(name);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)(0); //version and flags
		*out << (quint32)(0); //reserved
		out->writeRawData(qPrintable(handlerType), handlerType.size());
		*out << (quint32)(0); //reserved
		*out << (quint32)(0); //reserved
		*out << (quint32)(0); //reserved
		out->writeRawData(qPrintable(name), name.size() + 1);
	}

	int version;
	int flags;
};

class MinfBox : public IsoBox
{
public:
	MinfBox() : IsoBox() { boxType = "minf"; }
protected:
	int parseBox(const QByteArray &data)
	{
		/* likewise moov, only children expose info */
		return parseChildren(data.mid(8)) + 8;
	}
};

class DinfBox : public IsoBox
{
public:
	DinfBox() : IsoBox() { boxType = "dinf"; }
protected:
	int parseBox(const QByteArray &data)
	{
		/* likewise moov, only children expose info */
		return parseChildren(data.mid(8)) + 8;
	}
};

class StblBox : public IsoBox
{
public:
	StblBox() : IsoBox() { boxType = "stbl"; }
protected:
	int parseBox(const QByteArray &data)
	{
		/* likewise moov, only children expose info */
		return parseChildren(data.mid(8)) + 8;
	}
};

class StsdBox : public IsoBox
{
public:
	uint entryCount;
	StsdBox() : IsoBox() { boxType = "stsd"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;

		entryCount = ruint(pos); pos += 4;

		/* there will be only avc1, probably */
		return parseChildren(ba.mid(pos)) + pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("entry count: %1").arg(entryCount);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)(0); //version and flags
		*out << (quint32)entryCount; //reserved
	}
	uint version;
	uint flags;
};

class Avc1Box : public IsoBox
{
public:
	uint vres;
	uint hres;
	uint dataSize;
	ushort framesPerSample;
	ushort width;
	ushort height;
	ushort bps;
	ushort tableId;
	QString compressor;
	Avc1Box() : IsoBox() { boxType = "avc1"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		pos += 6; //reserved
		dri = rushort(pos); pos += 2;
		pos += 16; //version, revision, vendor, t. quality, s. quality */
		width = rushort(pos); pos += 2;
		height = rushort(pos); pos += 2;
		hres = ruint(pos); pos += 4;
		vres = ruint(pos); pos += 4;
		dataSize = ruint(pos); pos += 4;
		framesPerSample = rushort(pos); pos += 2;
		compressor = readstr(32); pos += 32;
		bps = rushort(pos); pos += 2;
		tableId = rushort(pos); pos += 2;

		/* there will avcC only, probably */
		return parseChildren(ba.mid(pos)) + pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("dri: %1").arg(dri);
		lines << QString("width: %1").arg(width);
		lines << QString("height: %1").arg(height);
		lines << QString("hres: %1").arg(hres);
		lines << QString("vres: %1").arg(vres);
		lines << QString("framesPerSample: %1").arg(framesPerSample);
		lines << QString("comp: %1").arg(compressor);
		lines << QString("bps: %1").arg(bps);
		lines << QString("tableId: %1").arg(tableId);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)(0); //reserved
		*out << (quint16)(0); //reserved
		*out << (quint16)1; //dri
		*out << (quint32)(0); //reserved
		*out << (quint32)(0); //reserved
		*out << (quint32)(0); //reserved
		*out << (quint32)(0); //reserved
		*out << (quint16)width; //dri
		*out << (quint16)height; //dri
		*out << (quint32)(hres);
		*out << (quint32)(vres);
		*out << (quint32)(dataSize);
		*out << (quint16)(framesPerSample);
		out->writeRawData(qPrintable(compressor), compressor.size());
		*out << (quint16)(bps);
		*out << (quint16)(tableId);
	}
	ushort dri;
};

class AvccBox : public IsoBox
{
public:
	AvccBox() : IsoBox() { boxType = "avcC"; }
	QByteArray sps, pps;
	uchar cVersion;
	uchar profile;
	uchar profCompat;
	uchar level;
	int nalLength;
	int spsCount;
	int spsLength;
	int ppsCount;
	int ppsLength;
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		cVersion = data[pos]; pos += 1;
		profile = data[pos]; pos += 1;
		profCompat = data[pos]; pos += 1;
		level = data[pos]; pos += 1;
		uchar tmp = data[pos]; pos += 1; //(tmp & 0x3f) should 63
		nalLength = (tmp >> 6) + 1;
		spsCount = data[pos] >> 3; pos += 1;
		spsLength = rushort(pos); pos += 2;
		sps = ba.mid(pos, spsLength);
		pos += spsLength;
		ppsCount = data[pos]; pos += 1;
		ppsLength = rushort(pos); pos += 2;
		pps = ba.mid(pos, ppsLength);
		pos += ppsLength;

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(cVersion);
		lines << QString("profile: %1").arg(profile);
		lines << QString("compat: %1").arg(profCompat);
		lines << QString("level: %1").arg(level);
		lines << QString("nal length: %1").arg(nalLength);
		lines << QString("sps count: %1").arg(spsCount);
		lines << QString("sps length: %1").arg(spsLength);
		lines << QString("pps count: %1").arg(ppsCount);
		lines << QString("pps length: %1").arg(ppsLength);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint8)cVersion;
		*out << (quint8)profile;
		*out << (quint8)profCompat;
		*out << (quint8)level;
		*out << (quint8)(((nalLength - 1) << 6) | 0x3f);
		*out << (quint8)((spsCount << 3) | 0x1);
		*out << (quint16)spsLength;
		out->writeRawData(sps.constData(), sps.size());
		*out << (quint8)ppsCount;
		*out << (quint16)ppsLength;
		out->writeRawData(pps.constData(), pps.size());
	}
};

class SttsBox : public IsoBox
{
public:
	SttsBox() : IsoBox() { boxType = "stts"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;

		entryCount = ruint(pos); pos += 4;

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("entry count: %1").arg(entryCount);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)0; //version and flags
		*out << (quint32)0; //entry count
	}
	uint version;
	uint flags;
	uint entryCount;
};

class StscBox : public IsoBox
{
public:
	StscBox() : IsoBox() { boxType = "stsc"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;

		entryCount = ruint(pos); pos += 4;

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("entry count: %1").arg(entryCount);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)0; //version and flags
		*out << (quint32)0; //entry count
	}
	uint version;
	uint flags;
	uint entryCount;
};

class StszBox : public IsoBox
{
public:
	StszBox() : IsoBox() { boxType = "stsz"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;

		sampleSize = ruint(pos); pos += 4;
		sampleCount = ruint(pos); pos += 4;

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("sample size: %1").arg(sampleSize);
		lines << QString("sample count: %1").arg(sampleCount);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)0; //version and flags
		*out << (quint32)0; //size
		*out << (quint32)0; //count
	}
	uint version;
	uint flags;
	uint sampleSize;
	uint sampleCount;
};

class DrefBox : public IsoBox
{
public:
	uint entryCount;
	DrefBox() : IsoBox() { boxType = "dref"; }
protected:
	int parseBox(const QByteArray &ba)
	{
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;

		entryCount = ruint(pos); pos += 4;

		/* we are not parsing url's */
		pos += 12 * entryCount;

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("entry count: %1").arg(entryCount);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)(0); //version and flags
		*out << (quint32)(1); //entry count
		*out << (quint32)(12); //entry count
		out->writeRawData("url ", 4);
		*out << (quint32)(1); //entry count
	}
	uint version;
	uint flags;
};

class TrexBox : public IsoBox
{
public:
	uint sdi;
	uint trackId;
	TrexBox() : IsoBox() { boxType = "trex"; }
protected:
	int  parseBox(const QByteArray &ba)
	{
		Q_UNUSED(ba);
		const uchar *data = (const uchar *)ba.constData();
		int pos = 8;

		version = data[pos];
		flags = ruint24(pos + 1);
		pos += 4;
		trackId = ruint(pos); pos += 4; //track id
		sdi = ruint(pos); pos += 4; //stsd id
		sampleDuration = ruint(pos); pos += 4; //duration
		sampleSize = ruint(pos); pos += 4; //size
		sampleFlags = ruint(pos); pos += 4; //flags

		return pos;
	}
	const QStringList getDebugInfo() const
	{
		QStringList lines;
		lines << QString("version: %1").arg(version);
		lines << QString("flags: %1").arg(flags);
		lines << QString("track id: %1").arg(trackId);
		lines << QString("sample duration: %1").arg(sampleDuration);
		lines << QString("sample size: %1").arg(sampleSize);
		lines << QString("sample flags: %1").arg(sampleFlags);
		lines << QString("stsd id: %1").arg(sdi);
		return lines;
	}
	void write(QDataStream *out)
	{
		*out << (quint32)0; //version and flags
		*out << (quint32)trackId;
		*out << (quint32)sdi;
		*out << (quint32)0; //duration
		*out << (quint32)0; //size
		*out << (quint32)0; //flags
	}
	uint version;
	uint flags;
	uint sampleDuration;
	uint sampleSize;
	uint sampleFlags;
};

class UnsupportedBox : public IsoBox
{
public:
	bool isSupported() { return false; }
protected:
	int parseBox(const QByteArray &ba)
	{
		boxData = ba.mid(8);
		return ba.size();
	}
	void write(QDataStream *out)
	{
		out->writeRawData(boxData.constData(), boxData.size());
	}
};

static IsoBox * fromData(const QByteArray &ba)
{
	const uchar *data = (const uchar *)ba.constData();
	int pos = 0;
	int boxSize = ruint(pos);
	pos += 4;
	QString boxType = readstr(4);
	IsoBox *box = NULL;
	if (boxType == "ftyp") box = new FtypBox();
	if (boxType == "moov") box = new MoovBox();
	if (boxType == "moof") box = new MoofBox();
	if (boxType == "mdat") box = new MdatBox();
	if (boxType == "mvhd") box = new MvhdBox();
	if (boxType == "mvex") box = new MvexBox();
	if (boxType == "trak") box = new TrakBox();
	if (boxType == "trex") box = new TrexBox();
	if (boxType == "mfhd") box = new MfhdBox();
	if (boxType == "traf") box = new TrafBox();
	if (boxType == "tfhd") box = new TfhdBox();
	if (boxType == "tfdt") box = new TfdtBox();
	if (boxType == "trun") box = new TrunBox();
	if (boxType == "tkhd") box = new TkhdBox();
	if (boxType == "mdia") box = new MdiaBox();
	if (boxType == "mdhd") box = new MdhdBox();
	if (boxType == "hdlr") box = new HdlrBox();
	if (boxType == "minf") box = new MinfBox();
	if (boxType == "dinf") box = new DinfBox();
	if (boxType == "dref") box = new DrefBox();
	if (boxType == "stbl") box = new StblBox();
	if (boxType == "stsd") box = new StsdBox();
	if (boxType == "stts") box = new SttsBox();
	if (boxType == "stsc") box = new StscBox();
	if (boxType == "stsz") box = new StszBox();
	if (boxType == "avc1") box = new Avc1Box();
	if (boxType == "avcC") box = new AvccBox();
	if (!box) {
		box = new UnsupportedBox();
		qDebug() << "unsupported box type:" << boxType;
	}

	box->boxSize = boxSize;
	box->boxType = boxType;

	return box;
}

MpegDashServer::MpegDashServer(QObject *parent) :
	QObject(parent)
{
	fragCount = 30;
	dashElapsed.start();
	dashElapsed.invalidate();
}

const QByteArray MpegDashServer::makeDashHeader(int width, int height)
{
	QList<IsoBox *> boxes;

	FtypBox *ftyp = new FtypBox();
	ftyp->majorBrand = "dash";
	ftyp->brandVersion = 0;
	ftyp->compatiple = "iso6avc1mp41";
	boxes << ftyp;

	MoovBox *moov = new MoovBox();
	MvhdBox *mvhd = new MvhdBox();
	mvhd->trackId = 0;
	moov->addChild(mvhd);
	MvexBox *mvex = new MvexBox();
	moov->addChild(mvex);
	TrexBox *trex = new TrexBox();
	trex->trackId = 1;
	trex->sdi = 1;
	mvex->addChild(trex);
	TrakBox *trak = new TrakBox();
	TkhdBox *tkhd = new TkhdBox();
	tkhd->trackId = 1;
	tkhd->width = width;
	tkhd->height = height;
	trak->addChild(tkhd);
	MdiaBox *mdia = new MdiaBox();
	trak->addChild(mdia);
	MdhdBox *mdhd = new MdhdBox();
	HdlrBox *hdlr = new HdlrBox();
	hdlr->name = "VideoHandler";
	hdlr->handlerType = "vide";
	MinfBox *minf = new MinfBox();
	DinfBox *dinf = new DinfBox();
	DrefBox *dref = new DrefBox();
	dref->entryCount = 0;
	dinf->addChild(dref);
	minf->addChild(dinf);
	StblBox *stbl = new StblBox();
	StsdBox *stsd = new StsdBox();
	stsd->entryCount = 1;
	Avc1Box *avc1 = new Avc1Box();
	avc1->width = width;
	avc1->height = height;
	avc1->vres = 4718592;
	avc1->hres = 4718592;
	avc1->dataSize = 0;
	avc1->framesPerSample = 1;
	avc1->bps = 24;
	avc1->tableId = 65535;
	avc1->compressor = QString("").leftJustified(32, '\0');
	AvccBox *avcc = new AvccBox();
	avcc->cVersion = 1;
	avcc->profile = 77;
	avcc->profCompat = 64;
	avcc->level = 21;
	avcc->nalLength = 4;
	avcc->spsCount = 28;
	avcc->spsLength = 22;
	avcc->ppsCount = 1;
	avcc->ppsLength = 4;
	avcc->sps.resize(22);
	avcc->pps.resize(4);
	avc1->addChild(avcc);
	stsd->addChild(avc1);
	stbl->addChild(stsd);
	stbl->addChild(new SttsBox());
	stbl->addChild(new StscBox());
	stbl->addChild(new StszBox());
	minf->addChild(stbl);
	mdia->addChild(mdhd);
	mdia->addChild(hdlr);
	mdia->addChild(minf);
	moov->addChild(trak);
	boxes << moov;

	QByteArray ba;
	QDataStream out(&ba, QIODevice::WriteOnly);
	out.setFloatingPointPrecision(QDataStream::SinglePrecision);
	out.setByteOrder(QDataStream::BigEndian);
	for (int i = 0; i < boxes.size(); i++)
		boxes[i]->serialize(&out);
	qDeleteAll(boxes);

	/* add a default session */
	buflock.lock();
	dashElapsed.restart();
	buflock.unlock();

	return ba;
}

static inline int writeBuffer(QDataStream *out, const RawBuffer &buf)
{
	*out << (quint32)(buf.size() - 4);
	out->writeRawData((const char *)buf.constData() + 4, buf.size() - 4);
	return buf.size();
}

const QByteArray MpegDashServer::makeDashSegment(int segmentSeqNo, int frameDuration)
{
#if 0
	QByteArray mdatba;
	QDataStream out(&mdatba, QIODevice::WriteOnly);
	out.setFloatingPointPrecision(QDataStream::SinglePrecision);
	out.setByteOrder(QDataStream::BigEndian);
	QList<int> segmentSizes; /* for trun table */
	QList<int> segmentFlags; /* for trun table */
	/* let's assume we have packetized input */
	buflock.lock();
	int segAvail = segments.size();
	buflock.unlock();
	while (segAvail < fragCount) {
		int diff = fragCount < segAvail;
		mDebug("need=%d have=%d", fragCount, segAvail);
		//usleep(1000 * 34 * diff);
		sleep(1);
		//QSemaphore s;
		//s.tryAcquire(1, 34 * diff);
		buflock.lock();
		segAvail = segments.size();
		buflock.unlock();
	}
	for (int i = 0; i < segAvail; i++) {
		buflock.lock();
		QList<RawBuffer> segment = segments.takeFirst();
		buflock.unlock();
#if 0
		int nal = buf.constPars()->h264NalType;
		if (nal == 7) {
			/* we need 4 more */
			if (i + 4 >= bufCount)
				break;
			int written = 0;
			written += writeBuffer(&out, buf);
			written += writeBuffer(&out, buffers[i + 1]);
			written += writeBuffer(&out, buffers[i + 2]);
			if (buffers[i + 2].constPars()->h264NalType == 6)
				written += writeBuffer(&out, buffers[i + 3]);
			segmentSizes << written;
			segmentFlags << 0x00010000;
			if (buffers[i + 2].constPars()->h264NalType == 6)
				i += 3;
			else
				i += 2;
		} else if (nal == 1) {
			segmentSizes << writeBuffer(&out, buf);
			segmentFlags << 0x00010000;
		}
		if (segmentSizes.size() == segmentCount) {
			break;
		}
#else
		int written = 0;
		for (int i = 0; i < segment.size(); i++) {
			written += writeBuffer(&out, segment[i]);
		}
		segmentSizes << written;
		segmentFlags << 0x00010000;
		if (segmentSizes.size() == fragCount)
			break;
#endif
	}

	QByteArray ba;
	QDataStream out2(&ba, QIODevice::WriteOnly);
	out2.setFloatingPointPrecision(QDataStream::SinglePrecision);
	out2.setByteOrder(QDataStream::BigEndian);
	MoofBox *moof = new MoofBox();
	MfhdBox *mfhd = new MfhdBox();
	mfhd->seqn = segmentSeqNo;
	moof->addChild(mfhd);
	TrafBox *traf = new TrafBox();
	moof->addChild(traf);
	TfhdBox *tfhd = new TfhdBox();
	tfhd->flags = 8;
	tfhd->trackId = 1;
	tfhd->defaultDuration = frameDuration;
	traf->addChild(tfhd);
	TfdtBox *tfdt = new TfdtBox();
	tfdt->trackEnd = (segmentSeqNo - 1) * frameDuration * fragCount;
	traf->addChild(tfdt);
	TrunBox *trun = new TrunBox();
	trun->flags = 1537;
	trun->dataOffset = fragCount * 8 + 96;
	trun->segmentSizes = segmentSizes;
	trun->segmentFlags = segmentFlags;
	trun->sampleCount = trun->segmentSizes.size();
	traf->addChild(trun);
	moof->serialize(&out2);

	MdatBox *mdat = new MdatBox();
	mdat->boxData = mdatba;
	mdat->serialize(&out2);

	delete traf;
	delete mdat;

	return ba;
#else
	QByteArray mdatba;
	QDataStream out(&mdatba, QIODevice::WriteOnly);
	out.setFloatingPointPrecision(QDataStream::SinglePrecision);
	out.setByteOrder(QDataStream::BigEndian);
	QList<int> segmentSizes; /* for trun table */
	QList<int> segmentFlags; /* for trun table */
	/* let's assume we have packetized input */
	buflock.lock();
	dashElapsed.restart();
	int gopAvail = gops.size();
	buflock.unlock();
	while (!gopAvail) {
		sleep(1);
		buflock.lock();
		gopAvail = gops.size();
		buflock.unlock();
	}
	mInfo("%d GOPs available", gopAvail);
	buflock.lock();
	QList<RawBuffer> gop = gops.takeFirst();
	buflock.unlock();
	if (gop.size() != fragCount) {
		int written = 0;
		written += writeBuffer(&out, gop[0]); //nal unit 7
		written += writeBuffer(&out, gop[1]); //nal unit 8
		written += writeBuffer(&out, gop[2]); //nal unit 5
		segmentSizes << written;
		segmentFlags << 0x00010000;
		for (int i = 3; i < gop.size(); i++) {
			segmentSizes << writeBuffer(&out, gop[i]);
			segmentFlags << 0x00010000;
		}
	} else {
		for (int i = 0; i < gop.size(); i++) {
			segmentSizes << writeBuffer(&out, gop[i]);
			segmentFlags << 0x00010000;
		}
	}
	if (segmentSizes.size() != fragCount) {
		mDebug("dash fragment count and encoder GOP size mismatch: %d != %d", segmentSizes.size(), fragCount);
		buflock.lock();
		gops.clear();
		buflock.unlock();
		return makeDashSegment(segmentSeqNo, frameDuration);
	}

	QByteArray ba;
	QDataStream out2(&ba, QIODevice::WriteOnly);
	out2.setFloatingPointPrecision(QDataStream::SinglePrecision);
	out2.setByteOrder(QDataStream::BigEndian);
	MoofBox *moof = new MoofBox();
	MfhdBox *mfhd = new MfhdBox();
	mfhd->seqn = segmentSeqNo;
	moof->addChild(mfhd);
	TrafBox *traf = new TrafBox();
	moof->addChild(traf);
	TfhdBox *tfhd = new TfhdBox();
	tfhd->flags = 8;
	tfhd->trackId = 1;
	tfhd->defaultDuration = frameDuration;
	traf->addChild(tfhd);
	TfdtBox *tfdt = new TfdtBox();
	tfdt->trackEnd = (segmentSeqNo - 1) * frameDuration * fragCount;
	traf->addChild(tfdt);
	TrunBox *trun = new TrunBox();
	trun->flags = 1537;
	trun->dataOffset = fragCount * 8 + 96;
	trun->segmentSizes = segmentSizes;
	trun->segmentFlags = segmentFlags;
	trun->sampleCount = trun->segmentSizes.size();
	traf->addChild(trun);
	moof->serialize(&out2);

	MdatBox *mdat = new MdatBox();
	mdat->boxData = mdatba;
	mdat->serialize(&out2);

	delete traf;
	delete mdat;

	return ba;
#endif
}

void MpegDashServer::addBuffer(RawBuffer buf)
{
	int nal = buf.constPars()->h264NalType;
	if (nal == 6) //discard any SEI packet
		return;
	buflock.lock();
	if (!dashElapsed.isValid() || dashElapsed.elapsed() > 5000) {
		/* do not queue packets if no one is connected */
		gops.clear();
		buflock.unlock();
		return;
	}
	if (nal == 1) {
		/* wait till we have an IDR */
		if (currentGOP.size())
			currentGOP << buf;
#if 0
		int nalu = currentGOP.first().constPars()->h264NalType;
		if (nalu == 1 && currentGOP.size() == fragCount) {
			gops << currentGOP;
			currentGOP.clear();
		} else if (nalu == 7 && currentGOP.size() == fragCount + 2) {
			gops << currentGOP;
			currentGOP.clear();
		}
#endif
	} else if (nal == 7) {
		/* starts a 7 - 8 - 5 segment */
		/* TODO: we assume that all GOPs have same size */
		if (currentGOP.size()) {
			gops << currentGOP;
			currentGOP.clear();
		}
		currentGOP << buf;
	} else if (nal == 8) {
		currentGOP << buf;
	} else if (nal == 5) {
		/* finalizes 7 - 8 - 5 segment */
		currentGOP << buf;
	}
	buflock.unlock();
}

#if 0
const QByteArray DashStreamingServer::getFile(const QString filename, QString &mime, QUrl &url)
{
	mime = mimeTypesByExtension[filename.split(".").last()];
	if (mime.isEmpty())
		mime = "application/octet-stream";
	if (filename.endsWith(".mp4")) {
		/* this is dash header file */
		return makeDashHeader(1280, 720);
	} else if (filename.endsWith(".mpd")) {
		QByteArray ba;
		QXmlStreamWriter w(&ba);
		w.setAutoFormatting(true);
		w.writeStartDocument();
		w.writeStartElement("MPD");
		w.writeAttribute("xmlns", "urn:mpeg:dash:schema:mpd:2011");
		w.writeAttribute("minBufferTime", "PT0.500000S");
		w.writeAttribute("suggestedPresentationDelay", "PT0S");
		w.writeAttribute("type", "static");
		w.writeAttribute("mediaPresentationDuration", "PT1H0M0S");
		w.writeAttribute("profiles", "urn:mpeg:dash:profile:isoff-live:2011");

		w.writeStartElement("ProgramInformation");
		w.writeAttribute("moreInformationURL", "www.aselsan.com.tr");
		w.writeTextElement("Title", "Aselsan VK365 Video Encoder MPD");
		w.writeEndElement();

		w.writeStartElement("Period");
		w.writeAttribute("id", "");
		w.writeAttribute("duration", "PT1H0M0S");

		w.writeStartElement("AdaptationSet");
		w.writeAttribute("segmentAlignment", "true");
		w.writeAttribute("maxWidth", "1280");
		w.writeAttribute("maxHeight", "720");
		w.writeAttribute("maxFrameRate", "90000/3000");
		w.writeAttribute("par", "1280:720");

		w.writeStartElement("Representation");
		w.writeAttribute("id", "1");
		w.writeAttribute("mimeType", "video/mp4");
		w.writeAttribute("codecs", "avc1.4d4015");
		w.writeAttribute("width", "1280");
		w.writeAttribute("height", "720");
		w.writeAttribute("frameRate", "90000/3000");
		w.writeAttribute("sar", "1:1");
		w.writeAttribute("startWithSAP", "1");
		w.writeAttribute("bandwidth", "6000000");

		w.writeStartElement("SegmentTemplate");
		w.writeAttribute("timescale", "90000");
		w.writeAttribute("duration", "90000");
		w.writeAttribute("media", "stream1_$Number$.m4s");
		w.writeAttribute("startNumber", "1");
		w.writeAttribute("initialization", "stream_init.mp4");
		w.writeEndElement();

		w.writeEndElement(); //Representation
		w.writeEndElement(); //AdaptationSet
		w.writeEndElement(); //Period
		w.writeEndElement(); //MPD

		return ba;
	} else if (filename.endsWith(".php")) {
		/* html file with possible some php extensions */
			QString php = QString::fromUtf8(::getFile(filename, documentRoot));
			/* IP Address support */
			if (php.contains(IP_ADDR_STR_PHP))
				php.replace(IP_ADDR_STR_PHP, QString("var myip = \"%1\";")
							.arg(NetworkConfiguration::findIp("eth0").toString()));
			while (php.contains("<?php require")) {
				int start = php.indexOf("<?php");
				int end = php.indexOf("?>", start);
				QString reqs = php.mid(start, end - start + 2);
				QStringList tokens = reqs.split(" ");
				if (tokens[1] == "require") {
					QString filename = tokens[2].remove("'");
					bool enabled = true;
					if (features.contains(filename))
						enabled = features[filename];
					QString content;
					if (enabled)
						content = QString::fromUtf8(::getFile(filename, documentRoot));
					php.replace(start, end - start + 2, content);
				} else if (tokens[1] == "builtin") {
					tokens.removeFirst();
					tokens.removeFirst();
					tokens.removeLast();
					QString feature = tokens.takeLast().remove("\"");
					if (!features.contains(feature) || features[feature]) {
						QStringList func = tokens.join(" ").split(QRegExp("[(,)]"));
						php.replace(start, end - start + 2, QString("<li><a href=\"#%1\">%2</a></li>")
									.arg(func[1].remove("\"")).arg(func[2].remove("\"")));
					} else {
						php.replace(start, end - start + 2, "");
					}
				}
			}
			return php.toUtf8();
	} else if (filename.endsWith(".json")) {
		QJsonParseError e;
		QByteArray ba = ::getFile(filename, documentRoot);
		QJsonDocument doc = QJsonDocument::fromJson(ba, &e);
		if (doc.isNull()) {
			mDebug("doc %s parse error: %s", qPrintable(filename), qPrintable(e.errorString()));
			return ba;
		}
		if (doc.isArray())
			doc.setArray(parseJsonArray(doc.array()));
		else
			mDebug("json %s root element is not array", qPrintable(filename));
		return doc.toJson();
	} else if (filename.endsWith(".m4s")) {
		if (!filename.contains("_"))
			return QByteArray();
		int seg = filename.split(".")[0].split("_")[1].toInt();
		return makeDashSegment(seg, 3000);
	} else if (filename.endsWith(".js")) {
		/* serverdata.js holds IP address variable */
		QString text = QString::fromUtf8(::getFile(filename, documentRoot));
		text.replace("ws://serverip:1337", QString("ws://%1:1337")
					 .arg(NetworkConfiguration::findIp("eth0").toString()));
		return text.toUtf8();
	} else if (filename.endsWith(".log")) {
		/* serve application log files */
		QString path = ApplicationSettings::instance()->get("debugging_settings.log_directory").toString();
		return ::getFile(filename, path);
	} else if (staticFiles.contains(filename))
		return staticFiles[filename];

	return ::getFile(filename, documentRoot);
}
#endif
