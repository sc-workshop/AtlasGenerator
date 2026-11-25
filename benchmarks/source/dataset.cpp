#include "dataset.h"

#include <fstream>
#include <string>

#include "core/io/file_stream.h"
#include "core/stb/stb.h"

using namespace wk;
using namespace wk::AtlasGenerator;

namespace atlas_bench
{
	namespace
	{
		bool has_valid_guide(const std::filesystem::path& png_path) {
			std::filesystem::path guide_path = png_path;
			guide_path.replace_extension().concat("_guide.txt");
			if (!std::filesystem::exists(guide_path)) return false;

			std::ifstream guide_file(guide_path);
			std::vector<float> guide;
			std::string line;
			while (std::getline(guide_file, line, '\n')) {
				try {
					guide.push_back(std::stof(line));
				}
				catch (const std::exception&) {
					return false;
				}
			}

			return guide.size() == 4;
		}
	}

	LoadedSet load_items(const std::filesystem::path& directory) {
		LoadedSet result;

		for (const auto& entry : std::filesystem::directory_iterator(directory)) {
			const std::filesystem::path path = entry.path();
			if (path.extension() != ".png") continue;

			RawImageRef image;
			try {
				InputFileStream file(path);
				stb::load_image(file, image);
			}
			catch (const std::exception&) {
				++result.skipped;
				continue;
			}

			if (!image || image->width() < 1 || image->height() < 1) {
				++result.skipped;
				continue;
			}

			const bool sliced = has_valid_guide(path);

			const std::string basename = path.filename().replace_extension().string();
			if (basename.size() > 3 && basename.substr(basename.size() - 3) == "_la") {
				RawImage luminance_alpha(
					static_cast<uint16_t>(image->width()),
					static_cast<uint16_t>(image->height()),
					Image::PixelDepth::LUMINANCE8_ALPHA8
				);
				image->copy(luminance_alpha);
				result.items.emplace_back(luminance_alpha, sliced);
			}
			else {
				result.items.emplace_back(*image, sliced);
			}

			if (sliced) {
				Item& item = result.items.back();
				item.transform = Item::Transformation(
					0.0,
					Point(-(static_cast<int32_t>(item.width()) / 2), -(static_cast<int32_t>(item.height()) / 2))
				);
			}
		}

		return result;
	}

	size_t count_png(const std::filesystem::path& directory) {
		size_t count = 0;
		for (const auto& entry : std::filesystem::directory_iterator(directory)) {
			if (entry.path().extension() == ".png") ++count;
		}
		return count;
	}
}
