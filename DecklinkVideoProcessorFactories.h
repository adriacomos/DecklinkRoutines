#pragma once

#include <memory>

namespace cvf {

class IVideoProcessor;

namespace decklink {

class DecklinkVideoProcessorFactories
{
public:

	static std::shared_ptr<IVideoProcessor> createVideoProcessor();
};

}
}