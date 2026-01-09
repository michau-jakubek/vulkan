#include "viewerTests.hpp"
#include "vtfCommandLine.hpp"
#include "vtfVertexInput.hpp"
#include "vtfCanvas.hpp"
#include "vtfZRenderPass.hpp"
#include "vtfBacktrace.hpp"
#include "vtfFilesystem.hpp"
#include "vtfDSBMgr.hpp"
#include "vtfZPipeline.hpp"
#include "vtfGlfwEvents.hpp"
#include "vtfZCommandBuffer.hpp"
#include "vtfCopyUtils.hpp"
#include "vtfTextImage.hpp"
#include "vtfProgramCollection.hpp"

namespace
{
using namespace vtf;

std::string makeShorterString (add_cref<std::string> name, uint32_t length = 20)
{
	return (name.length() > length) ? ("..." + name.substr(name.length() - length + 3)) : name;
}

void printUsage (add_ref<std::ostream> log)
{
	log << "Parameters:"													<< std::endl;
	log << "  -f <image_file>"												<< std::endl;
	log << "      loads regular image file e.g. PNG, JPG, JPEG."			<< std::endl;
	log << "  -p <panorama_file>"											<< std::endl;
	log << "      loads panorama image file i.a. PNG, JPG, JPEG."			<< std::endl;
	log << "      Assume that image width is two times bigger than height"	<< std::endl;
	log << "  Parameters -f and -p can appear multiple times"				<< std::endl;
	log << "Navigation keys:"												<< std::endl;
	log << "  Tab, Space: load|show next image"								<< std::endl;
	log << "  Shift+Tab:  load|show prev image"								<< std::endl;
	log << "  Scroll:     zoom in|out panorama image"						<< std::endl;
	log << "  Ctrl+Mouse: rotate within panorama image"						<< std::endl;
	log << "  Escape:     quit this app"									<< std::endl;
}

struct ImageFileInfo
{
	std::string fileName;
	bool		panorama;
	bool		assets;
	ImageFileInfo ()
		: fileName(), panorama(false), assets(false) {}
	ImageFileInfo (std::string&& imageFile, bool isPanorama)
		: fileName(), panorama(isPanorama) { fileName.swap(imageFile); }
};

std::vector<ImageFileInfo> userReadImageFiles (add_cref<strings> params, add_cref<std::string> assets,
											   add_ref<std::ostream> log, add_ref<int32_t> regularCount, add_ref<int32_t> panoramaCount)
{
	UNREF(assets);
	strings				sink;
	strings				args(params);
	Option				optRegularImage { "-f", 1 };
	Option				optPanorameImage { "-p", 1 };
	std::vector<Option>	options { optRegularImage };
	regularCount	= consumeOptions(optRegularImage, options, args, sink);
	panoramaCount	= consumeOptions(optPanorameImage, options, args, sink);

	if ((regularCount + panoramaCount) == 0)
	{
		log << "[ERROR} Use the list of images to display as a parameter" << std::endl;
		return {};
	}

	bool isPanoramaFile = false;
	auto insert = [&](add_ref<std::string> s) { return ImageFileInfo(std::move(s), isPanoramaFile); };
	std::vector<ImageFileInfo> imageFiles(make_unsigned(regularCount + panoramaCount));

	isPanoramaFile = false;
	std::transform(sink.begin(), std::next(sink.begin(), regularCount),
							 imageFiles.begin(), insert);
	isPanoramaFile = true;
	std::transform(std::next(sink.begin(), regularCount), sink.end(),
							 std::next(imageFiles.begin(), regularCount), insert);

	for (auto i = imageFiles.begin(); i != imageFiles.end();)
	{
		if (!fs::exists(fs::path(i->fileName)))
		{
			std::string fileName = makeShorterString(i->fileName);
			log << "[WARNING} Unable to read from " << fileName << std::endl;
			log << "          Remove this file from list" << std::endl;
			i = imageFiles.erase(i);
		}
		else ++i;
	}

	if (auto imageCount = imageFiles.size(); imageCount == 0)
	{
		regularCount = panoramaCount = 0u;
		log << "[ERROR} No image files to perform tests" << std::endl;
		return {};
	}

	return imageFiles;
}

TriLogicInt runViewerSingleThread (add_ref<Canvas> cs, add_cref<std::string> assets,
								   add_cref<std::vector<ImageFileInfo>> files,
								   const int32_t regularCount, const int32_t panoramaCount);

TriLogicInt prepareTests (add_cref<TestRecord> record, add_ref<CommandLine> cmdLine)
{
	UNREF(cmdLineParams);
	printUsage(std::cout);

	add_cref<GlobalAppFlags> gf = getGlobalAppFlags();
	CanvasStyle canvasStyle = Canvas::DefaultStyle;
	canvasStyle.surfaceFormatFlags |= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);
	Canvas cs(record.name, gf.layers, {}, {}, canvasStyle, nullptr, gf.apiVer);

	TriLogicInt	result			(1);
	int32_t		regularCount	= 0;
	int32_t		panoramaCount	= 0;
	std::vector<ImageFileInfo> files = userReadImageFiles(cmdLineParams, record.assets, std::cout, regularCount, panoramaCount);
	if (regularCount + panoramaCount)
	{
		result = runViewerSingleThread(cs, record.assets, files, regularCount, panoramaCount);
	}
	return result;
}

struct PushConstant
{
	float centerX;
	float centerY;
	float centerZ;
	float angleX;
	float angleY;
	float angleZ;
	float radius;
	float zoom;
	PushConstant()
		: centerX(0.0f), centerY(0.0f), centerZ(0.0f)
		, angleX(0.0f), angleY(0.0f), angleZ(0.0f)
		, radius(1.0f), zoom(1.0f) {}
};

struct UserData
{
	UserData (uint32_t maxImageCount);
	int				drawTrigger;
	const float		PI;
	PushConstant*	pc;
	ZImage			image;
	uint32_t		nextImage ();
	uint32_t		prevImage ();
	uint32_t		getImageIndex () const { return imageIndex; }
	friend void onKey (add_ref<Canvas> cs, add_ptr<void> userData, const int key, int scancode, int action, int mods);
	friend void onCursorPos (add_ref<Canvas> canvas, add_ptr<void> userData, double xpos, double ypos);
	friend void onMouseBtn (add_ref<Canvas>, add_ptr<void> userData, int button, int action, int mods);
private:
	bool			shiftPressed;
	bool			ctrlPressed;
	double			miceKeyXXXcursor;
	double			miceKeyYYYcursor;
	double			windowXXXcursor;
	double			windowYYYcursor;
	float			stepMove;
	uint32_t		imageIndex;
	const uint32_t	imageCount;
};
UserData::UserData (uint32_t maxImageCount)
	: drawTrigger	(1)
	, PI			(std::atan(1.0f) * 4.0f)
	, pc			(nullptr)
	, image			()
	, shiftPressed	(false)
	, ctrlPressed	(false)
	, miceKeyXXXcursor(), miceKeyYYYcursor()
	, windowXXXcursor(), windowYYYcursor()
	, stepMove		(30.0f)
	, imageIndex	(0)
	, imageCount	(maxImageCount) {}
uint32_t UserData::nextImage ()
{
	drawTrigger = drawTrigger + 1;
	imageIndex = (imageIndex + 1u) % imageCount;
	return imageIndex;
}
uint32_t UserData::prevImage ()
{
	drawTrigger = drawTrigger + 1;
	if (imageIndex != 0)
		imageIndex = imageIndex - 1u;
	else imageIndex = imageCount - 1u;
	return imageIndex;
}

void onKey (add_ref<Canvas> cs, void* userData, const int key, int scancode, int action, int mods)
{
	UNREF(userData); UNREF(scancode); UNREF(mods);
	auto ui = static_cast<add_ptr<UserData>>(userData);
	if (GLFW_PRESS == action || GLFW_REPEAT == action)
	{
		switch (key)
		{
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(*cs.window, GLFW_TRUE);
			break;

		case GLFW_KEY_LEFT_SHIFT:
			ui->shiftPressed = true;
			break;

		case GLFW_KEY_TAB:
		case GLFW_KEY_SPACE:
			ui->shiftPressed ? ui->prevImage() : ui->nextImage();
			break;

		case GLFW_KEY_KP_ADD:
			if (ui->stepMove + 10.0f < 175.0f)
				ui->stepMove = ui->stepMove + 10.0f;
			break;

		case GLFW_KEY_KP_SUBTRACT:
			if (ui->stepMove - 10.0f > 5.0f)
				ui->stepMove = ui->stepMove - 10.0f;
			break;
		}
	}
	else
	{
		ui->shiftPressed = false;
	}
}

void onMouseBtn (add_ref<Canvas>, add_ptr<void> userData, int button, int action, int mods)
{
	UNREF(mods);
	if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		auto ui = static_cast<add_ptr<UserData>>(userData);
		++ui->drawTrigger;
		ui->miceKeyXXXcursor = ui->windowXXXcursor;
		ui->miceKeyYYYcursor = ui->windowYYYcursor;
		ui->ctrlPressed = (action == GLFW_PRESS);
	}
}

void onCursorPos (add_ref<Canvas> canvas, add_ptr<void> userData, double xpos, double ypos)
{
	UNREF(canvas);

	auto ui = static_cast<add_ptr<UserData>>(userData);
	ui->windowXXXcursor = xpos;
	ui->windowYYYcursor = ypos;

	if (ui->pc != nullptr && ui->ctrlPressed)
	{
		const double wx = xpos - ui->miceKeyXXXcursor;
		const double wy = ypos - ui->miceKeyYYYcursor;
		const float dx = float(wx) / float(canvas.width);
		const float dy = float(wy) / float(canvas.height);
		const float ax = ui->stepMove * ui->PI / 180.f;
		const float ay = ui->stepMove * ui->PI / 180.f;
		ui->pc->angleX = std::fmod(ui->pc->angleX + dy * ax, 2.0f * ui->PI);
		ui->pc->angleY = std::fmod(ui->pc->angleY + dx * ay, 2.0f * ui->PI);
		ui->miceKeyXXXcursor = xpos;
		ui->miceKeyYYYcursor = ypos;
		++ui->drawTrigger;
	}
}

void onScroll (add_ref<Canvas> canvas, add_ptr<void> userData, double xScrollOffset, double yScrollOffset)
{
	UNREF(canvas);
	UNREF(xScrollOffset);
	auto ui = static_cast<add_ptr<UserData>>(userData);
	if (ui->pc)
	{
		if (yScrollOffset > 0.0)
			ui->pc->zoom *= 1.125f;
		else
			ui->pc->zoom /= 1.125f;
		++ui->drawTrigger;
	}
}

void onResize (add_ref<Canvas> cs, add_ptr<void> userData, int width, int height)
{
	UNREF(cs); UNREF(width); UNREF(height);
	static_cast<add_ptr<UserData>>(userData)->drawTrigger += 1;
}

struct ImageLoader;

struct ZImageEx : public expander<ZImageEx, ZImage>, public ZImage
{
	using expander<ZImageEx, ZImage>::operator=;
	PushConstant	pc;
	bool			loaded;
	ZImageView		view;
	uint32_t		width;
	uint32_t		height;
	ZImageEx () : pc(), loaded(), view(), width(), height() { }

	std::unique_ptr<ImageLoader> imageLoader;
	bool load(ZCommandPool cmdPool,
			 add_cref<std::string> imageFileName,
			 uint32_t splashWidth, uint32_t splashHeight,
			 bool isPanorama, ZShaderModule faceShaderModule);
};

struct ImageLoader
{
	struct PushContant
	{
		Vec4 imageSize;
	};

	typedef std::vector<std::function<void(add_ref<bool>)>> Subroutines;

	ImageLoader (ZCommandPool cmdPool,
				 add_ref<ZImageEx> image, add_cref<std::string> imageFileName,
				 uint32_t splashWidth, uint32_t splashHeight,
				 bool isPanorama, ZShaderModule faceShaderModule)
		: pool				(cmdPool)
		, outputImage		(image)
		, imageFile			(imageFileName)
		, splashSize		(makeExtent2D(splashWidth, splashHeight))
		, panorama			(isPanorama)
		, faceModule		(faceShaderModule)
		, counter			(0)
		, subroutines		(makeSubroutines(isPanorama))
		, layout			(cmdPool.getParam<ZDevice>())
		, usage				(VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_USAGE_SAMPLED_BIT)
		, fontBuffer		(createTextImageFontBuffer(cmdPool.getParam<ZDevice>()))
		, fileBuffer		()
		, fileBufferHandle	(VK_NULL_HANDLE)
		, fileThread		()
		, fileThreadActive	(false)
		, splashProgress	(0u)
	{
		updateSplashImage(0);
	}

	bool operator ()()
	{
		if (counter < subroutines.size())
		{
			bool repeat = false;
			subroutines[counter](repeat);
			counter += repeat ? 0u : 1u;
			return true;
		}
		return false;
	}

protected:
	const ZCommandPool		pool;
	add_ref<ZImageEx>		outputImage;
	const std::string		imageFile;
	const VkExtent2D		splashSize;
	const bool				panorama;
	const ZShaderModule		faceModule;
	uint32_t				counter;
	Subroutines				subroutines;
	LayoutManager			layout;
	const ZImageUsageFlags	usage;
	ZBuffer					fontBuffer;
	ZBuffer					fileBuffer;
	std::atomic<VkBuffer>	fileBufferHandle;
	std::vector<std::thread>fileThread;
	bool					fileThreadActive;
	uint32_t				splashProgress;
	ZImage					panImage;
	VkFormat				format;
	uint32_t				imageWidth;
	uint32_t				imageHeight;
	uint32_t				faceSize;
	uint32_t				local_size_xy;
	ZImage					tmpImage;
	ZPipeline				computePipeline;
	uint32_t				panBinding;
	ZImageView				cubeView;
	uint32_t				cubeBinding;

	Subroutines makeSubroutines (bool isPanorama)
	{
		Subroutines sr;
		if (isPanorama)
		{
			sr.emplace_back(std::bind(&ImageLoader::phaseCreateFileBuffer, this, std::placeholders::_1));
			sr.emplace_back(std::bind(&ImageLoader::phaseCreatePanoramaImage, this, std::placeholders::_1));
			sr.emplace_back(std::bind(&ImageLoader::phaseCreateTemporaryImageAndSetupSizes, this, std::placeholders::_1));
			sr.emplace_back(std::bind(&ImageLoader::phaseCreateComputePipeline, this, std::placeholders::_1));
			sr.emplace_back(std::bind(&ImageLoader::phaseMakeOutputImage, this, std::placeholders::_1));
		}
		else
		{
			sr.emplace_back(std::bind(&ImageLoader::phaseCreateFileBuffer, this, std::placeholders::_1));
			sr.emplace_back(std::bind(&ImageLoader::phaseCreateRegularImage, this, std::placeholders::_1));
		}
		return sr;
	}

	void updateSplashImage (uint32_t percent)
	{
		const std::string text = "Loading: " + makeShorterString(imageFile) + " - " + std::to_string(percent) + '%';
		outputImage = createTextImage(pool, fontBuffer, text, splashSize,
										makeExtent2D(10,20),
										VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
										Vec4(0,0,0,0), Vec4(0,1,0,0));
	}
	void createFileBufferProc ()
	{
		ZDevice device = pool.getParam<ZDevice>();
		fileBuffer = createBufferAndLoadFromImageFile(device, imageFile, ZBufferUsageFlags());
		fileBufferHandle.store(*fileBuffer);
	}

	void phaseCreateFileBuffer (add_ref<bool> repeat)
	{
		updateSplashImage(splashProgress++);
		if (!fileThreadActive)
		{
			fileThreadActive = true;
			fileThread.emplace_back(&ImageLoader::createFileBufferProc, this);
			fileThread.back().detach();
		}
		repeat = (VK_NULL_HANDLE == fileBufferHandle.load());
	}
	void phaseCreatePanoramaImage (add_ref<bool>)
	{
		ZDevice device = fileBuffer.getParam<ZDevice>();
		panImage = createImageFromFileMetadata(device, fileBuffer, usage);
		format = imageGetFormat(panImage);
		updateSplashImage(40);
	}
	void phaseCreateTemporaryImageAndSetupSizes (add_ref<bool>)
	{
		ZDevice device = pool.getParam<ZDevice>();
		add_cref<VkExtent3D> extent = imageGetExtent(panImage);
		imageWidth		= extent.width;
		imageHeight		= extent.height;
		if (getGlobalAppFlags().verbose)
		{
			std::cout << "Assume image height is two times less than its width "
					  << "and the width itself is dividable by 4" << std::endl;
		}
		if (((imageWidth % 4u) != 0u) || ((imageHeight * 2) != imageWidth))
		{
			imageWidth = ROUNDUP(extent.width, 8);
			imageHeight = imageWidth / 2;
			if (getGlobalAppFlags().verbose)
			{
				std::cout << "Unfortunatelly not, resize "
						  << extent.width << 'x' << extent.height << " to "
						  << imageWidth << 'x' << imageHeight << std::endl;
			}
			tmpImage = createImage(device, format, VK_IMAGE_TYPE_2D, imageWidth, imageHeight, usage);
		}

		local_size_xy	= 1u;
		faceSize		= imageWidth / 4;
		const uint32_t divisors[] = { 2, 4, 8, 16, 32 };
		for (uint32_t divisor : divisors)
		{
			if ((faceSize % divisor) == 0u)
				local_size_xy = divisor;
		}
		updateSplashImage(60);
	}
	void phaseCreateComputePipeline (add_ref<bool>)
	{
		ZDevice					device				= pool.getParam<ZDevice>();
		ZImageView				panView				= createImageView(tmpImage.has_handle() ? tmpImage : panImage);
								panBinding			= layout.addBinding(panView, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		ZImage					cubeImage			= createImage(device, format, VK_IMAGE_TYPE_2D, faceSize, faceSize,
																  usage, VK_SAMPLE_COUNT_1_BIT, 1u, 6u);
								cubeView			= createImageView(cubeImage, VK_FORMAT_UNDEFINED, VK_IMAGE_VIEW_TYPE_MAX_ENUM,
																		VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM, 0u, 1u, 0u, 6u);
								cubeBinding			= layout.addBinding(cubeView, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		ZDescriptorSetLayout	descriptorSetLayout	= layout.createDescriptorSetLayout();
		ZPipelineLayout			pipelineLayout		= layout.createPipelineLayout({ descriptorSetLayout },
																					ZPushRange<PushContant>());

		if (getGlobalAppFlags().verbose)
		{
			std::cout << "[INFO] Create descriptor set:"
					  << "\n         panorama:VK_DESCRIPTOR_TYPE_STORAGE_IMAGE = " << panBinding
					  << "\n         cube:VK_DESCRIPTOR_TYPE_STORAGE_IMAGE = " << cubeBinding
					  << std::endl;
		}

		computePipeline = createComputePipeline(pipelineLayout, faceModule, {}, UVec3(local_size_xy, local_size_xy, 1u));
		updateSplashImage(80);
	}
	void phaseMakeOutputImage (add_ref<bool>)
	{
		ZImage					cubeImage			= imageViewGetImage(cubeView);
		VkPipelineStageFlags	stageCompute		= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		VkPipelineStageFlags	stageTransfer		= VK_PIPELINE_STAGE_TRANSFER_BIT;
		PushContant				pc					{ Vec4(imageWidth, imageHeight, faceSize, faceSize) };
		VkImageSubresourceRange	cubeRange			= imageMakeSubresourceRange(cubeImage, 0u, 1u, 0u, 6u);
		ZImageMemoryBarrier		preCubeBarrier		(cubeImage, VK_ACCESS_NONE, VK_ACCESS_SHADER_WRITE_BIT,
													 VK_IMAGE_LAYOUT_GENERAL, cubeRange);
		ZImageMemoryBarrier		postCubeBarrier		(cubeImage, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
													 VK_IMAGE_LAYOUT_GENERAL, cubeRange);
		ZImageMemoryBarrier		preImageBarrier		(tmpImage.has_handle() ? tmpImage : panImage,
													 VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		ZImageMemoryBarrier		postImageBarrier	(tmpImage.has_handle() ? tmpImage : panImage,
													 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL);
		ZPipelineLayout			pipelineLayout		= pipelineGetLayout(computePipeline);
		ZCommandBuffer			cmdBuffer			= allocateCommandBuffer(pool);

		commandBufferBegin(cmdBuffer);
		commandBufferBindPipeline(cmdBuffer, computePipeline);
		bufferCopyToImage(cmdBuffer, fileBuffer, panImage,
						  VK_ACCESS_NONE, VK_ACCESS_NONE,
						  VK_ACCESS_NONE, (tmpImage.has_handle() ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_SHADER_READ_BIT),
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (tmpImage.has_handle() ? stageTransfer : stageCompute),
						  (tmpImage.has_handle() ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL));
		if (tmpImage.has_handle())
		{
			commandBufferPipelineBarriers(cmdBuffer, stageTransfer, stageTransfer, preImageBarrier);
			commandBufferBlitImage(cmdBuffer, panImage, tmpImage);
			commandBufferPipelineBarriers(cmdBuffer, stageTransfer, stageCompute, postImageBarrier);
		}
		commandBufferPipelineBarriers(cmdBuffer, stageCompute, stageCompute, preCubeBarrier);
		commandBufferPushConstants(cmdBuffer, pipelineLayout, pc);
		commandBufferDispatch(cmdBuffer, UVec3((faceSize / local_size_xy), (faceSize / local_size_xy), 6u));
		commandBufferPipelineBarriers(cmdBuffer, stageCompute, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, postCubeBarrier);
		commandBufferEnd(cmdBuffer);
		commandBufferSubmitAndWait(cmdBuffer);

		outputImage.width	= faceSize;
		outputImage.height	= faceSize;
		outputImage.view	= cubeView;
		outputImage			= cubeImage;
	}
	void phaseCreateRegularImage (add_ref<bool>)
	{
		ZDevice		device	= fileBuffer.getParam<ZDevice>();
		outputImage			= createImageFromFileMetadata(device, fileBuffer);
		outputImage.width	= imageGetExtent(outputImage).width;
		outputImage.height	= imageGetExtent(outputImage).height;
		OneShotCommandBuffer shot(pool);
		bufferCopyToImage(shot.commandBuffer, fileBuffer, outputImage,
						  VK_ACCESS_NONE, VK_ACCESS_NONE,
						  VK_ACCESS_NONE, VK_ACCESS_TRANSFER_READ_BIT,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
						  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	}
};

bool ZImageEx::load(ZCommandPool cmdPool,
					add_cref<std::string> imageFileName,
					uint32_t splashWidth, uint32_t splashHeight,
					bool isPanorama, ZShaderModule faceShaderModule)
{
	bool result = false;
	add_ref<ZImageEx> image = *this;
	if (false == loaded)
	{
		if (nullptr == imageLoader.get())
		{
			result = true;
			imageLoader = std::make_unique<ImageLoader>(cmdPool, std::ref(image), std::cref(imageFileName),
														splashWidth, splashHeight, isPanorama, faceShaderModule);
		}
		else
		{
			result = imageLoader->operator()();
			if (false == result)
			{
				imageLoader.reset(nullptr);
				loaded = true;
			}
		}
	}
	return result;
}

void populateVertexInput (add_ref<VertexInput> input)
{
	/*                             /|\                     0 --- 1
	*                       /       |    X    \            |  T  |
	*         4 --- 5       --------*----------            |  P  |
	*       / |   / |       \      /|         /      0 --- 4 --- 5 --- 1 --- 0
	*      0 --- 1  |             / |                |  L  |  F  |  R  |  B  |
	*      |  7 -|- 6          Z /  | Y              |  T  |  T  |  T  |  K  |
	*      | /   | /            /   |                3 --- 7 --- 6 --- 2 --- 3
	*      3 --- 2            |/_  \|/                     |  B  |
	*                                                      |  T  |
	*                                                      3 --- 2
	* Order: right, left, top, bottom, back, front
	*/

	const std::vector<Vec3> v {
		{ -1, +1, 0 }, { -1, -1, 0 }, { +1, -1, 0 },
		{ +1, -1, 0 }, { +1, +1, 0 }, { -1, +1, 0 }
	};

	const std::vector<Vec3> n {
		{ -1, -1, -1 }, { -1, +1, -1 }, { +1, +1, -1 },
		{ +1, +1, -1 }, { +1, -1, -1 }, { -1, -1, -1 }

	};

	input.binding(0).addAttributes(v, n);
}

TriLogicInt runViewerSingleThread (add_ref<Canvas> cs, add_cref<std::string> assets,
						   add_cref<std::vector<ImageFileInfo> > files, const int32_t regularCount, const int32_t panoramaCount)
{
	UserData		userData	(static_cast<uint32_t>(files.size()));
	ZRenderPass		renderPass	= cs.createSinglePresentationRenderPass();

	ZShaderModule	faceShader	{};
	ZShaderModule	vertShader	{};
	ZShaderModule	fragShader	{};
	LayoutManager	layoutMgr	(cs.device);
	ZSampler		sampler		{};
	VertexInput		vertexInput	(cs.device);
	ZPipeline		panPipeline	{};
	UNREF(regularCount);
	if (panoramaCount)
	{
		const strings				includes{ "." };
		ProgramCollection			programs(cs.device, assets);
		programs.addFromFile(VK_SHADER_STAGE_COMPUTE_BIT,  "face.comp", includes);
		programs.addFromFile(VK_SHADER_STAGE_VERTEX_BIT,   "pan.vert", includes);
		programs.addFromFile(VK_SHADER_STAGE_FRAGMENT_BIT, "pan.frag", includes);
		const GlobalAppFlags		flags(getGlobalAppFlags());
		programs.buildAndVerify(flags.vulkanVer, flags.spirvVer, flags.spirvValidate, flags.genSpirvDisassembly);
		faceShader = programs.getShader(VK_SHADER_STAGE_COMPUTE_BIT);
		vertShader = programs.getShader(VK_SHADER_STAGE_VERTEX_BIT);
		fragShader = programs.getShader(VK_SHADER_STAGE_FRAGMENT_BIT);

		populateVertexInput(vertexInput);
		sampler	= createSampler(cs.device);
		layoutMgr.addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		ZDescriptorSetLayout	dsLayout	= layoutMgr.createDescriptorSetLayout(false);
		ZPipelineLayout			panLayout	= layoutMgr.createPipelineLayout({ dsLayout }, ZPushRange<PushConstant>());
		panPipeline = createGraphicsPipeline(panLayout, renderPass, vertShader, fragShader, vertexInput,
											 VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR);
	};

	cs.events().cbKey.set(onKey, &userData);
	cs.events().cbScroll.set(onScroll, &userData);
	cs.events().cbWindowSize.set(onResize, &userData);
	cs.events().cbCursorPos.set(onCursorPos, &userData);
	cs.events().cbMouseButton.set(onMouseBtn, &userData);

	bool status = false;
	std::map<std::string, ZImageEx> images;

	auto onCommandRecording = [&](add_ref<Canvas>, add_cref<Canvas::Swapchain> swapchain, ZCommandBuffer cmdBuffer, ZFramebuffer framebuffer)
	{
		add_cref<ImageFileInfo> fileInfo = files[userData.getImageIndex()];
		ZImage dstImage	= framebufferGetImage(framebuffer);
		add_ref<ZImageEx> srcImage = images[fileInfo.fileName + (fileInfo.panorama ? "_pan" : "_reg")];

		if (status = srcImage.load(commandBufferGetCommandPool(cmdBuffer), fileInfo.fileName,
									cs.width, cs.height, fileInfo.panorama, faceShader); status)
		{
			userData.drawTrigger = userData.drawTrigger + 1;
		}
		cs.events().cbKey.enable(!status);
		cs.events().cbScroll.enable(!status);
		cs.events().cbWindowSize.enable(!status);
		cs.events().cbCursorPos.enable(!status);
		cs.events().cbMouseButton.enable(!status);

		if (srcImage != userData.image)
		{
			userData.image = srcImage;
			userData.pc = fileInfo.panorama ? &srcImage.pc : nullptr;

		}

		commandBufferBegin(cmdBuffer);
		if (!status && fileInfo.panorama)
		{
			auto pipeLayout		= pipelineGetLayout(panPipeline);
			auto dsLayout		= layoutMgr.getDescriptorSetLayout(pipeLayout);
			auto descriptorSet	= layoutMgr.getDescriptorSet(dsLayout);
			layoutMgr.updateDescriptorSet(descriptorSet, 0u, srcImage.view, sampler);
			commandBufferClearColorImage(cmdBuffer, dstImage, makeClearColorValue(Vec4()));
			commandBufferBindPipeline(cmdBuffer, panPipeline);
			commandBufferPushConstants(cmdBuffer, pipeLayout, srcImage.pc);
			commandBufferBindVertexBuffers(cmdBuffer, vertexInput);
			vkCmdSetViewport(*cmdBuffer, 0, 1, &swapchain.viewport);
			vkCmdSetScissor(*cmdBuffer, 0, 1, &swapchain.scissor);
			auto rpbi = commandBufferBeginRenderPass(cmdBuffer, framebuffer);
				vkCmdDraw(*cmdBuffer, vertexInput.getVertexCount(0), 1u, 0u, 0u);
			commandBufferEndRenderPass(rpbi);
		}
		else
		{
			ZImageMemoryBarrier	srcPreBlit	(srcImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_READ_BIT,
											 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			ZImageMemoryBarrier	dstPreBlit	(dstImage, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
											 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			ZImageMemoryBarrier	postBlit (srcImage, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_NONE,
										  VK_IMAGE_LAYOUT_GENERAL);
			commandBufferPipelineBarriers(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, srcPreBlit, dstPreBlit);
			commandBufferBlitImage(cmdBuffer, srcImage, dstImage);
			commandBufferPipelineBarriers(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, postBlit);
		}
		commandBufferMakeImagePresentationReady(cmdBuffer, dstImage);
		commandBufferEnd(cmdBuffer);
	};

	return cs.run(onCommandRecording, renderPass, std::ref(userData.drawTrigger));
}

} // unnamed namespace

template<> struct TestRecorder<VIEWER>
{
	static bool record (TestRecord&);
};
bool TestRecorder<VIEWER>::record (TestRecord& record)
{
	record.name = "viewer";
	record.call = &prepareTests;
	return true;
}

