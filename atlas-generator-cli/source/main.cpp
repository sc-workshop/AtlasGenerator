#include "atlas_generator/Generator.h"

#include "core/stb/stb.h"
#include "core/io/file_stream.h"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <core/time/timer.h>
namespace fs = std::filesystem;

#include "atlas_generator/PackagingException.h"
using namespace wk;
using namespace AtlasGenerator;

#define print(message) std::cout << message << std::endl

static void print_help(char* executable)
{
	print("Sc Atlas Generator Command Line App: ");
	print("Usage: " << executable << " [Folder name with output] ...args");
	print("Arguments: Paths to images or to folder with image files");
	print("Flags: ");
	print("--force: rewrite output folder even if it already exists");
	print("--debug: draws and shows atlas of polygons and atlas itself");
	print("--item-debug: draws and shows polygon for each item");
}

class ProgramOptions
{
public:
	ProgramOptions(int argc, char* argv[])
	{
		output = argv[1];

		for (int i = last_argument_index(); argc > i; i++)
		{
			std::string argument = argv[i];

			// Flags
			if (argument == "--force")
			{
				force_output = true;
				continue;
			}

			if (argument == "--debug")
			{
				is_debug = true;
				continue;
			}

			if (argument == "--item-debug")
			{
				is_item_debug = true;
				continue;
			}

			// Paths
			if (!fs::exists(argument))
			{
				print("Unknown or wrong agument " << argument);
			}

			auto valid_path = [](fs::path path)
				{
					if (path.extension() == ".png") return true;

					return false;
				};

			if (fs::is_directory(argument))
			{
				for (fs::path path : fs::directory_iterator(argument))
				{
					if (valid_path(path))
						files.push_back(path);
				}
			}
			else
			{
				if (valid_path(argument))
					files.push_back(argument);
			}
		}
	}

	static int last_argument_index()
	{
		//	  0       1		2
		// File.exe output paths
		return 2;
	}

public:
	fs::path output;
	std::vector<fs::path> files;
	bool force_output = false;
	bool is_debug = false;
	bool is_item_debug = false;
};

#pragma region CV Debug Functions

void grayAlpha_to_rgba(cv::Mat& input, cv::Mat& output)
{
	cv::Mat gray, gray_alpha;

	cv::extractChannel(input, gray, 0);
	cv::extractChannel(input, gray_alpha, 1);

	std::vector<cv::Mat> channels({ gray, gray, gray, gray_alpha });

	cv::merge(channels, output);
}

void ShowImage(std::string name, cv::Mat& image) {
	cv::namedWindow(name, cv::WINDOW_NORMAL);

	if (image.channels() == 2)
	{
		cv::Mat result;
		grayAlpha_to_rgba(image, result);
		cv::imshow(name, result);
	}
	else
	{
		cv::imshow(name, image);
	}

	cv::waitKey(0);
}

void ShowContour(cv::Mat& src, std::vector<cv::Point> points) {
	const float scale_factor = 8.0f;

	cv::Mat drawing = src.clone();
	cv::resize(drawing, drawing, cv::Size(), scale_factor, scale_factor, cv::INTER_NEAREST);

	for (cv::Point& point : points)
	{
		point.x *= scale_factor;
		point.y *= scale_factor;
	}

	drawContours(
		drawing,
		std::vector<std::vector<cv::Point>>(1, points),
		0,
		cv::Scalar(255, 255, 0),
		5,
		cv::LINE_AA
	);

	for (cv::Point& point : points) {
		circle(
			drawing,
			point,
			2,
			{ 0, 0, 255 },
			2,
			cv::LINE_AA
		);
	}
	ShowImage("Image polygon", drawing);
	cv::destroyAllWindows();
}


void ShowContour(cv::Mat& src, std::vector<wk::Point>& points) {
	std::vector<cv::Point> cvPoints;
	for (auto& point : points) {
		cvPoints.push_back({ point.x, point.y });
	}

	ShowContour(src, cvPoints);
}

void ShowContour(cv::Mat& src, std::vector<AtlasGenerator::Vertex>& points) {
	std::vector<cv::Point> cvPoints;
	for (auto& point : points) {
		cvPoints.push_back({ point.uv.x, point.uv.y });
	}

	ShowContour(src, cvPoints);
}

#pragma endregion

void process(ProgramOptions& options)
{
	if (fs::exists(options.output) && fs::is_directory(options.output))
	{
		if (options.force_output)
		{
			fs::remove_all(options.output);
		}
		else
		{
			throw Exception("Folder already exist");
		}
	}

	fs::create_directory(options.output);

	fs::path atlas_data_output = options.output / "atlas.txt";
	std::ofstream atlas_data(atlas_data_output);

	std::vector<AtlasGenerator::Item> items;
	items.reserve(options.files.size());

	std::map<size_t, Rect> guides;
	std::map<size_t, AtlasGenerator::Item::Transformation> guide_transforms;

	for (fs::path& path : options.files)
	{
		if (path.extension() != ".png") continue;

		std::string basename = fs::path(path.filename()).replace_extension().string();
		fs::path guide_path = fs::path(path).replace_extension().concat("_guide.txt");

		if (!fs::exists(guide_path))
		{
			if (basename.size() > 3 && basename.substr(basename.size() - 3)== "_la")
			{
				InputFileStream file(path);
				RawImageRef image;
				stb::load_image(file, image);

				RawImage gray(image->width(), image->height(), Image::PixelDepth::LUMINANCE8_ALPHA8);
				image->copy(gray);

				items.emplace_back(gray);
			}
			else
			{
				items.emplace_back(path);
			}
		}
		else
		{
			std::vector<float> guide;
			std::ifstream guide_file(guide_path);

			std::string line;
			while (std::getline(guide_file, line, '\n'))
			{
				guide.push_back(
					std::stof(line)
				);
			}

			if (guide.size() != 4) continue;

			guides[items.size()] = Rect(
				(int32_t)ceil(guide[0]), (int32_t)ceil(guide[3]),
				(int32_t)ceil(guide[1]), (int32_t)ceil(guide[2])
				);

			AtlasGenerator::Item item(path, true);

			AtlasGenerator::Item::Transformation transform(
				0.0,
				Point(-(item.width() / 2), -(item.height() / 2))
			);

			guide_transforms[items.size()] = transform;

			items.push_back(item);
		}
	}

	//std::vector<cv::Mat> original_images;
	//if (options.is_item_debug)
	//{
	//	for (Item& item : items)
	//	{
	//		original_images.emplace_back(item.image());
	//	}
	//}

	uint8_t scale_factor = 1;
	AtlasGenerator::Config config(
		4096, 4096,
		scale_factor, 2
	);

	config.progress = [&items](size_t count)
	{
		std::cout << std::string(100, '\b') << count + 1 << "\\" << items.size() << std::flush;
	};

	size_t bin_count = 0;
	AtlasGenerator::Generator generator(config);
	{
		Timer timer;
		std::cout << "0\\" << items.size();
		try
		{
			bin_count = generator.generate(items);
			std::cout << std::endl;
		}
		catch (const AtlasGenerator::PackagingException& exception)
		{
			std::cout << std::endl;
			size_t item_index = exception.index();
			if (item_index == SIZE_MAX)
			{
				std::cout << "Unknown package exception" << std::endl;
			}
			else
			{
				std::cout << "Failed to package item \"" << options.files[item_index] << "\"" << std::endl;
			}
			std::cout << exception.what() << std::endl;
			return;
		}
		catch (const std::exception& exception)
		{
			std::cout << std::endl;
			std::cout << exception.what() << std::endl;
			return;
		}
		print("Packaging done by " << (double)timer.elapsed() / 1000 << "s");
	}

	for (uint8_t i = 0; bin_count > i; i++)
	{
		RawImage& image = generator.get_atlas(i);
		std::string destination = fs::path(options.output / fs::path("atlas_").concat(std::to_string(i)).concat(".png")).string();

		wk::OutputFileStream file(destination);
		wk::stb::write_image(image, wk::stb::ImageFormat::PNG, file);
	}

	for (size_t i = 0; items.size() > i; i++)
	{
		AtlasGenerator::Item& item = items[i];
		fs::path& path = options.files[i];

		atlas_data << "path=" << path << std::endl;
		atlas_data << "textureIndex=" << std::to_string(item.texture_index) << std::endl;

		atlas_data << "uv=";
		for (AtlasGenerator::Vertex vertex : item.vertices)
		{
			item.transform.transform_point(vertex.uv);
			atlas_data << "[";
			atlas_data << std::to_string(vertex.uv.x / scale_factor);
			atlas_data << ",";
			atlas_data << std::to_string(vertex.uv.y / scale_factor);
			atlas_data << "]";
		}

		atlas_data << std::endl;

		atlas_data << "xy=";
		for (AtlasGenerator::Vertex& vertex : item.vertices)
		{
			atlas_data << "[";
			atlas_data << std::to_string(vertex.xy.x);
			atlas_data << ",";
			atlas_data << std::to_string(vertex.xy.y);
			atlas_data << "]";
		}

		atlas_data << std::endl << std::endl;
	}

	if (options.is_debug)
	{
		std::vector<cv::Mat> sheets;
		cv::RNG rng = cv::RNG(time(NULL));

		for (uint8_t i = 0; bin_count > i; i++) {
			RawImage& atlas = generator.get_atlas(i);

			int type = 0;
			switch (atlas.depth())
			{
			case Image::PixelDepth::RGBA8:
				type = CV_8UC4;
				break;
			case Image::PixelDepth::RGB8:
				type = CV_8UC3;
				break;
			case Image::PixelDepth::LUMINANCE8_ALPHA8:
				type = CV_8UC2;
				break;
			case Image::PixelDepth::LUMINANCE8:
				type = CV_8UC1;
				break;
			}

			cv::Mat& mat = sheets.emplace_back(
				cv::Mat(atlas.height(), atlas.width(), type,
				atlas.data())
			);

			switch (atlas.depth())
			{
			case Image::PixelDepth::RGBA8:
				cv::cvtColor(mat, mat, cv::COLOR_RGBA2BGRA);
				break;
			case Image::PixelDepth::RGB8:
				cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
				break;
			default:
				break;
			}
		}

		for (uint8_t i = 0; bin_count > i; i++) {
			ShowImage("Atlas", sheets[i]);
		}

		for (size_t i = 0; items.size() > i; i++) {
			AtlasGenerator::Item& item = items[i];
			Item::Transformation& transform = item.transform;
			std::vector<cv::Point> atlas_contour;
			std::vector<cv::Point> item_contour;
			fs::path& path = options.files[i];

			for (AtlasGenerator::Vertex vertex : item.vertices) {
				transform.transform_point(vertex.uv);

				atlas_contour.push_back(cv::Point(vertex.uv.x, vertex.uv.y));
				item_contour.push_back(cv::Point(vertex.xy.x, vertex.xy.y));
			}

			{
				cv::Scalar color(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255));
				fillPoly(sheets[item.texture_index], atlas_contour, color);
			}
		}

		for (cv::Mat& sheet : sheets) {
			ShowImage("Sheet", sheet);
		}

		cv::destroyAllWindows();
	}
}

int main(int argc, char* argv[])
{
	if (argc <= ProgramOptions::last_argument_index())
	{
		print_help(argv[0]);
		return 1;
	}

	ProgramOptions options(argc, argv);

	try
	{
		process(options);
	}
	catch (const std::exception& exception)
	{
		print(exception.what());
		return 1;
	}

	return 0;
}