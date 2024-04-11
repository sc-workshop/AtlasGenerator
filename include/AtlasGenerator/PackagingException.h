#pragma once

#include "exception/GeneralRuntimeException.h"

namespace sc
{
	namespace AtlasGenerator
	{
		class PackagingException : GeneralRuntimeException
		{
		public:
			enum class Reason
			{
				Unknown = 0,
				TooBigImage,
				UnsupportedImage,
				InvalidPolygon
			};
		public:
			PackagingException(Reason reason, size_t item_index = SIZE_MAX)
				: GeneralRuntimeException("PackagingException"), m_item_index(item_index), m_reason(reason)
			{
				switch (reason)
				{
				case Reason::TooBigImage:
					m_message = "Image is too big for packaging";
					break;
				case Reason::UnsupportedImage:
					m_message = "Image has unsupported type";
					break;
				case Reason::InvalidPolygon:
					m_message = "Failed to generate polygon for image";
					break;
				default:
					m_message = "Unknown exception";
					break;
				}
			}
		public:
			Reason reason()
			{
				return m_reason;
			};

			size_t index()
			{
				return m_item_index;
			};
		private:
			Reason m_reason;
			size_t m_item_index;
		};
	}
}