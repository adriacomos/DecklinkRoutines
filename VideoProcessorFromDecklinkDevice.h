#pragma once

#include "VideoProcessor.h"
#include <memory>
#include <vector>
#include <mutex>

#include "opencv2\core\core.hpp"

#include "DeckLinkAPI_h.h"

namespace cvf {

namespace decklink {
	class DeckLinkDeviceDiscovery;
	class DeckLinkDevice;
}



class IDeviceManager 
{
public:
	virtual void addDevice( std::shared_ptr<decklink::DeckLinkDevice> ptr ) = 0;
};


class VideoProcessorFromDecklinkDevice :	public VideoProcessor, 
											public IDeviceManager,
											public std::enable_shared_from_this<VideoProcessorFromDecklinkDevice>,
											public IDeckLinkScreenPreviewCallback
{
	double mFrameRate;

	decklink::DeckLinkDeviceDiscovery *m_deckLinkDiscovery;

	std::mutex mtxDevices;
	std::vector<std::shared_ptr<decklink::DeckLinkDevice>> mLstDevices;  // Protegido por mtxDevices

	std::mutex mtxFrameBytes;
	cv::Mat mFrame;	// Protegido por mtxFrameBytes
	cv::Mat mField1;	// Protegido por mtxFrameBytes
	cv::Mat mField2;	// Protegido por mtxFrameBytes

	int mHeight, mWidth;

	std::mutex mtxFrameCount;
	long long mLastFrameTick;  // protegido por mtxFrameCount
	long long mLastAcceptedFrameTick; // protegido por mtxFrameCount

	bool convertFrameToOpenCV(IDeckLinkVideoFrame* in, cv::Mat &frame);


	void waitForNextFrame( long elapsedTime, long frec, int &keyPressed ) override;

	cv::gpu::GpuMat *m_gpuframe;
	cv::gpu::GpuMat *m_gpufield1;
	cv::gpu::GpuMat *m_gpufield2;

public:
	VideoProcessorFromDecklinkDevice(double frameRate );
	virtual ~VideoProcessorFromDecklinkDevice(void);

	//NOTA: el tamaño del frame debe coincidir con el vídeo de entrada
	bool setInput(int frameWidth, int frameHeight);

	void addDevice( std::shared_ptr<decklink::DeckLinkDevice> ptr  ) override;


	// IUnknown only needs a dummy implementation
	virtual HRESULT STDMETHODCALLTYPE	QueryInterface (REFIID iid, LPVOID *ppv)	{return E_NOINTERFACE;}
	virtual ULONG	STDMETHODCALLTYPE	AddRef ()									{return 1;}
	virtual ULONG	STDMETHODCALLTYPE	Release ()									{return 1;}

	// IDeckLinkScreenPreviewCallback
	virtual HRESULT STDMETHODCALLTYPE	DrawFrame(IDeckLinkVideoFrame* theFrame);

	bool readNextFrame( cv::Mat& frame ) override;
	bool isOpened() override {return true;}

	double getFrameRate() override;
	void stopIt() override;
	cv::Size  getFrameSize() const override;



};


};

