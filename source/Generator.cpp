#include "AtlasGenerator/Generator.h"
#include "AtlasGenerator/limits.h"

#include <libnest2d/libnest2d.hpp>

namespace sc
{
	namespace AtlasGenerator
	{
		Generator::Generator(const Config& config) : m_config(config)
		{
		}

		uint8_t Generator::generate(Container<Item>& items)
		{
			Container<size_t> inverse_duplicate_indices;
			for (size_t i = 0; items.size() > i; i++)
			{
				Item& item = items[i];

				if (item.image().channels() != 4)
				{
					throw PackagingException(PackagingException::Reason::UnsupportedImage, i);
				}

				// Searching for duplicates
				{
					size_t item_index = SIZE_MAX;
					for (size_t j = 0; i > j; j++)
					{
						if (items[j] == item)
						{
							item_index = j;
							break;
						}
					}
					m_duplicate_indices.push_back(item_index);
					if (item_index != SIZE_MAX) continue;
				}

				inverse_duplicate_indices.push_back(i);
				m_items.push_back(&item);
			}

			for (size_t i = 0; m_items.size() > i; i++)
			{
				Item* item = m_items[i];

				if (item->status() == Item::Status::Unset)
				{
					item->generate_image_polygon(m_config);
					if (item->vertices.empty())
					{
						throw PackagingException(PackagingException::Reason::InvalidPolygon, inverse_duplicate_indices[i]);
					}

					if (item->width() > m_config.width() || item->height() > m_config.height())
					{
						throw PackagingException(PackagingException::Reason::TooBigImage, inverse_duplicate_indices[i]);
					}
				}
			}

			// Sorting by perimeter
			std::stable_sort(m_items.begin(), m_items.end(),
				[](Item* a, Item* b) {
					return (*a).perimeter() > (*b).perimeter();
				}
			);

			if (!pack_items())
			{
				throw PackagingException(PackagingException::Reason::Unknown);
			};

			for (size_t i = 0; items.size() > i; i++)
			{
				size_t item_index = m_duplicate_indices[i];

				if (item_index != SIZE_MAX)
				{
					items[i] = items[item_index];
				}
			}

			m_duplicate_indices.clear();
			m_items.clear();

			return (uint8_t)m_atlases.size();
		}

		cv::Mat& Generator::get_atlas(uint8_t atlas)
		{
			return m_atlases[atlas];
		}

		bool Generator::pack_items()
		{
			// Vector with polygons for libnest2d
			std::vector<libnest2d::Item> packer_items;

			for (Item* item : m_items)
			{
				libnest2d::Item& packer_item = packer_items.emplace_back(
					std::vector<ClipperLib::IntPoint>(item->vertices.size() + 1)
				);

				for (uint16_t i = 0; packer_item.vertexCount() > i; i++) {
					if (i == item->vertices.size()) { // End point for libnest
						packer_item.setVertex(i, { item->vertices[0].uv.x, item->vertices[0].uv.y });
					}
					else {
						packer_item.setVertex(i, { item->vertices[i].uv.x, item->vertices[i].uv.y });
					}
				}
			}

			libnest2d::NestConfig<libnest2d::BottomLeftPlacer, libnest2d::DJDHeuristic> cfg;
			libnest2d::NestControl control;
			control.progressfn = m_config.progress;

			size_t bin_count = nest(
				packer_items,
				libnest2d::Box(
					m_config.width(), m_config.height(),
					{ (int)ceil(m_config.width() / 2), (int)ceil(m_config.height() / 2) }),
				m_config.extrude(), cfg, control
			);

			// Gathering texture size info
			std::vector<cv::Size> sheet_size(bin_count);
			for (libnest2d::Item item : packer_items) {
				if (item.binId() == libnest2d::BIN_ID_UNSET)
				{
					return false;
				};

				auto shape = item.transformedShape();
				auto box = item.boundingBox();

				cv::Size& size = sheet_size[item.binId()];

				auto x = libnest2d::getX(box.maxCorner());
				auto y = libnest2d::getY(box.maxCorner());

				if (x > size.height) {
					size.height = (int)x;
				}
				if (y > size.width) {
					size.width = (int)y;
				}
			}

			for (cv::Size& size : sheet_size)
			{
				int type = CV_8UC4;
				switch (m_config.type())
				{
				case Config::TextureType::RGB:
					type = CV_8UC3;
					break;
				case Config::TextureType::LUMINANCE_ALPHA:
					type = CV_8UC2;
					break;
				case Config::TextureType::LIMINANCE:
					type = CV_8UC1;
					break;
				default:
					break;
				}

				m_atlases.emplace_back(
					size.width,
					size.height,
					type,
					cv::Scalar(0)
				);
			}

			for (size_t i = 0; m_items.size() > i; i++) {
				libnest2d::Item packer_item = packer_items[i];
				Item& item = *m_items[i];

				auto rotation = packer_item.rotation();
				double rotation_degree = -(rotation.toDegrees());

				auto shape = packer_item.transformedShape();
				auto box = packer_item.boundingBox();

				// Item Data
				item.texture_index = (uint8_t)packer_item.binId();
				item.transform.rotation = rotation;
				item.transform.translation.x = (int32_t)libnest2d::getX(packer_item.translation()) - m_config.extrude();
				item.transform.translation.y = (int32_t)libnest2d::getY(packer_item.translation()) - m_config.extrude();

				// Image Transform
				if (rotation_degree != 0) {
					cv::Point2f center((float)((item.width() - 1) / 2.0), (float)((item.height() - 1) / 2.0));
					cv::Mat rot = cv::getRotationMatrix2D(center, rotation_degree, 1.0);
					cv::Rect2f bbox = cv::RotatedRect(cv::Point2f(), item.image().size(), (float)rotation_degree).boundingRect2f();

					rot.at<double>(0, 2) += bbox.width / 2.0 - item.width() / 2.0;
					rot.at<double>(1, 2) += bbox.height / 2.0 - item.height() / 2.0;

					cv::warpAffine(item.image(), item.image(), rot, bbox.size(), cv::INTER_NEAREST);
				}

				cv::copyMakeBorder(item.image(), item.image(), m_config.extrude(), m_config.extrude(), m_config.extrude(), m_config.extrude(), cv::BORDER_REPLICATE);

				place_image_to(
					item.image(),
					item.texture_index,
					static_cast<uint16_t>(libnest2d::getX(box.minCorner()) - m_config.extrude() * 2),
					static_cast<uint16_t>(libnest2d::getY(box.minCorner()) - m_config.extrude() * 2)
				);
			}

			return true;
		}

		void Generator::place_image_to(cv::Mat& src, uint8_t atlas_index, uint16_t x, uint16_t y)
		{
			using namespace cv;

			cv::Mat& dst = m_atlases[atlas_index];

			Size srcSize = src.size();
			Size dstSize = dst.size();

			for (uint16_t h = 0; srcSize.height > h; h++) {
				uint16_t dstH = h + y;
				if (dstH >= dstSize.height) continue;

				for (uint16_t w = 0; srcSize.width > w; w++) {
					uint16_t dstW = w + x;
					if (dstW >= dstSize.width) continue;

					Vec4b pixel(0, 0, 0, 0);

					switch (src.channels())
					{
					case 4:
						pixel = src.at<cv::Vec4b>(h, w);
						break;
					default:
						break;
					}

					if (pixel[3] == 0) {
						continue;
					}

					switch (dst.channels())
					{
					case 4:
						dst.at<Vec4b>(dstH, dstW) = pixel;
						break;
					default:
						break;
					}
				}
			}
		};
	}
}