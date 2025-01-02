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

		cv::Mat& Generator::get_atlas(size_t atlas)
		{
			return m_atlases[atlas];
		}

		bool Generator::validate_image(const cv::Mat& image)
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

			for (const Item& item : m_items)
			{
				libnest2d::Item& packer_item = packer_items.emplace_back(
					std::vector<libnest2d::Point>(item.vertices.size() + 1)
				);

				for (uint16_t i = 0; packer_item.vertexCount() > i; i++) {
					if (i == item.vertices.size()) { // End point for libnest2d
						packer_item.setVertex(i, { item.vertices[0].uv.x, item.vertices[0].uv.y });
					}
					else {
						packer_item.setVertex(i, { item.vertices[i].uv.x, item.vertices[i].uv.y });
					}
				}
			}

			libnest2d::NestConfig<libnest2d::NfpPlacer, libnest2d::FirstFitSelection> cfg;
			cfg.placer_config.alignment = libnest2d::NestConfig<>::Placement::Alignment::DONT_ALIGN;
			cfg.placer_config.starting_point = libnest2d::NestConfig<>::Placement::Alignment::BOTTOM_LEFT;
			cfg.placer_config.parallel = m_config.parallel();
#if WK_DEBUG
			cfg.placer_config.accuracy = 0.0;
#else
			cfg.placer_config.accuracy = 0.6;
#endif
			cfg.selector_config.verify_items = false;
			//cfg.selector_config.texture_parallel_hard = m_config.parallel();

			libnest2d::NestControl control;
			if (m_config.progress)
			{
				control.progressfn = [&](unsigned)
					{
						m_config.progress(m_duplicate_item_counter + m_item_counter++);
					};
			}

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

				//auto& shape = item.transformedShape();
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
				Item& item = m_items[i];

				auto rotation = packer_item.rotation();
				double rotation_degree = -(rotation.toDegrees());

				auto shape = packer_item.transformedShape();
				auto box = packer_item.boundingBox();

				// Item Data
				item.texture_index = bin_offset + packer_item.binId();
				item.transform.rotation = rotation;
				item.transform.translation.x = (int32_t)libnest2d::getX(packer_item.translation()) - m_config.extrude();
				item.transform.translation.y = (int32_t)libnest2d::getY(packer_item.translation()) - m_config.extrude();

				cv::Mat& image = item.image_ref();

				// Image Transform
				if (rotation_degree != 0) {
					cv::Point2f center((float)((item.width() - 1) / 2.0), (float)((item.height() - 1) / 2.0));
					cv::Mat rot = cv::getRotationMatrix2D(center, rotation_degree, 1.0);
					cv::Rect2f bbox = cv::RotatedRect(cv::Point2f(), item.image().size(), (float)rotation_degree).boundingRect2f();

					rot.at<double>(0, 2) += bbox.width / 2.0 - item.width() / 2.0;
					rot.at<double>(1, 2) += bbox.height / 2.0 - item.height() / 2.0;

					cv::warpAffine(image, image, rot, bbox.size(), cv::INTER_NEAREST);
				}

				cv::copyMakeBorder(image, image, m_config.extrude(), m_config.extrude(), m_config.extrude(), m_config.extrude(), cv::BORDER_REPLICATE);

				auto index = item.texture_index;
				auto x = (int)libnest2d::getX(box.minCorner()) - m_config.extrude() * 2;
				auto y = (int)libnest2d::getY(box.minCorner()) - m_config.extrude() * 2;

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