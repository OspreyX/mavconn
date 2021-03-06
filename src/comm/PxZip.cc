#include "PxZip.h"

#include <cassert>
#include <opencv2/imgproc/imgproc.hpp>
#include <turbojpeg.h>
#include <zlib.h>

PxZip* PxZip::mInstance = 0;

PxZip*
PxZip::instance(void)
{
	if (mInstance == 0)
	{
		mInstance = new PxZip();
	}
	return mInstance;
}

PxZip::PxZip()
 : kChunkSize(128 * 1024)
{
	jpegBufferSize = TJBUFSIZE(1280, 960);
	jpegBuffer = new unsigned char[jpegBufferSize];

	handleCompress = tjInitCompress();
	handleDecompress = tjInitDecompress();

	chunkBuffer = new unsigned char[kChunkSize];
}

PxZip::~PxZip()
{
	delete [] jpegBuffer;
	delete [] chunkBuffer;

	tjDestroy(handleCompress);
	tjDestroy(handleDecompress);
}

void
PxZip::compressData(unsigned char* inData, size_t inDataSize,
					std::vector<unsigned char>& outData)
{
	std::vector<unsigned char> buffer;

	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.next_in = inData;
	strm.avail_in = inDataSize;
	strm.next_out = chunkBuffer;
	strm.avail_out = kChunkSize;
	
	deflateInit(&strm, Z_BEST_SPEED);

	int flush, ret;

	do
	{
		flush = (strm.avail_in == 0) ? Z_FINISH : Z_NO_FLUSH;

		do
		{
			strm.next_out = chunkBuffer;
			strm.avail_out = kChunkSize;

			ret = deflate(&strm, flush);
			assert(ret != Z_STREAM_ERROR);

			buffer.insert(buffer.end(), chunkBuffer,
						  chunkBuffer + kChunkSize - strm.avail_out);
		}
		while (strm.avail_out == 0);
	}
	while (flush != Z_FINISH);

	assert(ret == Z_STREAM_END);

	deflateEnd(&strm);

	outData.swap(buffer);
}			

void
PxZip::decompressData(unsigned char* inData, size_t inDataSize,
					  std::vector<unsigned char>& outData)
{
	std::vector<unsigned char> buffer;

	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.next_in = inData;
	strm.avail_in = inDataSize;
	strm.next_out = chunkBuffer;
	strm.avail_out = kChunkSize;
	
	inflateInit(&strm);

	int ret;

	do
	{
		if (strm.avail_in == 0)
		{
			break;
		}

		do
		{
			strm.next_out = chunkBuffer;
			strm.avail_out = kChunkSize;

			ret = inflate(&strm, Z_NO_FLUSH);
			assert(ret != Z_STREAM_ERROR);

			buffer.insert(buffer.end(), chunkBuffer,
						  chunkBuffer + kChunkSize - strm.avail_out);
		}
		while (strm.avail_out == 0);
	}
	while (ret != Z_STREAM_END);

	inflateEnd(&strm);

	outData.swap(buffer);
}

void
PxZip::compressImage(const cv::Mat& inData, std::vector<unsigned char>& outData)
{
	assert(inData.channels() == 1 || inData.channels() == 3);
	assert(inData.type() == CV_8U);

	unsigned long maxsize = TJBUFSIZE(inData.cols, inData.rows);
	if (maxsize > jpegBufferSize)
	{
		delete [] jpegBuffer;
		jpegBufferSize = maxsize;
		jpegBuffer = new unsigned char[jpegBufferSize];
	}

	int flags, jpegsubsamp;
	if (inData.channels() == 1)
	{
		flags = 0;
		jpegsubsamp = TJ_GRAYSCALE;
	}
	else
	{
		flags = TJ_BGR;
		jpegsubsamp = TJ_420;
	}

	unsigned long jpegSize = 0;
	tjCompress(handleCompress,
			   inData.data, inData.cols, inData.step[0], inData.rows,
			   inData.elemSize(), jpegBuffer, &jpegSize, jpegsubsamp, 50, flags);

	outData = std::vector<unsigned char>(jpegBuffer, jpegBuffer + jpegSize);
}

void
PxZip::decompressImage(unsigned char* inData, size_t inDataSize,
					   cv::Mat& outData)
{
	// get image attributes
	int width, height, jpegsubsamp;
	tjDecompressHeader2(handleDecompress, inData, inDataSize,
						&width, &height, &jpegsubsamp);

	int type, flags;
	if (jpegsubsamp == TJ_GRAYSCALE)
	{
		type = CV_8UC1;
		flags = 0;
	}
	else
	{
		type = CV_8UC3;
		flags = TJ_BGR;
	}
	outData = cv::Mat(height, width, type);

	tjDecompress(handleDecompress, inData, inDataSize,
				 outData.data, outData.cols, outData.step[0], outData.rows,
				 outData.elemSize(), flags);
}
