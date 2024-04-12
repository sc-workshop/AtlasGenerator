#include "AtlasGenerator/Item/SlicedItem.h"

namespace sc
{
	namespace AtlasGenerator
	{
		void SlicedItem::get_slice(Area area, Rect<int32_t>& guide, Rect<int32_t>& xy, Rect<uint16_t>& uv, Transformation xy_transform)
		{
			// TODO: Move to seperate builder class?
			Rect<int32_t> xy_bound = bound();

			Point<int32_t> xy_bottom_left_corner(xy_bound.left, xy_bound.bottom);
			xy_transform.transform_point(xy_bottom_left_corner);

			Point<int32_t> xy_top_right_corner(xy_bound.right, xy_bound.top);
			xy_transform.transform_point(xy_top_right_corner);

			Rect<int32_t> xy_rectangle(
				xy_bottom_left_corner.x, xy_bottom_left_corner.y,
				xy_top_right_corner.x, xy_top_right_corner.y
			);

			Rect<uint16_t> uv_rectangle(
				vertices[0].uv.u, vertices[0].uv.v,
				vertices[2].uv.u, vertices[2].uv.v
			);

			uint16_t left_width_size = abs(guide.left - xy_rectangle.x);
			uint16_t top_height_size = abs(guide.top - xy_rectangle.height);
			uint16_t bottom_height_size = abs(guide.bottom - xy_rectangle.y);
			uint16_t middle_width_size = abs(guide.right - xy_rectangle.x) - left_width_size;
			uint16_t middle_height_size = abs(guide.top - (xy_rectangle.y + bottom_height_size));
			uint16_t right_width_size = abs(guide.right - xy_rectangle.width);

			switch (area)
			{
			case Area::BottomLeft:
				if (guide.left < xy_rectangle.x || guide.bottom < xy_rectangle.y) return;

				{
					xy.x = xy_rectangle.x;
					xy.y = xy_rectangle.y;

					xy.width = left_width_size;
					xy.height = bottom_height_size;
				}

				{
					uv.x = uv_rectangle.x;
					uv.y = uv_rectangle.y;

					uv.width = xy.width;
					uv.height = xy.height;
				}
				break;
			case Area::BottomMiddle:
			{
				xy.x = xy_rectangle.x + left_width_size;
				xy.y = xy_rectangle.y;

				xy.width = middle_width_size;
				xy.height = bottom_height_size;
			}

			{
				uv.x = uv_rectangle.x + left_width_size;
				uv.y = uv_rectangle.y;

				uv.width = xy.width;
				uv.height = xy.height;
			}

			break;
			case Area::BottomRight:
			{
				xy.x = guide.right;
				xy.y = xy_rectangle.y;

				xy.width = right_width_size;
				xy.height = bottom_height_size;
			}

			{
				uv.x = uv_rectangle.x + left_width_size + middle_width_size;
				uv.y = uv_rectangle.y;

				uv.width = xy.width;
				uv.height = xy.height;
			}
			break;
			case Area::MiddleLeft:
			{
				xy.x = xy_rectangle.x;
				xy.y = xy_rectangle.y + bottom_height_size;

				xy.width = left_width_size;
				xy.height = middle_height_size;
			}

			{
				uv.x = uv_rectangle.x;
				uv.y = uv_rectangle.y + bottom_height_size;

				uv.width = xy.width;
				uv.height = xy.height;
			}
			break;
			case Area::Center:
			{
				xy.x = xy_rectangle.x + left_width_size;
				xy.y = xy_rectangle.y + bottom_height_size;

				xy.width = middle_width_size;
				xy.height = middle_height_size;
			}

			{
				uv.x = uv_rectangle.x + left_width_size;
				uv.y = uv_rectangle.y + bottom_height_size;

				uv.width = xy.width;
				uv.height = xy.height;
			}
			break;
			case Area::MiddleRight:
			{
				xy.x = guide.right;
				xy.y = xy_rectangle.y + bottom_height_size;

				xy.width = right_width_size;
				xy.height = middle_height_size;
			}

			{
				uv.x = uv_rectangle.x + left_width_size + middle_width_size;
				uv.y = uv_rectangle.y + bottom_height_size;

				uv.width = xy.width;
				uv.height = xy.height;
			}
			break;
			case Area::TopLeft:
			{
				xy.x = xy_rectangle.x;
				xy.y = xy_rectangle.y + bottom_height_size + middle_height_size;

				xy.width = left_width_size;
				xy.height = top_height_size;
			}

			{
				uv.x = uv_rectangle.x;
				uv.y = uv_rectangle.y + bottom_height_size + middle_height_size;

				uv.width = xy.width;
				uv.height = xy.height;
			}
			break;
			case Area::TopMiddle:
			{
				xy.x = xy_rectangle.x + left_width_size;
				xy.y = xy_rectangle.y + bottom_height_size + middle_height_size;

				xy.width = middle_width_size;
				xy.height = top_height_size;
			}

			{
				uv.x = uv_rectangle.x + left_width_size;
				uv.y = uv_rectangle.y + bottom_height_size + middle_height_size;

				uv.width = xy.width;
				uv.height = xy.height;
			}
			break;
			case Area::TopRight:
			{
				xy.x = guide.right;
				xy.y = xy_rectangle.y + bottom_height_size + middle_height_size;

				xy.width = right_width_size;
				xy.height = top_height_size;
			}

			{
				uv.x = uv_rectangle.x + left_width_size + middle_width_size;
				uv.y = uv_rectangle.y + bottom_height_size + middle_height_size;

				uv.width = xy.width;
				uv.height = xy.height;
			}
			break;
			default:
				break;
			}
		}
	}
}