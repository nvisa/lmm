#include "lmmbufferpool.h"

#include <errno.h>

/**
	\class LmmBufferPool

	\brief Bu sinif bir tampon havuzu sunar. Bu sinif ile belirli bir sayida
	tamponu kutuphane icinde farkli noktalarda kullanmak icin kullanabilirsiniz.

	LmmBufferPool sinifinin 2 kullanim sekli mevcuttur, tampon-yonetimli ya da
	kaynak-yonetimli. Tampon-yonetimli kullanim seklinde, LmmBufferPool nesnesini
	yaratirken yapilandirici fonksiyona bir sayi gecebilirsiniz. Bu durumda
	LmmBufferPool o sayida RawBuffer olusturacak ve tamponlarin sahibini kendisi
	olarak isaretleyecektir. Bu sayede tampon yonetimini kendisi saglayacaktir.
	Bu noktadan sonra tek yapmaniz gereken take() fonksiyonunu kullanarak
	yeni bir tampon almanizdir. Eger havuz icinde tampon yoksa take() fonksiyonu
	yeni bir tampon gelene kadar blocke olacaktir. Bu sekilde aldiginiz tamponlari
	give() fonksiyonu ile geri verebilirsiniz. give() fonksiyonu bloke olan butun
	bekleyicileri uyandiracaktir.

	Kaynak-yonetimli bicimde ise LmmBufferPool nesnesini yaratirken LmmBufferPool()
	yapilandirici fonksiyonunu kullanirsiniz. Daha sonra istediginiz kadar tamponu
	addBuffer() fonksiyonu ile havuza birakirsiniz. Daha sonra take() ve give()
	fonksiyonlari ile tamponlari yonetebilirsiniz.

*/

LmmBufferPool::LmmBufferPool(QObject *parent) :
	QObject(parent)
{
	finalized = false;
}

LmmBufferPool::LmmBufferPool(int count, int size, QObject *parent) :
	QObject(parent)
{
	for (int i = 0; i < count; i++) {
		buffersFree << RawBuffer(QString("unknown"), (int)size, (BaseLmmElement *)this);
	}
	finalized = false;
}

int LmmBufferPool::addBuffer(RawBuffer buf)
{
	mutex.lock();
	finalized = false;
	buffersFree << buf;
	mutex.unlock();
	return 0;
}

int LmmBufferPool::usedBufferCount()
{
	mutex.lock();
	int cnt = buffersUsed.size();
	mutex.unlock();
	return cnt;
}

int LmmBufferPool::freeBufferCount()
{
	mutex.lock();
	int cnt = buffersFree.size();
	mutex.unlock();
	return cnt;
}

RawBuffer LmmBufferPool::take(bool keepRef)
{
	if (finalized)
		return RawBuffer();
	mutex.lock();
	if (buffersFree.size() == 0) {
		wc.wait(&mutex);
		if (finalized) {
			mutex.unlock();
			return RawBuffer();
		}
	}
	RawBuffer buf = buffersFree.takeFirst();
	if (keepRef)
		buffersUsed << buf;
	mutex.unlock();
	return buf;
}

int LmmBufferPool::give(RawBuffer buf)
{
	if (finalized)
		return -EINVAL;
	mutex.lock();
	buffersFree << buf;
	buffersUsed.removeAll(buf);
	mutex.unlock();
	wc.wakeAll();
	return 0;
}

void LmmBufferPool::finalize()
{
	mutex.lock();
	finalized = true;
	buffersFree.clear();
	buffersUsed.clear();
	mutex.unlock();
	wc.wakeAll();
	return;
}
