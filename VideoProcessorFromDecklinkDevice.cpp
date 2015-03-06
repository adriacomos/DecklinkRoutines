#include "stdafx.h"
#include "VideoProcessorFromDecklinkDevice.h"
#include "DeckLinkDevice.h"
#include <iostream>

#include "opencv2\core\core.hpp"
#include "opencv2\imgproc\imgproc.hpp"

#include "opencv2\gpu\gpu.hpp"

#include "..\CUDARoutines\kernels.h"


using namespace cvf::decklink;
using namespace std;

namespace cvf {

VideoProcessorFromDecklinkDevice::VideoProcessorFromDecklinkDevice(double frameRate)
	:mFrameRate(frameRate), m_deckLinkDiscovery(nullptr)
	,m_gpuframe( nullptr), m_gpufield1( nullptr), m_gpufield2( nullptr)
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	mHeight = 0;
	mWidth = 0;

	

}


VideoProcessorFromDecklinkDevice::~VideoProcessorFromDecklinkDevice(void)
{
	//stopIt();
	if (m_deckLinkDiscovery)
		delete m_deckLinkDiscovery;

	mLstDevices.clear();
	CoUninitialize();

	if (m_gpuframe)
		delete m_gpuframe;
	
	if (m_gpufield1)
		delete m_gpufield1;
	
	if (m_gpufield2)
		delete m_gpufield2;


	m_gpuframe = nullptr;
	m_gpufield1 = nullptr;
	m_gpufield2 = nullptr;

}

double VideoProcessorFromDecklinkDevice::getFrameRate() {
	return mFrameRate;
}


void VideoProcessorFromDecklinkDevice::stopIt() 
{
	VideoProcessor::stopIt();

	lock_guard<mutex> g(mtxDevices);
	if (!mLstDevices.empty())
	{
		auto device = mLstDevices[0];
		device->StopCapture();
	}
}

cv::Size  VideoProcessorFromDecklinkDevice::getFrameSize() const 
{
	if (hasWorkingSize())
		return getWorkingSize();

	return cv::Size( mWidth,mHeight );
}

bool VideoProcessorFromDecklinkDevice::setInput( int frameWidth, int frameHeight)
{
	mHeight = frameHeight;
	mWidth = frameWidth;

	m_deckLinkDiscovery = new DeckLinkDeviceDiscovery( shared_from_this() );
		
	if (! m_deckLinkDiscovery->Enable()) {
		cout << "Please install the Blackmagic Desktop Video drivers to use the features of this application." << "This application requires the Desktop Video drivers installed.";
		return false;
	}

	
	int64 ticks = ::GetTickCount64();
	while(1)
	{
		lock_guard<mutex> g(mtxDevices);
		if (!mLstDevices.empty())
		{
			auto device = mLstDevices[0];
			device->StartCapture(0, this , true );
			return true;
		}

		if (::GetTickCount64() - ticks > 2000)  //espera dos segundos
			break;
	}

	return false;
}


void VideoProcessorFromDecklinkDevice::addDevice( std::shared_ptr<decklink::DeckLinkDevice> newDevice ) 
{

	// Initialise new DeckLinkDevice object
	if (!newDevice->Init())
	{
		newDevice->Release();
		return;
	}

	mLstDevices.push_back( newDevice );

	std::string name = newDevice->GetDeviceName();
	cout << "Decklink device detected: " << name << endl;

}


HRESULT		VideoProcessorFromDecklinkDevice::DrawFrame(IDeckLinkVideoFrame* videoFrame)
{
	//static double frec = cv::getTickFrequency()/1000;
	//static long long antTick;

	//cout << ((mLastFrameTick - antTick)/frec) << endl; 
	//antTick = mLastFrameTick;

	mLastFrameTick = cv::getTickCount();
	
    // Handle Video Frame
    if(videoFrame && !isStopped())
    {
        if (videoFrame->GetFlags() & bmdFrameHasNoInputSource)
        {
			fprintf(stderr, "Frame received - No input signal detected\n");
        }
        else
        {
			convertFrameToOpenCV( videoFrame, mFrame );

			
			if (!mFrame.empty() && !m_gpufield1 && !m_gpufield2) {
				m_gpuframe = new cv::gpu::GpuMat(mFrame);
				m_gpufield1 = new cv::gpu::GpuMat(mFrame);
				m_gpufield2 = new cv::gpu::GpuMat(mFrame);\\
			}
			else {
				m_gpuframe->upload(mFrame);
			}

			
			if (m_gpuframe && m_gpufield1 && m_gpufield2)
			{
				CUDARoutines::VideoFilters::deinterlaceInterpol( videoFrame->GetWidth(), 
															  videoFrame->GetHeight(),
															  m_gpuframe->data,
															  m_gpufield1->data,
															  m_gpufield2->data,
															  m_gpuframe->step);

				lock_guard<mutex> g( mtxFrameBytes);
				m_gpufield1->download(mField1);
				m_gpufield2->download(mField2);
			}
        }
    }
	return S_OK;
}



bool  VideoProcessorFromDecklinkDevice::readNextFrame( cv::Mat &frame )
{
	static double frec = cv::getTickFrequency()/1000;
	//static long long antTick;

	// Bucle que espera un nuevo frame
	while (mLastAcceptedFrameTick == mLastFrameTick );

	mLastAcceptedFrameTick = mLastFrameTick;


	long long timeAct = cv::getTickCount();
	//cout << ((timeAct - antTick)/frec) << endl; 
	//antTick = timeAct;

	
	if (mFrame.empty() || mField1.empty() || mField2.empty())
		return false;
		
	lock_guard<mutex> g( mtxFrameBytes);
	if ((long)((timeAct-mLastFrameTick)/frec) < delay/2 )
	{
		mField1.copyTo(frame);
	}
	else {
		mField2.copyTo(frame);
	}
	
	return true;
}



bool VideoProcessorFromDecklinkDevice::convertFrameToOpenCV(IDeckLinkVideoFrame* in, cv::Mat &frame)
{
	switch (in->GetPixelFormat()) {
    case bmdFormat8BitYUV:
    {
        void* data;
        if (FAILED(in->GetBytes(&data)))
            return false;

        cv::Mat mat = cv::Mat(in->GetHeight(), in->GetWidth(), CV_8UC2, data,
            in->GetRowBytes());

		lock_guard<mutex> g( mtxFrameBytes);
        cv::cvtColor(mat, frame, CV_YUV2BGR_UYVY);
        return true;
    }
    case bmdFormat8BitBGRA:
    {
        void* data;
        if (FAILED(in->GetBytes(&data)))
            return false;

		cv::Mat mat = cv::Mat(in->GetHeight(), in->GetWidth(), CV_8UC4, data);

		cv::cvtColor(mat, frame, CV_BGRA2BGR);
        return true;
    }
    default:
    {
		/*
        ComPtr<IDeckLinkVideoConversion> deckLinkVideoConversion =
            CreateVideoConversionInstance();
        if (! deckLinkVideoConversion)
            return false;

        CvMatDeckLinkVideoFrame cvMatWrapper(in->GetHeight(), in->GetWidth());
        if (FAILED(deckLinkVideoConversion->ConvertFrame(in.get(), &cvMatWrapper)))
            return false;
        
		lock_guard<mutex> g( mtxFrameBytes);
		cv::cvtColor(cvMatWrapper.mat, out, CV_BGRA2BGR);
        return true;*/
		return false;
    }}
}



// ------------------------------------------------------------------------------------------------------------- 
// NOTA: La diferencia principal con la implementación por defecto en VideoProcessor es que el delay que se
// espera entre frame y frame es la mitad, doblando el frame rate efectivo.
// Esto se ha hecho así porque, al menos en los videos entrelazados, se ha de leer field a field, y no frame
// a frame (es decir, al doble de frecuencia, y por tanto, en la mitad de tiempo -delay-).
void VideoProcessorFromDecklinkDevice::waitForNextFrame( long elapsedTime, long frec, int &keyPressed )
{
	bool paused = getPause();


	if (!paused)
	{
		long timeToWait = std::max(2L,(delay/2)-elapsedTime);
		keyPressed = cv::waitKey( timeToWait );
	}
	if (paused)
	{
		while ( paused && ( keyPressed = cv::waitKey(1)) == -1)
		{
			paused = getPause();
		}
	}
}

}