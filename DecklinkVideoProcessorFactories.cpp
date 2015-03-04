#include "stdafx.h"
#include "DecklinkVideoProcessorFactories.h"
#include "IVideoProcessor.h"
#include "VideoProcessorFromDecklinkDevice.h"

using namespace std;


namespace cvf {
namespace decklink {

std::shared_ptr<IVideoProcessor> DecklinkVideoProcessorFactories::createVideoProcessor()
{
	return make_shared<VideoProcessorFromDecklinkDevice>(25.0);
}


}
}