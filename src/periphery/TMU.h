/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef INTERMEDIATE_TMU_H
#define INTERMEDIATE_TMU_H

#include "../Bitfield.h"
#include "../Types.h"
#include "../Module.h"

namespace vc4c
{
	namespace periphery
	{
		const Value TMU_READ_REGISTER(REG_TMU_OUT, TYPE_UNKNOWN);
		const Value TMU_GENERAL_READ_ADDRESS(REG_TMU_ADDRESS, TYPE_INT32.toVectorType(16).toPointerType());
		const Value TMU_COORD_S_REGISTER(REG_TMU_COORD_S_U_X, TYPE_FLOAT.toVectorType(16));
		const Value TMU_COORD_T_REGISTER(REG_TMU_COORD_T_V_Y, TYPE_FLOAT.toVectorType(16));

		/*
		 * See Broadcom specification, table 18
		 */
		struct TextureType : public InstructionPart
		{
			std::string to_string() const;

			constexpr TextureType(unsigned char val, unsigned char bpp, unsigned char chans) : InstructionPart(val), bitsPerPixel(bpp), channels(chans) {};

			unsigned char bitsPerPixel;
			//the number of channels
			unsigned char channels;
		};

		//"8-bit per channel red, green, blue, alpha"
		constexpr TextureType RGBA8888{0, 32, 4};
		//"8-bit per channel RGA, alpha set to 1.0"
		constexpr TextureType RGBX8888{1, 32, 4};
		//"4-bit per channel red, green, blue, alpha"
		constexpr TextureType RGBA4444{2, 16, 4};
		//"5-bit per channel red, green, blue, 1-bit alpha"
		constexpr TextureType RGBA5551{3, 16, 4};
		//"Alpha channel set to 1.0"
		constexpr TextureType RGB565{4, 16, 4};
		//"8-bit luminance (alpha channel set to 1.0)"
		constexpr TextureType LUMINANCE{5, 8, 1};
		//"8-bit alpha (RGA channels set to 0)"
		constexpr TextureType ALPHA{6, 8, 1};
		//"8-bit luminance, 8-bit alpha"
		constexpr TextureType LUMALPHA{7, 16, 2};
		//"Ericsson Texture Compression format"
		constexpr TextureType ECT1{8, 4, 0};
		//"16-bit float sample (blending supported)"
		constexpr TextureType S16F{9, 16, 1};
		//"8-bit integer sample (blending supported)"
		constexpr TextureType S8{10, 8, 1};
		//"16-bit integer sample (point sampling only)"
		constexpr TextureType S16{11, 16, 1};
		//"1-bit black and white"
		constexpr TextureType BW1{12, 1, 1};
		//"4-bit alpha"
		constexpr TextureType A4{13, 4, 1};
		//"1-bit alpha"
		constexpr TextureType A1{14, 1, 1};
		//"16-bit float per RGBA channel"
		constexpr TextureType RGBA64{15, 64, 4};
		//"Raster format 8-bit per channel red, green, blue, alpha"
		constexpr TextureType RGBA32R{16, 32, 4};
		//"Raster format 8-bit per channel Y, U, Y, V"
		constexpr TextureType YUYV422R{17, 32, 4};

		/*
		 * See Broadcom specification, table 19
		 */
		enum class TextureFilter
		{
			//magnification: "Sample 2x2 pixels and blend. (bilinear)"
			//minification: "Bilinear sample from LOD 0 only"
			LINEAR = 0,
			//magnification: "Sample nearest pixel (point sample)"
			//minification: "Sample nearest pixel in LOD 0 only"
			NEAREST = 1,
			//minification: "Sample nearest pixel from nearest LOD level"
			NEAR_MIP_NEAR = 2,
			//minification: "Sample nearest pixel from nearest 2 LOD levels and blend"
			NEAR_MIP_LIN = 3,
			//minification: "Bilinear sample from nearest LOD level"
			LIN_MIP_NEAR = 4,
			//minification: "Blend Bilinear samples from 2 nearest LOD levels (trilinear)"
			LIN_MIP_LIN = 5
		};

		/*
		 * See Broadcom specification, table 16
		 */
		enum class WrapMode
		{
			REPEAT = 0,
			CLAMP = 1,
			MIRROR = 2,
			BORDER = 3
		};

		/*
		 * See Broadcom specification, table 17
		 */
		enum class ParameterType
		{
			//"Not Used (for example, for 2D textures + bias)"
			NONE = 0,
			//"Cube Map Stride"
			CUBE_MAP = 1,
			//"Child Image Dimensions"
			CHILD_DIMENSIONS = 2,
			//"Child Image Offsets"
			CHILD_OFFSETS = 3
		};

		//XXX these types are not really required here (but in VC4CL),
		//since we need the fields offsets, not access to them directly

		/*
		 * first UNIFORM-value read by the TMU to configure texture-reads
		 *
		 * See Broadcom specification, table 15
		 */
		class BasicTextureSetup : private Bitfield<uint32_t>
		{
		public:

			BasicTextureSetup(uint32_t texturePointer, TextureType type) : Bitfield(0)
			{
				setBasePointer(texturePointer);
				setType(type.value & 0xF);
			}

			/*
			 * "Texture Base Pointer (in multiples of 4Kbytes)."
			 */
			BITFIELD_ENTRY(BasePointer, uint32_t, 12, Int)
			/*
			 * "Cache Swizzle"
			 */
			BITFIELD_ENTRY(CacheSwizzle, uint8_t, 10, Tuple)
			/*
			 * "Cube Map Mode"
			 */
			BITFIELD_ENTRY(CubeMapMode, bool, 9, Bit)
			/*
			 * "Flip Texture Y Axis"
			 */
			BITFIELD_ENTRY(FlipYAxis, bool, 8, Bit)
			/*
			 * "Texture Data Type"
			 */
			BITFIELD_ENTRY(Type, uint8_t, 4, Quadruple)
			/*
			 * "Number of Mipmap Levels minus 1"
			 */
			BITFIELD_ENTRY(MipMapLevels, uint8_t, 0, Quadruple)
		};

		/*
		 * second UNIFORM-value read by the TMU to configure texture-reads
		 *
		 * See Broadcom specification, table 16
		 */
		class TextureAccessSetup : private Bitfield<uint32_t>
		{
		public:

			TextureAccessSetup(TextureType type, uint16_t width, uint16_t height) : Bitfield(0)
			{
				setTypeExtended(type.value >> 4);
				setHeight(height);
				setWidth(width);
			}

			/*
			 * "Texture Data Type Extended (bit 4 of texture type)"
			 */
			BITFIELD_ENTRY(TypeExtended, bool, 31, Bit)
			/*
			 * "Image Height (0 = 2048)"
			 */
			BITFIELD_ENTRY(Height, uint16_t, 20, Undecuple)
			/*
			 * "Flip ETC Y (per block)"
			 */
			BITFIELD_ENTRY(FlipETCYAxis, bool, 19, Bit)
			/*
			 * "Image Width (0 = 2048)"
			 */
			BITFIELD_ENTRY(Width, uint16_t, 8, Undecuple)
			/*
			 * "Magnification Filter"
			 */
			BITFIELD_ENTRY(MagnificationFilter, TextureFilter, 7, Bit)
			/*
			 * "Minification Filter"
			 */
			BITFIELD_ENTRY(MinificationFilter, TextureFilter, 4, Triple)
			/*
			 * "T Wrap Mode (0, 1, 2, 3 = repeat, clamp, mirror, border)"
			 */
			BITFIELD_ENTRY(WrapT, WrapMode, 2, Tuple)
			/*
			 * "S Wrap Mode (0, 1, 2, 3 = repeat, clamp, mirror, border)"
			 */
			BITFIELD_ENTRY(WrapS, WrapMode, 0, Tuple)
		};

		/*
		 * third UNIFORM-value read by the TMU to configure texture-reads. This value is only read for Cube map strides or child image parameters
		 *
		 * See Broadcom specification, table 17
		 */
		class ExtendedTextureSetup : private Bitfield<uint32_t>
		{
		public:

			ExtendedTextureSetup(uint32_t cubeMapStride, bool disableAutomaticLOD = false) : Bitfield(0)
			{
				setParameterType(ParameterType::CUBE_MAP);
				setCubeMapStride(cubeMapStride);
				setDisableAutomaticLOD(disableAutomaticLOD);
			}

			ExtendedTextureSetup(uint16_t childWidth, uint16_t childHeight) : Bitfield(0)
			{
				setParameterType(ParameterType::CHILD_DIMENSIONS);
				setChildHeight(childHeight);
				setChildWidth(childWidth);
			}

			ExtendedTextureSetup(uint32_t childOffsetX, uint32_t childOffsetY) : Bitfield(0)
			{
				setParameterType(ParameterType::CHILD_OFFSETS);
				setChildOffsetY(childOffsetY);
				setChildOffsetX(childOffsetX);
			}

			/*
			 * "Determines meaning of rest of parameter:
			 *   0 = Not Used (for example, for 2D textures + bias)
			 *   1 = Cube Map Stride
			 *   2 = Child Image Dimensions
			 *   3 = Child Image Offsets"
			 */
			BITFIELD_ENTRY(ParameterType, ParameterType, 30, Tuple)

			//Cube Map Stride

			/*
			 * "Cube Map Stride (in multiples of 4 Kbytes)"
			 */
			BITFIELD_ENTRY(CubeMapStride, uint32_t, 12, Int)
			/*
			 * "Disable automatic LOD, use bias only"
			 */
			BITFIELD_ENTRY(DisableAutomaticLOD, bool, 0, Bit)

			//Child Image Dimensions

			/*
			 * Child height
			 */
			BITFIELD_ENTRY(ChildHeight, uint16_t, 12, Undecuple)
			/*
			 * Child width
			 */
			BITFIELD_ENTRY(ChildWidth, uint16_t, 0, Undecuple)

			//Child Image Offsets

			/*
			 * "Child Image Y Offset"
			 */
			BITFIELD_ENTRY(ChildOffsetY, uint16_t, 12, Undecuple)
			/*
			 * "Child Image X Offset"
			 */
			BITFIELD_ENTRY(ChildOffsetX, uint16_t, 0, Undecuple)
		};

		/*
		 * Perform a general 32-bit memory lookup via the TMU.
		 *
		 * Actually, 16 separate 32-bit memory loads are performed for the 16 elements of the address-vector.
		 */
		InstructionWalker insertGeneralReadTMU(InstructionWalker it, const Value& dest, const Value& addr);

		/*
		 * Inserts a read via TMU from the given image-parameter at the coordinates x, y (y optional), which need to be converted to [0, 1] prior to this call
		 * and stores the result in dest.
		 */
		InstructionWalker insertReadTMU(Method& method, InstructionWalker it, const Value& image, const Value& dest, const Value& xCoord, const Optional<Value>& yCoord = NO_VALUE);

	} /* namespace periphery */
} /* namespace vc4c */

#endif /* INTERMEDIATE_TMU_H */
