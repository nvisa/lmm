#include "simplezip.h"
#include "miniz.c"

#include <ecl/debug.h>

#include <QFile>

struct SimpleZipPriv {
	mz_zip_archive *zip;
};

SimpleZip::SimpleZip(QObject *parent) :
	QObject(parent)
{
	priv = new SimpleZipPriv;
	priv->zip = new mz_zip_archive;
	memset(priv->zip, 0, sizeof(mz_zip_archive));
	mz_zip_writer_init_heap(priv->zip, 0, 0);
}

SimpleZip::~SimpleZip()
{
	delete priv;
}

QByteArray SimpleZip::getArchive()
{
#if 0
	QByteArray ba;
	mz_zip_archive zip;
	memset(&zip, 0, sizeof(zip));
	bool ok = mz_zip_writer_init_heap(&zip, 0, 0);
	if (!ok)
		return ba;
	QHashIterator<QString, QByteArray> i(files);
	while (i.hasNext()) {
		i.next();
		qDebug() << QString::fromUtf8(i.value());
		ok = mz_zip_writer_add_mem(&zip, qPrintable(i.key()), i.value().constData(), i.value().size(), MZ_NO_COMPRESSION);
	}
	mz_zip_writer_finalize_archive(&zip);
	ba = QByteArray((const char *)zip.m_pState->m_pMem, zip.m_pState->m_mem_size);
	mz_zip_writer_end(&zip);
	return ba;
#else
	void *pBuf = NULL;
	size_t pSize = 0;
	mz_zip_writer_finalize_heap_archive(priv->zip, &pBuf, &pSize);
	QByteArray ba = QByteArray((const char *)pBuf, pSize);
	free(pBuf);
	mz_zip_writer_end(priv->zip);
	delete priv->zip;
	priv->zip = NULL;
	return ba;
#endif
}

bool SimpleZip::saveArchive(const QString &filename)
{
	QFile f(filename);
	if (!f.open(QIODevice::WriteOnly))
		return false;
	f.write(getArchive());
	f.close();
	return true;
}

void SimpleZip::addDirectory(const QString &dir)
{
	if (dir.endsWith("/"))
		mz_zip_writer_add_mem(priv->zip, qPrintable(dir), NULL, 0, MZ_NO_COMPRESSION);
	else
		mz_zip_writer_add_mem(priv->zip, qPrintable(dir + "/"), NULL, 0, MZ_NO_COMPRESSION);
}

bool SimpleZip::addFile(const QString &filename, const QByteArray &ba, const QString &comment)
{
	Q_UNUSED(comment);
	mz_zip_writer_add_mem(priv->zip, qPrintable(filename), ba.constData(), ba.size(), MZ_NO_COMPRESSION);
	return true;
	/*
	files.insert(filename, ba);
	return true;
	return mz_zip_add_mem_to_archive_file_in_place("/tmp/archive.zip",
											qPrintable(filename),
											ba.constData(),
											ba.size(),
											qPrintable(comment),
											comment.size(),
											MZ_NO_COMPRESSION);*/
}
