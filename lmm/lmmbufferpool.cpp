#include "lmmbufferpool.h"

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
	yeni bir tampon gelene kadar block olacaktir. Bu sekilde aldiginiz tamponlari
	geri vermenize gerek yoktur, sistemde tamponu kullanan kimse kalmadigi zaman
	tampon otomatik olarak havuza dusecektir.

	Kaynak-yonetimli bicimde ise

*/

LmmBufferPool::LmmBufferPool(QObject *parent) :
	QObject(parent)
{
}

LmmBufferPool::LmmBufferPool(int count, QObject *parent)
{
}

void LmmBufferPool::addBuffer(RawBuffer buf)
{
}

int LmmBufferPool::totalBufferCount()
{
}

int LmmBufferPool::freeBufferCount()
{
}

RawBuffer LmmBufferPool::take()
{
}

int LmmBufferPool::give(RawBuffer buf)
{
}
