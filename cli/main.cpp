#include "AtlasGenerator/Generator.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

#include "exception/GeneralRuntimeException.h"
using namespace sc;

#define print(message) std::cout << message << std::endl

SC_CONSTRUCT_CHILD_EXCEPTION(GeneralRuntimeException, FolderAlreadyExistException, "Output folder already exists");

void print_help(char* executable)
{
	print("Sc Atlas Generator Command Line App: ");
	print("Usage: " << executable << " [Folder name with output] ...args");
	print("Arguments: Paths to images or to folder with image files");
	print("Flags: ");
	print("--force: rewrite output folder even if it already exists");
	print("--debug: draws and shows polygon for each item");
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

			// Paths
			if (!fs::exists(argument))
			{
				print("Unknown or wrong agument" << argument);
			}

			if (fs::is_directory(argument))
			{
				for (fs::path path : fs::directory_iterator(argument))
				{
					files.push_back(path);
				}
			}
			else
			{
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
};

#pragma region CV Debug Functions
inline cv::RNG rng = cv::RNG(time(NULL));

void ShowImage(std::string name, cv::Mat& image) {
	cv::namedWindow(name, cv::WINDOW_NORMAL);

	cv::imshow(name, image);
	cv::waitKey(0);
}

void ShowContour(cv::Mat& src, std::vector<cv::Point>& points) {
	cv::Mat drawing = src.clone();
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
			throw FolderAlreadyExistException();
		}
	}

	fs::create_directory(options.output);

	fs::path atlas_data_output = options.output / "atlas.txt";
	std::ofstream atlas(atlas_data_output);

	std::vector<AtlasGenerator::Item> items;

	for (fs::path& path : options.files)
	{
		items.emplace_back(
			path
		);
	}

	AtlasGenerator::Config config(
		AtlasGenerator::Config::TextureType::RGBA,
		4096, 4096,
		1, 2
	);

	AtlasGenerator::Generator generator(config);

	uint8_t bin_count = generator.generate(items);

	for (uint8_t i = 0; bin_count > i; i++)
	{
		cv::Mat& image = generator.get_atlas(i);

		cv::imwrite(
			fs::path(options.output / fs::path("atlas_").concat(std::to_string(i)).concat(".png")).string(),
			image
		);
	}

	{
		for (size_t i = 0; items.size() > i; i++)
		{
			AtlasGenerator::Item& item = items[i];
			fs::path& path = options.files[i];

			atlas << "path=" << path << std::endl;
			atlas << "textureIndex=" << std::to_string(item.texture_index) << std::endl;

			atlas << "uv=";
			for (AtlasGenerator::Vertex& vertex : item.vertices)
			{
				atlas << "[";
				atlas << std::to_string(vertex.uv.x);
				atlas << ",";
				atlas << std::to_string(vertex.uv.y);
				atlas << "]";
			}

			atlas << std::endl;

			atlas << "xy=";
			for (AtlasGenerator::Vertex& vertex : item.vertices)
			{
				atlas << "[";
				atlas << std::to_string(vertex.xy.x);
				atlas << ",";
				atlas << std::to_string(vertex.xy.y);
				atlas << "]";
			}

			atlas << std::endl << std::endl;
		}
	}

	if (options.is_debug)
	{
		std::vector<cv::Mat> sheets;
		cv::RNG rng = cv::RNG(time(NULL));

		for (uint8_t i = 0; bin_count > i; i++) {
			cv::Mat& atlas = generator.get_atlas(i);
			sheets.emplace_back(
				atlas.size(),
				CV_8UC4,
				cv::Scalar(0)
			);
		}

		for (AtlasGenerator::Item& item : items) {
			std::vector<cv::Point> contour;
			for (AtlasGenerator::Vertex point : item.vertices) {
				contour.push_back(cv::Point(point.uv.x, point.uv.y));
			}
			cv::Scalar color(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255));
			fillPoly(sheets[item.texture_index], contour, color);
		}

		for (cv::Mat& sheet : sheets) {
			ShowImage("Sheet", sheet);
		}

		for (uint8_t i = 0; bin_count > i; i++) {
			ShowImage("Atlas", generator.get_atlas(i));
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
	catch (const GeneralRuntimeException& exception)
	{
		print(exception.message());
		return 1;
	}

	return 0;
}