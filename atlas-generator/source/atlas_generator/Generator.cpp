#include "Generator.h"
#include "limits.h"

#include <libnest2d/libnest2d.hpp>

namespace wk
{
	namespace AtlasGenerator
	{
		Generator::Generator(const Config& config) : m_config(config)
		{
		}

		size_t Generator::generate(Container<Item>& items)
		{
			if (items.empty()) return 0;

			std::map<int, size_t, std::greater<int>> texture_variants;
			for (size_t i = 0; items.size() > i; i++)
			{
				Item& item = items[i];

				if (!Generator::validate_image(item.image()))
				{
					throw PackagingException(PackagingException::Reason::UnsupportedImage, i);
				}

				texture_variants[item.image().type()]++;
			}

			if (texture_variants.size() == 1)
			{
				int type = items[0].image().type();

				// iterate just by items vector
				auto it = ItemIterator<size_t>(0, items.size());
				return generate(items, it, type);
			}

			size_t bin_count = 0;
			m_item_counter = 0;
			for (auto it = texture_variants.begin(); it != texture_variants.end(); ++it)
			{
				int type = it->first;
				Container<size_t> item_indices;
				item_indices.reserve(it->second);

				for (size_t i = 0; items.size() > i; i++)
				{
					Item& item = items[i];
					if (item.image().type() == type)
					{
						item_indices.push_back(i);
					}
				}

				m_items.reserve(item_indices.size());
				m_duplicate_indices.reserve(item_indices.size());

				// iterate only by current type items
				auto item_it = ItemIterator<size_t>(0, item_indices.size(), item_indices);
				bin_count += generate(items, item_it, type);
			}
			
			return bin_count;
		}

		size_t Generator::generate(Container<Item>& items, ItemIterator<size_t>& item_iterator, int type)
		{
			Container<size_t> inverse_duplicate_indices;
			inverse_duplicate_indices.reserve(items.size() / 2);

			for (auto it = item_iterator.begin(); it != item_iterator.end(); ++it)
			{
				const size_t i = *it;
				Item& item = items[i];

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
			
			size_t current_atlas_count = m_atlases.size();
			if (!pack_items(type))
			{
				throw PackagingException(PackagingException::Reason::Unknown);
			};
		
			for (size_t i = 0; m_items.size() > i; i++)
			{
				size_t item_index = m_duplicate_indices[i];
		
				if (item_index != SIZE_MAX)
				{
					items[i] = items[item_index];
				}
			}
		
			m_duplicate_indices.clear();
			m_items.clear();
		
			return m_atlases.size() - current_atlas_count;
		}

		cv::Mat& Generator::get_atlas(uint8_t atlas)
		{
			return m_atlases[atlas];
		}

		bool Generator::validate_image(cv::Mat& image)
		{
			if (1 > image.rows || 1 > image.cols)
			{
				return false;
			}

			int type = image.type();
			if (type != CV_8UC1 && type != CV_8UC2 && type != CV_8UC3 && type != CV_8UC4)
			{
				return false;
			}

			return true;
		}

		bool Generator::pack_items(int atlas_type)
		{
			// Vector with polygons for libnest2d
			std::vector<libnest2d::Item> packer_items;
			packer_items.reserve(m_items.size());

			for (const Item* item : m_items)
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

			libnest2d::NestConfig<libnest2d::NfpPlacer, libnest2d::FirstFitSelection> cfg;
			cfg.placer_config.alignment = libnest2d::NestConfig<>::Placement::Alignment::DONT_ALIGN;
			cfg.placer_config.starting_point = libnest2d::NestConfig<>::Placement::Alignment::BOTTOM_LEFT;

			libnest2d::NestControl control;
			control.progressfn = [&](unsigned)
				{
					m_config.progress(m_item_counter++);
				};

			size_t bin_offset = m_atlases.size();
			size_t bin_count = nest(
				packer_items,
				libnest2d::Box(
					m_config.width(), m_config.height(),
					{ (int)ceil(m_config.width() / 2), (int)ceil(m_config.height() / 2) }),
				m_config.extrude() * 2, cfg, control
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
				m_atlases.emplace_back(
					size.width,
					size.height,
					atlas_type,
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

				auto& image = item.image();
				auto index = bin_offset + item.texture_index;
				auto x = static_cast<uint16_t>(libnest2d::getX(box.minCorner()) - m_config.extrude() * 2);
				auto y = static_cast<uint16_t>(libnest2d::getY(box.minCorner()) - m_config.extrude() * 2);

				switch (atlas_type)
				{
				case CV_8UC1:
					place_image_to<cv::Vec<uchar, 1>, false>(
						image,
						index,
						x,
						y
					);
					break;
				case CV_8UC2:
					place_image_to<cv::Vec2b, true, 1>(
						image,
						index,
						x,
						y
					);
					break;
				case CV_8UC3:
					place_image_to<cv::Vec3b, false>(
						image,
						index,
						x,
						y
					);
					break;
				case CV_8UC4:
					place_image_to<cv::Vec4b, true, 3>(
						image,
						index,
						x,
						y
					);
					break;
				default:
					break;
				}
				
			}

			return true;
		}
	}
}